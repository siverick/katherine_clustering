
/**
 * @clustering_time_embed.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "clustering_time_embed.h"
#include <cassert>

void clustering_time_embed::do_clustering(std::string& lines, const ClusteringParams& new_params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;
	stat_reset();
	log_clear();

	params = new_params;
	this->abort = abort;

	// Set global variables to default
	signal_run = true;
	signal_separator_finished = false;
	signal_end_frame_sent = false;
	signal_clusterers_finished = false;

	/* Get number of threads according to used platform */
	numOfThreads = std::thread::hardware_concurrency();
	numOfThreads = std::min(numOfThreads, UINT16_MAX);
	if (numOfThreads == 0) numOfThreads = 1;
	numOfThreads = 10;
	log_append(utility::print_time_info("Threads", "t", numOfThreads));

	for (uint16_t t_num = 0; t_num < numOfThreads; t_num++)
	{
		/* For new merge stations in count exactly NumOfThreads - because we have one extra merge station between 1st and last */
		clustersToMerge.emplace_back();
		pixelsToMerge.emplace_back();
		sendingMtx.lock();
		continualThreadInputs.emplace_back();	// Create inputs
		clustering_threads_finished.emplace_back(false);
		sendingMtx.unlock();
	}

	/* Start data passing thread */
	auto t_passLine = &clustering_time_embed::PassLineToSeparator;
	auto t_passer = std::thread(t_passLine, this, std::ref(lines));

	/* Start thread data separator */
	auto t_dataSeparator = &clustering_time_embed::SeparateFileIntoThreadsCont;
	auto t_separator = std::thread(t_dataSeparator, this);

	ContinualTimer timer;
	timer.Start();

	std::vector<std::thread> workers;
	auto t_clustering = &clustering_time_embed::ProcessDispatchStation;
	for (uint16_t t_num = 0; t_num < numOfThreads; t_num++)	/* Create threads and run clustering */
	{
		workers.push_back(std::thread(t_clustering, this, t_num, params));
		//log_append(utility::print_time_info("t_num", "t", t_num));
	}

	//log_append(utility::print_time_info("Elapsed to workers began working", "ms", timer.Stop() * 0.001));
	//timer.Start();

	/* Wait for input passer and separator to finish feeding data */
	t_passer.join();
	t_separator.join();

	for (auto& wo : workers)	/* Wait for every thread to finish */
	{
		wo.join();
	}
	workers.clear();
	workers.shrink_to_fit();
	/* Clear thread inputs after workers finish clustering */
	continualThreadInputs.clear();
	continualThreadInputs.shrink_to_fit();

	//log_append(utility::print_time_info("Elapsed to workers finished clustering", "ms", myTim.Stop() * 0.001));
	//timer.Start();

	size_t elems = 0;
	for (auto& station : pixelsToMerge)
	{
		elems += station.size();
	}

	for (auto& clusterFrame : clustersToMerge)
	{
		for (auto& cluster : clusterFrame.front())
		{
			for (auto& elem : cluster.pix)
			{
				elems++;
			}
		}
	}

	//log_append(utility::print_time_info("Hits waiting for merge:", "Hits", elems));

	// TODO: Uberu pass mergu odsud, merge tady jenom rozjedu a potom tam budu hazet data zase pres nejakej Queue
	auto t_merge = &clustering_time_embed::MergeMiddleClusters;
	for (uint16_t merge_station = 0; merge_station < numOfThreads; merge_station++)
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

	auto measTime = doneClusters.back().maxToA - doneClusters.front().minToA;
	log_append(utility::print_time_info("Measurement time was: ", "s", measTime / 1000000000.0));

	/* Clear input data and other shared variables */
	clustersToMerge.clear();
	clustersToMerge.shrink_to_fit();
	pixelsToMerge.clear();
	pixelsToMerge.shrink_to_fit();
	clustering_threads_finished.clear();
	clustering_threads_finished.shrink_to_fit();
	continualThreadInputs.clear();
	continualThreadInputs.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	stat_save(timer.ElapsedMs(), doneClusters.size());
	return;
}

// quickly check if all threads finished
bool AllThreadsFinished(std::vector<bool> states)
{
	bool allFinished = true;	// Has to be true from start
	for (bool val : states)
		allFinished = allFinished && val;
	return allFinished;
}

void clustering_time_embed::ProcessDispatchStation(uint16_t thread_num, const ClusteringParams t_params)
{
	uint32_t cnt = 0;
	bool firstRun = true;

	while (!abort)
	{
		if ((signal_separator_finished) && continualThreadInputs[thread_num].empty())
		{
			break;
		}

		if (!continualThreadInputs[thread_num].empty())		// If data frame has been dropped, take it and process it
		{
			std::unique_lock<std::mutex> sendingLock(sendingMtx);	// Unique lock makes locking and unlocking in context possible
			// Process Data Queue
			if (firstRun && thread_num == 0)
			{
				firstRun = false;
				ProcessDataFrame(thread_num, continualThreadInputs[thread_num].front(), t_params, FrameNumber::first);
				continualThreadInputs[thread_num].pop();	// Pop the frame
			}
			else
			{
				ProcessDataFrame(thread_num, continualThreadInputs[thread_num].front(), t_params, FrameNumber::middle);
				continualThreadInputs[thread_num].pop();	// Pop the frame
			}
		}
		else
			cnt++;

		if (cnt > 10000)	// Wait a bit of no data are coming
		{
			cnt = 0;
			std::this_thread::sleep_for(std::chrono::microseconds(500));
		}

	}

	if (abort)
		return;

	// Check if other threads finished
	sendingMtx.lock();
	clustering_threads_finished[thread_num] = true;
	bool allDone = AllThreadsFinished(clustering_threads_finished);
	sendingMtx.unlock();
	if (allDone)
		signal_clusterers_finished = true;

	/* Wait for other threads to finish */
	while (signal_end_frame_sent == false)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	/* Process the last frame of the measurement */
	if (!continualThreadInputs[thread_num].empty())		// If data frame has been dropped, take it and process it
	{
		if (continualThreadInputs[thread_num].front().front()[0] == 'E')	// If string indicated the thread to be the last one
		{
			continualThreadInputs[thread_num].front().front().erase(0, 1);	// Erase the 'E' char
			ProcessDataFrame(thread_num, continualThreadInputs[thread_num].front(), t_params, FrameNumber::last);
			continualThreadInputs[thread_num].pop();	// Pop the frame
		}
		else     // This is a unexpected state, but better do it then not
		{
			ProcessDataFrame(thread_num, continualThreadInputs[thread_num].front(), t_params, FrameNumber::middle);
			continualThreadInputs[thread_num].pop();	// Pop the frame
		}
	}
}

void clustering_time_embed::ProcessDataFrame(uint16_t thread_num, std::queue<std::string>& thread_lines, const ClusteringParams t_params, FrameNumber frameNum)
{
	std::string oneLine;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };		// Temp storage for txt numerals
	std::queue<OnePixel> pixelData;		// FIFO for pixelData

	// PARAMETERS
	int upFilter = 255 - t_params.outerFilterSize;
	int doFilter = t_params.outerFilterSize;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;

	double minToa = 0;
	double maxToa = 0;

	bool firstLoop = true;
	thread_data threadData = { 0,0 };
	threadData.thread_num = thread_num;	// indexed from 0
	
	while (!thread_lines.empty()) {      // Gets lines without ending line chars - ex. "\n"
		oneLine = thread_lines.front();
		thread_lines.pop();

		if (oneLine[0] == '#') continue;

		if (abort)
		{
			return;
		}

		stat_lines_sorted++;

		/* Split line into rows */
		rows[0] = strtok_s(&oneLine.front(), "\t", &context); //strtok(oneLine.data(), "\t");
		rows[1] = strtok_s(NULL, "\t", &context);
		rows[2] = strtok_s(NULL, "\t", &context);
		rows[3] = strtok_s(NULL, "\t", &context);

		// ATOI 6300 ms vs STRTOINT 6100 ms with 20 PIX filter
		// ATOI CCA 200 ms slower than my Fast strtoint
		int coordX = strtoint(rows[0]) / 256; // std::atoi(rows[0]) / 256;
		int coordY = strtoint(rows[0]) % 256; // std::atoi(rows[0]) % 256;
		double toaAbsTime = (double)((strtolong(rows[1]) * toaLsb) - (strtolong(rows[2]) * toaFineLsb));  // (ns) TimeFromBeginning = 25 * ToA - 1.5625 * fineToA
		int ToTValue = strtoint(rows[3]);//std::atoi(rows[3]);

		// This will have to be accomodated in FPGA differently - send firstToa with data from CPU is better
		if (firstLoop)
		{
			firstLoop = false;
			threadData.firstToA = static_cast<uint64_t>(toaAbsTime);	// We derive clusters for merging from this
		}

		// Just correctnes debugging purposes
		maxToa = std::max(maxToa, toaAbsTime);
		minToa = std::min(minToa, toaAbsTime);
		
		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) 
			continue;   // This is faster after the inlined funkctions, not after coordY

		if (t_params.calibReady)
		{
			ToTValue = energy_calc(coordX, coordY, ToTValue);
		}

		// Push new Pixel to most recent queue
		pixelData.push(std::move(OnePixel(coordX, coordY, ToTValue, toaAbsTime)));
		stat_lines_processed++;
	}

	if (!pixelData.empty())   // Can happen if filter causes frame to have no pixelData saved
		assert(((maxToa - minToa) > (params.maxClusterDelay * 10)) && "Frame size is too small");

	/* The idea is I will not use "lock" on shared Cluster variables, but I will create unique variables for
	each thread and then at the end I will insert those variables to doneClusters and they get deleted after */
	Clusters open_clusters_back;
	Clusters open_clusters_front;
	std::queue<OnePixel> open_pixels_front;
	Clusters thread_done_clusters;

	/* Process all pixel data like FIFO */
	while (!pixelData.empty())
	{
		if (abort)
			return;

		ProcessPixelAlgorithm(pixelData.front(), open_clusters_back, thread_done_clusters, open_clusters_front, threadData);
		pixelData.pop();
	}

	if (frameNum != FrameNumber::first)	// Merge open front only if its not the first frame
	{
		// Dismember open clusters front pixels to open pixels
		for (auto& cluster : open_clusters_front)
		{
			for (auto& pixel : cluster.pix)
			{
				open_pixels_front.emplace(pixel);
			}
		}
		open_clusters_front.clear();
		open_clusters_front.shrink_to_fit();
	}

	/* Merge for all numOfThread the same - differentiation in SendToMergeStation */
	{	/* Use scoped mutex for exception safety - no deadlock */
		std::lock_guard<std::mutex> lock(mtx);
		doneClusters.insert(doneClusters.end(), thread_done_clusters.begin(), thread_done_clusters.end());
		doneClusters.insert(doneClusters.end(), open_clusters_front.begin(), open_clusters_front.end());	// this will happen only if its first frame!
		SendToMergeStation(open_pixels_front, open_clusters_back, thread_num, frameNum);
	}
}

void clustering_time_embed::ProcessPixelAlgorithm(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters, Clusters& open_clusters_front, const thread_data& thread_first_toa)
{
	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;
	auto clstr = open_clusters.begin();

	while (clstr != open_clusters.end()) {

		// NOTE: This is working properly, tested!
		if ((pixel_data.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
		{
			/* Move to open clusters if the cluster is in toa delay - for merge */
			if (IsFrontMergeCluster(clstr->minToA, thread_first_toa))
			{
				open_clusters_front.emplace_back(*clstr);
			}
			else    /* If not for merge, cluster to done clusters */
			{
				assert((clstr->minToA - thread_first_toa.firstToA) > 0);	// Assert that no "merge" clusters are coming here
				thread_done_clusters.emplace_back(*clstr);
			}

			clstr = open_clusters.erase(clstr);
			continue;
		}

		/* ToA range check */
		// NOTE: m_abs() or double-if makes maxClusterSpan to be double sided, downwards and upwards
		if ((pixel_data.ToA - clstr->minToA) > params.maxClusterSpan || (pixel_data.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
			clstr++;
			continue;
		}

		/* Decide if its worth to go through this cluster */
		/* IS TOO FAR UNDER || IS TOO FAR UP */
		if (pixel_data.y < (clstr->yMin - 1) || pixel_data.y >(clstr->yMax + 1)) {
			clstr++;
			continue;
		}
		if (pixel_data.x < (clstr->xMin - 1) || pixel_data.x >(clstr->xMax + 1)) {
			clstr++;
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
					clstr++;
				}

				break;  // Pixel was added to cluster => try NEXT cluster
			}
		}

		if (rel == false)
			clstr++;
	}

	if (prevAdded == false)  // Pixel doesnt match to any Cluster - Place new cluster
	{
		ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)pixel_data.x, (uint16_t)pixel_data.y, pixel_data.ToT, pixel_data.ToA} },
			pixel_data.ToA, pixel_data.ToA, (uint16_t)pixel_data.x, (uint16_t)pixel_data.x, (uint16_t)pixel_data.y, (uint16_t)pixel_data.y };
		open_clusters.emplace_back(std::move(cluster));    // Add new cluster
	}
}

void clustering_time_embed::ProcessPixelMerge(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters)
{
	// Loop variables
	bool prevAdded = false;
	bool rel, relX, relY = false;
	int lastAddCluster = 0;

	for (auto clstr = open_clusters.begin(); clstr != open_clusters.end(); clstr++) {

		if ((pixel_data.ToA - clstr->maxToA) > (params.maxClusterDelay)) // Close Old cluster
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

// Returns true if clusters minToa pixel is in the delay range of the merge clusters
bool clustering_time_embed::IsFrontMergeCluster(double clusterMinToa, const thread_data& threadData)
{
	if ((clusterMinToa - threadData.firstToA) < params.maxClusterDelay)
	{
		return true;
	}

	return false;
}

void clustering_time_embed::SendToMergeStation(std::queue<OnePixel>& open_pixels_front, Clusters& open_clusters_back, uint16_t thread_num, FrameNumber frameNum)
{

	/*                                Scheme for merge (x axis = timeline)
			   ______	______	 ______				   ______	______	 ______
			   | T0 |	| T1 |	 | T2 |				   | T0 |	| T1 |	 | T2 |
			   ______	______	 ______		.....	   ______	______	 ______
			 ||      ||       ||       ||			 ||      ||       ||       ||
		  START    ______	______    Cond			Cond   ______	______     END
				   | M0 |	| M1 |						   | M0 |	| M1 |
				   ______	______						   ______	______

		Cond = conditional merge - true when in middle, false when first=start or last=finish (frameNum)
		*/

		/* In case of only 1 thread, we have only one merge station */
	if (numOfThreads == 1)
	{
		clustersToMerge[thread_num].emplace(open_clusters_back);
		pixelsToMerge[thread_num].emplace(open_pixels_front);
		ClearQu(open_pixels_front);
		return;
	}

	/* Place to appropriate merge stations! */
	if (frameNum == FrameNumber::first)	// First thread
	{
		/* Note: thread_num should be always 0 */
		assert(thread_num == 0 && "Error: wrong deduction of first frame");
		clustersToMerge[thread_num].emplace(open_clusters_back);
		assert(open_pixels_front.empty() == true && "Error - first frame cannot have open_pixels!");
	}
	else if (frameNum == FrameNumber::last)	// Last thread
	{
		doneClusters.insert(doneClusters.end(), open_clusters_back.begin(), open_clusters_back.end());
		if (thread_num != 0)
		{
			pixelsToMerge[thread_num - 1].emplace(open_pixels_front);
		}
		else
		{
			pixelsToMerge[numOfThreads - 1].emplace(open_pixels_front);

		}
		ClearQu(open_pixels_front);
	}
	else   // Threads in middle - middle frames
	{
		clustersToMerge[thread_num].emplace(open_clusters_back);
		
		if (thread_num == 0)	/* First thread is middle frame - it has to be placed to last merge station "numOfThreads - 1" cause its merging with last thread  */
		{
			pixelsToMerge[numOfThreads - 1].emplace(open_pixels_front);
		}
		else    /* Middle or last thread is middle frame */
		{
			pixelsToMerge[thread_num - 1].emplace(open_pixels_front);
		}
		ClearQu(open_pixels_front);
	}
}

/* Merge station that runs in thread. Merges all of the given frames until they are all merged */
void clustering_time_embed::MergeMiddleClusters(std::queue<std::queue<OnePixel>>& pixelData, std::queue<Clusters>& openClusters)
{
	while (!pixelData.empty() || !openClusters.empty())
	{
		auto pixelFrame = pixelData.front();
		auto clusterFrame = openClusters.front();
		MergeTwoFrames(pixelFrame, clusterFrame);
		pixelData.pop();
		openClusters.pop();
	}
}

/* Takes pixelData and openClusters from the corresponding frames and merges them */
void clustering_time_embed::MergeTwoFrames(std::queue<OnePixel>& pixelDataFrame, Clusters& openClustersFrame)
{
	Clusters thread_done_clusters;
	thread_data threadData{};

	// Do basic clustering on clusters to merge (openClusters) with pixels to merge (pixelData)
	while (!pixelDataFrame.empty())
	{
		if (abort)
			return;

		ProcessPixelMerge(pixelDataFrame.front(), openClustersFrame, thread_done_clusters);
		pixelDataFrame.pop();
	}

	{	/* Use scoped mutex for exception safety - no deadlock */
		std::lock_guard<std::mutex> lock(mtx);
		doneClusters.insert(doneClusters.end(), thread_done_clusters.begin(), thread_done_clusters.end());
		doneClusters.insert(doneClusters.end(), openClustersFrame.begin(), openClustersFrame.end());
	}
}

void clustering_time_embed::AfterRunClstrCheck()
{
	for (const auto& clstr : doneClusters) {
		if (abort)
			return;
		for (const auto& pixs : clstr.pix) {
			stat_lines_saved++;
			if (pixs.x > clstr.xMax || pixs.x < clstr.xMin || pixs.y > clstr.yMax || pixs.y < clstr.yMin)
			{
				//qDebug() << "Min Max not correct: PIX_X, PIX_Y" << pixs.x << " " << pixs.y <<
					//", Xmax = " << clstr.xMax << ", Xmin = " << clstr.xMin << ", Ymax = " << clstr.yMax << ", Ymin = " << clstr.yMin;
			}
		}
	}

	log_append(utility::print_time_info("Hits saved to clusters:", "Hits", static_cast<double>(stat_lines_saved)));
}