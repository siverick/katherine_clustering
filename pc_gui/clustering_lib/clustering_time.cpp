
/**
 * @clustering_time.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "clustering_time.h"
#include <algorithm>

#define _CRT_SECURE_NO_WARNINGS

void clustering_time_parallelisation::do_clustering(std::string& lines, const ClusteringParams& new_params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;
	stat_reset();
	log_clear();

	params = new_params;
	this->abort = abort;

	// Get num of hits. If not obtained, use approx number from lines number
	size_t hits_num = GetNumOfHits(lines);	// Num of hits
	if (hits_num == -1) hits_num = params.no_lines;	// error when reading hits, use approx no_lines

	/* Get number of threads according to used platform */
	numOfThreads = std::thread::hardware_concurrency();
	numOfThreads = std::min(numOfThreads, UINT16_MAX);	
	if (numOfThreads == 0) numOfThreads = 1;
	log_append(utility::print_time_info("Threads", "t", numOfThreads));

	std::vector<std::string> inputs_for_threads;
	SeparateFileST(lines, inputs_for_threads);		// Separate file into small chunks - destructive for "lines"

	ContinualTimer timer;
	timer.Start();

	std::vector<std::thread> workers;
	auto t_clustering = &clustering_time_parallelisation::ProcessDataQueue;
	
	for (uint16_t i = 0; i < (inputs_for_threads.size() - 1); i++)
	{
		/* For new merge stations in count (pixelForThreads.size() - 1) */
		clustersToMerge.emplace_back();
		pixelsToMerge.emplace_back();
	}

	uint16_t t_num = 0;	// Num of thread sent to worker
	for (auto& qu : inputs_for_threads)	/* Create threads and run clustering */
	{	
		workers.push_back(std::thread(t_clustering, this, t_num, std::ref(qu), params));
		//log_append(log_append(utility::print_time_info("t_num", "t", t_num)));
		t_num++;

		if (t_num > numOfThreads)
			break;
	}

	for (auto& wo : workers)			/* Wait for every thread to finish */
	{
		wo.join();
	}
	workers.clear();
	workers.shrink_to_fit();

	auto t_merge = &clustering_time_parallelisation::MergeMiddleClusters;

	for (uint16_t merge_station = 0; merge_station < (inputs_for_threads.size() - 1); merge_station++)
	{
		workers.push_back(std::thread(t_merge, this, std::ref(pixelsToMerge[merge_station]), std::ref(clustersToMerge[merge_station])));
	}

	for (auto& wo : workers)
	{
		wo.join();
	}
	workers.clear();
	workers.shrink_to_fit();

	timer.Stop();

	/* Clear input data and other clutter */
	inputs_for_threads.clear();
	inputs_for_threads.shrink_to_fit();
	clustersToMerge.clear();
	clustersToMerge.shrink_to_fit();
	pixelsToMerge.clear();
	pixelsToMerge.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	stat_save(timer.ElapsedMs(), doneClusters.size());
	return;
}

void clustering_time_parallelisation::ProcessDataQueue(uint16_t thread_num, std::string& thread_lines, const ClusteringParams t_params)
{
	std::string oneLine;
	char* rows[4] = { 0, 0, 0, 0 };		// Temp storage for txt numerals
	bool delim = t_params.rn_delim;		// Type of delimiter
	std::queue<OnePixel> pixelData;		// FIFO for pixelData
	
	// PARAMETERS
	int upFilter = 255 - t_params.outerFilterSize;
	int doFilter = t_params.outerFilterSize;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;

	// Positions in file "thread_lines" - must be becuase of threads - they cant share static variables in function
	size_t last_pos = 0;
	size_t pos = 0;

	while (m_get_my_line_MT(thread_lines, oneLine, delim, last_pos, pos)) {      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;

		if (abort)
		{
			return;
		}

		stat_lines_sorted++;

		/* Split line into rows */
		rows[0] = strtok(strdup(oneLine.data()), "\t");
		rows[1] = strtok(NULL, "\t");
		rows[2] = strtok(NULL, "\t");
		rows[3] = strtok(NULL, "\t");

		// ATOI 6300 ms vs STRTOINT 6100 ms with 20 PIX filter
		// ATOI CCA 200 ms slower than my Fast strtoint
		int coordX = strtoint(rows[0]) / 256; // std::atoi(rows[0]) / 256;
		int coordY = strtoint(rows[0]) % 256; // std::atoi(rows[0]) % 256;
		double toaAbsTime = (double)((strtolong(rows[1]) * toaLsb) - (strtolong(rows[2]) * toaFineLsb));  // (ns) TimeFromBeginning = 25 * ToA - 1.5625 * fineToA
		int ToTValue = strtoint(rows[3]);//std::atoi(rows[3]);
		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue;   // This is faster after the inlined funkctions, not after coordY

		if (t_params.calibReady)
		{
			ToTValue = static_cast<int>(energy_calc(coordX, coordY, ToTValue));
		}

		// Push new Pixel to most recent queue
		pixelData.push(std::move(OnePixel(coordX, coordY, ToTValue, toaAbsTime)));
		stat_lines_processed++;
	}

	/* The idea is I will not use "lock" on shared Cluster variables, but I will create unique variables for
	each thread and then at the end I will insert those variables to doneClusters and they get deleted after */
	Clusters open_clusters_back;
	std::queue<OnePixel> open_pixels_front;
	Clusters thread_done_clusters;
	thread_data threadData = {0,0};
	threadData.thread_num = thread_num;	// indexed from 0
	threadData.firstToA = static_cast<uint64_t>(pixelData.front().ToA);	// We derive clusters for merging from this

	/* Process all pixel data like FIFO */
	while (!pixelData.empty())
	{
		ProcessPixel(pixelData.front(), open_clusters_back, thread_done_clusters, open_pixels_front, threadData);
		pixelData.pop();
	}

	if (numOfThreads > 1)	// Only in case of multiple threads
	{	/* Use scoped mutex for exception safety - no deadlock */
		std::lock_guard<std::mutex> lock(mtx);
		doneClusters.insert(doneClusters.end(), thread_done_clusters.begin(), thread_done_clusters.end());
		SendToMergeStation(open_pixels_front, open_clusters_back, thread_num);
	}
	else    // Dont merge when only 1 thread
	{
		std::lock_guard<std::mutex> lock(mtx);
		doneClusters.insert(doneClusters.end(), open_clusters_back.begin(), open_clusters_back.end());
		doneClusters.insert(doneClusters.end(), thread_done_clusters.begin(), thread_done_clusters.end());
	}
}

void clustering_time_parallelisation::ProcessPixel(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters)
{
	ProcessPixelAlgorithm(pixel_data, open_clusters, thread_done_clusters);
}

void clustering_time_parallelisation::ProcessPixel(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters, std::queue<OnePixel>& front_merge_pixels, const thread_data& threadData)
{
	// Dont do for first thread (0.)
	if (threadData.thread_num > 0)	
	{
		// Dont process first pixels, just add to another PixelData to be processed in merge
		if ((pixel_data.ToA - threadData.firstToA) < params.maxClusterDelay)
		{
			front_merge_pixels.emplace(pixel_data);
			return;
		}
	}

	ProcessPixelAlgorithm(pixel_data, open_clusters, thread_done_clusters);
}

void clustering_time_parallelisation::ProcessPixelAlgorithm(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters)
{
	// Loop variables
	bool prevAdded = false;
	bool rel, relX, relY = false;
	int lastAddCluster = 0;

	for (auto clstr = open_clusters.begin(); clstr != open_clusters.end(); clstr++) {

		if ((pixel_data.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
		{
			if (clstr - open_clusters.begin() > 1)
			{
				thread_done_clusters.emplace_back(*clstr);
				clstr = open_clusters.erase(clstr);
				clstr--;
			}

			continue;
		}

		/* ToA range check */
		// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
		if ((pixel_data.ToA - clstr->minToA) > params.maxClusterSpan || (pixel_data.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
			continue;
		}

		/* Decide if its worth to go through this cluster */
		/* IS TOO FAR UNDER || IS TOO FAR UP */
		if (pixel_data.y < (clstr->yMin - 1) || pixel_data.y >(clstr->yMax + 1)) {
			continue;
		}
		if (pixel_data.x < (clstr->xMin - 1) || pixel_data.x >(clstr->xMax + 1)) {
			continue;
		}

		for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
			//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

			relX = ((pixel_data.x) == (pixs.x + 1)) || ((pixel_data.x) == (pixs.x - 1)) || ((pixel_data.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
			relY = ((pixel_data.y) == (pixs.y + 1)) || ((pixel_data.y) == (pixs.y - 1)) || ((pixel_data.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
			rel = relX && relY;       // Is related in both X AND Y

			if (rel)    // If Neighbor, add to cluster
			{
				if (prevAdded)  // Join clusters
				{
					// Join current Cluster into LastAddedTo cluster and Erase the current one
					open_clusters[lastAddCluster].pix.insert(open_clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

					/* Join min max values of clusters */
					if (clstr->xMax > open_clusters[lastAddCluster].xMax) open_clusters[lastAddCluster].xMax = clstr->xMax;
					if (clstr->xMin < open_clusters[lastAddCluster].xMin) open_clusters[lastAddCluster].xMin = clstr->xMin;
					if (clstr->yMax > open_clusters[lastAddCluster].yMax) open_clusters[lastAddCluster].yMax = clstr->yMax;
					if (clstr->yMin < open_clusters[lastAddCluster].yMin) open_clusters[lastAddCluster].yMin = clstr->yMin;
					if (clstr->minToA < open_clusters[lastAddCluster].minToA) open_clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
					if (clstr->maxToA > open_clusters[lastAddCluster].maxToA) open_clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

					clstr = open_clusters.erase(clstr);       // Erase the current Cluster
					clstr--;
				}
				else            // Simply Add Pixel
				{
					/* Update Min Max coord values */
					if (pixel_data.x > clstr->xMax) clstr->xMax = pixel_data.x;
					else if (pixel_data.x < clstr->xMin) clstr->xMin = pixel_data.x;
					if (pixel_data.y > clstr->yMax) clstr->yMax = pixel_data.y;
					else if (pixel_data.y < clstr->yMin) clstr->yMin = pixel_data.y;
					if (clstr->minToA > pixel_data.ToA) clstr->minToA = pixel_data.ToA; // Save ToA min
					if (clstr->maxToA < pixel_data.ToA) clstr->maxToA = pixel_data.ToA; // Save ToA max

					OnePixel newPixel = OnePixel{ pixel_data.x, pixel_data.y, pixel_data.ToT, pixel_data.ToA };
					clstr->pix.emplace_back(std::move(newPixel));
					lastAddCluster = clstr - open_clusters.begin();
					prevAdded = true;
				}

				break;  // Pixel was added to cluster => try NEXT cluster
			}
		}
	}

	if (prevAdded == false)  // Pixel doesnt match to any Cluster - Place new cluster
	{
		ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)pixel_data.x, (uint16_t)pixel_data.y, pixel_data.ToT, pixel_data.ToA} },
			pixel_data.ToA, pixel_data.ToA, (uint16_t)pixel_data.x, (uint16_t)pixel_data.x, (uint16_t)pixel_data.y, (uint16_t)pixel_data.y };
		open_clusters.emplace_back(std::move(cluster));    // Add new cluster
	}
}

void clustering_time_parallelisation::SendToMergeStation(std::queue<OnePixel>& open_pixels_front, Clusters& open_clusters_back, uint16_t thread_num)
{
	/*            Scheme for merge
			   ______	______	 ______
			   | T0 |	| T1 |	 | T2 |
			   ______	______	 ______
			 ||      ||       ||       ||
			Done   ______	______    Done
				   | M0 |	| M1 |
				   ______	______
		*/

		/* Place to appropriate merge stations! */
	if (thread_num == 0)	// First thread
	{
		clustersToMerge[thread_num].insert(clustersToMerge[thread_num].end(), open_clusters_back.begin(), open_clusters_back.end());
	}
	else if (thread_num == (numOfThreads - 1))	// Last thread
	{
		doneClusters.insert(doneClusters.end(), open_clusters_back.begin(), open_clusters_back.end());
		pixelsToMerge[thread_num - 1] = open_pixels_front;
	}
	else   // Threads in between
	{
		clustersToMerge[thread_num].insert(clustersToMerge[thread_num].end(), open_clusters_back.begin(), open_clusters_back.end());
		pixelsToMerge[thread_num - 1] = open_pixels_front;
	}
}

void clustering_time_parallelisation::MergeMiddleClusters(std::queue<OnePixel>& pixelData, Clusters& openClusters)
{
	Clusters thread_done_clusters;

	// Do basic clustering on clusters to merge (openClusters) with pixels to merge (pixelData)
	while (!pixelData.empty())
	{
		ProcessPixel(pixelData.front(), openClusters, thread_done_clusters);
		pixelData.pop();
	}

	{	/* Use scoped mutex for exception safety - no deadlock */
		std::lock_guard<std::mutex> lock(mtx);
		doneClusters.insert(doneClusters.end(), thread_done_clusters.begin(), thread_done_clusters.end());
		doneClusters.insert(doneClusters.end(), openClusters.begin(), openClusters.end());
	}
}

void clustering_time_parallelisation::AfterRunClstrCheck()
{
	for (const auto& clstr : doneClusters) {
		for (const auto& pixs : clstr.pix) {
			stat_lines_saved++;
			if (pixs.x > clstr.xMax || pixs.x < clstr.xMin || pixs.y > clstr.yMax || pixs.y < clstr.yMin)
			{
				//qDebug() << "Min Max not correct: PIX_X, PIX_Y" << pixs.x << " " << pixs.y <<
					//", Xmax = " << clstr.xMax << ", Xmin = " << clstr.xMin << ", Ymax = " << clstr.yMax << ", Ymin = " << clstr.yMin;
			}
		}
	}
}