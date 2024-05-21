
/**
 * @cluster_benchmark.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "cluster_benchmark.h"
#include <queue>

void cluster_benchmark::parse_data(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;

	stat_lines_sorted = 0;
	stat_lines_processed = 0;
	minToT = 0;
	maxToT = 0;
	avgToT = 0;

	while (pixelData.size() > 0)
	{
		pixelData.pop();
	}

	// PARAMETERS
	int upFilter = 255 - params.outerFilterSize;
	int doFilter = params.outerFilterSize;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;

	std::string oneLine;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };

	while (get_my_line(lines, oneLine, params.rn_delim)) {      // Gets lines without ending line chars - ex. "\n"

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

		// Test
		if (minToT == 0)	minToT = ToTValue;
		if (minToT > ToTValue) minToT = ToTValue;
		if (maxToT < ToTValue) maxToT = ToTValue;
		avgToT += ToTValue;

		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue;   // This is faster after the inlined funkctions, not after coordY

		if (params.calibReady)
		{
			//ToTValue = energy_calc(coordX, coordY, ToTValue);
		}

		pixelData.push(std::move(OnePixel(coordX, coordY, ToTValue, toaAbsTime)));
		stat_lines_processed++;
	}

	avgToT = avgToT / stat_lines_sorted;
}
// BRUTEFORCE
void cluster_benchmark::do_clustering_A(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClusters.clear();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
			{
				if (clstr - clusters.begin() > 1)
				{
					doneClusters.emplace_back(*clstr);
					clstr = clusters.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			/*if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
				continue;
			}*/

			/* Decide if its worth to go through this cluster */
			/* IS TOO FAR UNDER || IS TOO FAR UP */
			/*if (inPixel.y < (clstr->yMin - 1) || inPixel.y >(clstr->yMax + 1)) {
				continue;
			}
			if (inPixel.x < (clstr->xMin - 1) || inPixel.x >(clstr->xMax + 1)) {
				continue;
			}*/

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
					continue;
				}

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clusters[lastAddCluster].pix.insert(clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						/*if (clstr->xMax > clusters[lastAddCluster].xMax) clusters[lastAddCluster].xMax = clstr->xMax;
						if (clstr->xMin < clusters[lastAddCluster].xMin) clusters[lastAddCluster].xMin = clstr->xMin;
						if (clstr->yMax > clusters[lastAddCluster].yMax) clusters[lastAddCluster].yMax = clstr->yMax;
						if (clstr->yMin < clusters[lastAddCluster].yMin) clusters[lastAddCluster].yMin = clstr->yMin;
						if (clstr->minToA < clusters[lastAddCluster].minToA) clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clusters[lastAddCluster].maxToA) clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max
						*/

						clstr = clusters.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						/*if (inPixel.x > clstr->xMax) clstr->xMax = inPixel.x;
						else if (inPixel.x < clstr->xMin) clstr->xMin = inPixel.x;
						if (inPixel.y > clstr->yMax) clstr->yMax = inPixel.y;
						else if (inPixel.y < clstr->yMin) clstr->yMin = inPixel.y;
						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max
						*/

						clstr->pix.emplace_back(OnePixel{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA });
						lastAddCluster = clstr - clusters.begin();
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			clusters.emplace_back(ClusterType{ PixelCluster{ OnePixel {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y });    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end());      // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters_simple();
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClusters.size());
	pixelData = pixelDataTemp;
	return;
}

// TIME MOVE OUT
void cluster_benchmark::do_clustering_B(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClusters.clear();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
			{
				if (clstr - clusters.begin() > 1)
				{
					doneClusters.emplace_back(*clstr);
					clstr = clusters.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
				continue;
			}

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clusters[lastAddCluster].pix.insert(clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						if (clstr->minToA < clusters[lastAddCluster].minToA) clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clusters[lastAddCluster].maxToA) clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

						clstr = clusters.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						clstr->pix.emplace_back(OnePixel{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA });
						lastAddCluster = clstr - clusters.begin();
						prevAdded = true;

						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			clusters.emplace_back(ClusterType{ PixelCluster{ OnePixel {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y });    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end());      // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters_simple();
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClusters.size());
	pixelData = pixelDataTemp;
	return;
}

// X AND Y IF STATEMENT
void cluster_benchmark::do_clustering_C(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClusters.clear();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
			{
				if (clstr - clusters.begin() > 1)
				{
					doneClusters.emplace_back(*clstr);
					clstr = clusters.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
				continue;
			}

			/* Decide if its worth to go through this cluster */
			/* IS TOO FAR UNDER || IS TOO FAR UP */
			if (inPixel.y < (clstr->yMin - 1) || inPixel.y >(clstr->yMax + 1)) {
				continue;
			}
			if (inPixel.x < (clstr->xMin - 1) || inPixel.x >(clstr->xMax + 1)) {
				continue;
			}

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clusters[lastAddCluster].pix.insert(clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						if (clstr->xMax > clusters[lastAddCluster].xMax) clusters[lastAddCluster].xMax = clstr->xMax;
						if (clstr->xMin < clusters[lastAddCluster].xMin) clusters[lastAddCluster].xMin = clstr->xMin;
						if (clstr->yMax > clusters[lastAddCluster].yMax) clusters[lastAddCluster].yMax = clstr->yMax;
						if (clstr->yMin < clusters[lastAddCluster].yMin) clusters[lastAddCluster].yMin = clstr->yMin;
						if (clstr->minToA < clusters[lastAddCluster].minToA) clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clusters[lastAddCluster].maxToA) clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

						clstr = clusters.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						if (inPixel.x > clstr->xMax) clstr->xMax = inPixel.x;
						else if (inPixel.x < clstr->xMin) clstr->xMin = inPixel.x;
						if (inPixel.y > clstr->yMax) clstr->yMax = inPixel.y;
						else if (inPixel.y < clstr->yMin) clstr->yMin = inPixel.y;
						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max

						clstr->pix.emplace_back(OnePixel{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA });
						lastAddCluster = clstr - clusters.begin();
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			clusters.emplace_back(ClusterType{ PixelCluster{ OnePixel {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y });    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end());      // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClusters.size());
	pixelData = pixelDataTemp;
	return;
}

void cluster_benchmark::do_clustering_D(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClusters.clear();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;


	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
			{
				if (clstr - clusters.begin() > 1)
				{
					doneClusters.emplace_back(*clstr);
					clstr = clusters.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
				continue;
			}

			/* Decide if its worth to go through this cluster */
			/* IS TOO FAR UNDER || IS TOO FAR UP */
			if (inPixel.y < (clstr->yMin - 1) || inPixel.y >(clstr->yMax + 1)) {
				continue;
			}
			if (inPixel.x < (clstr->xMin - 1) || inPixel.x >(clstr->xMax + 1)) {
				continue;
			}

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clusters[lastAddCluster].pix.insert(clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						if (clstr->xMax > clusters[lastAddCluster].xMax) clusters[lastAddCluster].xMax = clstr->xMax;
						if (clstr->xMin < clusters[lastAddCluster].xMin) clusters[lastAddCluster].xMin = clstr->xMin;
						if (clstr->yMax > clusters[lastAddCluster].yMax) clusters[lastAddCluster].yMax = clstr->yMax;
						if (clstr->yMin < clusters[lastAddCluster].yMin) clusters[lastAddCluster].yMin = clstr->yMin;
						if (clstr->minToA < clusters[lastAddCluster].minToA) clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clusters[lastAddCluster].maxToA) clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

						clstr = clusters.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						if (inPixel.x > clstr->xMax) clstr->xMax = inPixel.x;
						else if (inPixel.x < clstr->xMin) clstr->xMin = inPixel.x;
						if (inPixel.y > clstr->yMax) clstr->yMax = inPixel.y;
						else if (inPixel.y < clstr->yMin) clstr->yMin = inPixel.y;
						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max

						OnePixel newPixel = OnePixel{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA };
						clstr->pix.emplace_back(std::move(newPixel));
						lastAddCluster = clstr - clusters.begin();
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y };
			clusters.emplace_back(std::move(cluster));    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end());      // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClusters.size());
	pixelData = pixelDataTemp;
	return;
}

// Improve 
// uint times instead of double
void cluster_benchmark::do_clustering_E(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClustersDef.clear();
	doneClustersDef.shrink_to_fit();
	doneClusters.clear();
	doneClusters.shrink_to_fit();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	OnePixelDef inPixel(0, 0, 0, 0);

	OnePixel tempp = OnePixel{ 0,0,0,0 };
	OnePixelDef temp_nice = OnePixelDef{ 0,0,0,0 };

	while (pixelDataTemp.empty() == false)
	{
		tempp = pixelDataTemp.front();
		pixelDataTemp.pop();

		temp_nice.x = tempp.x;
		temp_nice.y = tempp.y;
		temp_nice.ToT = tempp.ToT;
		temp_nice.ToA = static_cast<int64_t>(tempp.ToA);

		pixelDataDef.push(temp_nice);
	}

	ContinualTimer timer;
	timer.Start();

	while (!pixelDataDef.empty())
	{
		inPixel = pixelDataDef.front();
		pixelDataDef.pop();

		for (auto clstr = clustersDef.begin(); clstr != clustersDef.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > static_cast<int64_t>(params.maxClusterDelay)) // Close Old cluster
			{
				if (clstr - clustersDef.begin() > 1)
				{
					doneClustersDef.emplace_back(*clstr);
					clstr = clustersDef.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			if ((inPixel.ToA - clstr->minToA) > static_cast<int64_t>(params.maxClusterSpan) || (inPixel.ToA - clstr->minToA) < -static_cast<int64_t>(params.maxClusterSpan)) {   // Cant add to this cluster
				continue;
			}

			/* Decide if its worth to go through this cluster */
			/* IS TOO FAR UNDER || IS TOO FAR UP */
			if (inPixel.y < (clstr->yMin - 1) || inPixel.y >(clstr->yMax + 1)) {
				continue;
			}
			if (inPixel.x < (clstr->xMin - 1) || inPixel.x >(clstr->xMax + 1)) {
				continue;
			}

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clustersDef[lastAddCluster].pix.insert(clustersDef[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						if (clstr->xMax > clustersDef[lastAddCluster].xMax) clustersDef[lastAddCluster].xMax = clstr->xMax;
						if (clstr->xMin < clustersDef[lastAddCluster].xMin) clustersDef[lastAddCluster].xMin = clstr->xMin;
						if (clstr->yMax > clustersDef[lastAddCluster].yMax) clustersDef[lastAddCluster].yMax = clstr->yMax;
						if (clstr->yMin < clustersDef[lastAddCluster].yMin) clustersDef[lastAddCluster].yMin = clstr->yMin;
						if (clstr->minToA < clustersDef[lastAddCluster].minToA) clustersDef[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clustersDef[lastAddCluster].maxToA) clustersDef[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

						clstr = clustersDef.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						if (inPixel.x > clstr->xMax) clstr->xMax = inPixel.x;
						else if (inPixel.x < clstr->xMin) clstr->xMin = inPixel.x;
						if (inPixel.y > clstr->yMax) clstr->yMax = inPixel.y;
						else if (inPixel.y < clstr->yMin) clstr->yMin = inPixel.y;
						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max

						OnePixelDef newPixel = OnePixelDef{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA };
						clstr->pix.emplace_back(std::move(newPixel));
						lastAddCluster = clstr - clustersDef.begin();
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			ClusterTypeDef cluster = ClusterTypeDef{ std::vector<OnePixelDef>{ OnePixelDef {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y };
			clustersDef.emplace_back(std::move(cluster));    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClustersDef.insert(doneClustersDef.end(), clustersDef.begin(), clustersDef.end());      // Remaining move to DONE
	clustersDef.clear();
	clustersDef.shrink_to_fit();

	for (const auto& clstr : doneClustersDef) {
		for (const auto& pixs : clstr.pix) {
			stat_lines_saved++;
		}
	}

	/* Utility functions after the clustering */
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClustersDef.size());
	return;
}

// Energy LUT
void cluster_benchmark::do_clustering_F(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClusters.clear();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;


	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		if (params.calibReady)	inPixel.ToT = energy_calc(inPixel.x, inPixel.y, inPixel.ToT);

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
			{
				if (clstr - clusters.begin() > 1)
				{
					doneClusters.emplace_back(*clstr);
					clstr = clusters.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
				continue;
			}

			/* Decide if its worth to go through this cluster */
			/* IS TOO FAR UNDER || IS TOO FAR UP */
			if (inPixel.y < (clstr->yMin - 1) || inPixel.y >(clstr->yMax + 1)) {
				continue;
			}
			if (inPixel.x < (clstr->xMin - 1) || inPixel.x >(clstr->xMax + 1)) {
				continue;
			}

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clusters[lastAddCluster].pix.insert(clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						if (clstr->xMax > clusters[lastAddCluster].xMax) clusters[lastAddCluster].xMax = clstr->xMax;
						if (clstr->xMin < clusters[lastAddCluster].xMin) clusters[lastAddCluster].xMin = clstr->xMin;
						if (clstr->yMax > clusters[lastAddCluster].yMax) clusters[lastAddCluster].yMax = clstr->yMax;
						if (clstr->yMin < clusters[lastAddCluster].yMin) clusters[lastAddCluster].yMin = clstr->yMin;
						if (clstr->minToA < clusters[lastAddCluster].minToA) clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clusters[lastAddCluster].maxToA) clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

						clstr = clusters.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						if (inPixel.x > clstr->xMax) clstr->xMax = inPixel.x;
						else if (inPixel.x < clstr->xMin) clstr->xMin = inPixel.x;
						if (inPixel.y > clstr->yMax) clstr->yMax = inPixel.y;
						else if (inPixel.y < clstr->yMin) clstr->yMin = inPixel.y;
						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max

						OnePixel newPixel = OnePixel{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA };
						clstr->pix.emplace_back(std::move(newPixel));
						lastAddCluster = clstr - clusters.begin();
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y };
			clusters.emplace_back(std::move(cluster));    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end());      // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClusters.size());
	pixelData = pixelDataTemp;
	return;
}

// Energy LUT
void cluster_benchmark::do_clustering_G(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	auto pixelDataTemp = pixelData;
	uint64_t temp_lines = stat_lines_processed;
	stat_reset();
	stat_lines_processed = temp_lines;
	stat_lines_saved = 0;
	doneClusters.clear();

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	if (params.calibReady)
		create_energy_lut(500);

	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		if (params.calibReady)	inPixel.ToT = energy_calc_lut(inPixel.x, inPixel.y, inPixel.ToT);

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
			{
				if (clstr - clusters.begin() > 1)
				{
					doneClusters.emplace_back(*clstr);
					clstr = clusters.erase(clstr);
					clstr--;
				}

				continue;
			}

			/* ToA range check */
			// NOTE: m_abs() or double-if causes maxClusterSpan to be double sided, downwards and upwards
			if ((inPixel.ToA - clstr->minToA) > params.maxClusterSpan || (inPixel.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
				continue;
			}

			/* Decide if its worth to go through this cluster */
			/* IS TOO FAR UNDER || IS TOO FAR UP */
			if (inPixel.y < (clstr->yMin - 1) || inPixel.y >(clstr->yMax + 1)) {
				continue;
			}
			if (inPixel.x < (clstr->xMin - 1) || inPixel.x >(clstr->xMax + 1)) {
				continue;
			}

			for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
				//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

				relX = ((inPixel.x) == (pixs.x + 1)) || ((inPixel.x) == (pixs.x - 1)) || ((inPixel.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
				relY = ((inPixel.y) == (pixs.y + 1)) || ((inPixel.y) == (pixs.y - 1)) || ((inPixel.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
				rel = relX && relY;       // Is related in both X AND Y

				if (rel)    // If Neighbor, add to cluster
				{
					if (prevAdded)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						clusters[lastAddCluster].pix.insert(clusters[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						if (clstr->xMax > clusters[lastAddCluster].xMax) clusters[lastAddCluster].xMax = clstr->xMax;
						if (clstr->xMin < clusters[lastAddCluster].xMin) clusters[lastAddCluster].xMin = clstr->xMin;
						if (clstr->yMax > clusters[lastAddCluster].yMax) clusters[lastAddCluster].yMax = clstr->yMax;
						if (clstr->yMin < clusters[lastAddCluster].yMin) clusters[lastAddCluster].yMin = clstr->yMin;
						if (clstr->minToA < clusters[lastAddCluster].minToA) clusters[lastAddCluster].minToA = clstr->minToA; // Merge ToA min
						if (clstr->maxToA > clusters[lastAddCluster].maxToA) clusters[lastAddCluster].maxToA = clstr->maxToA; // Merge ToA max

						clstr = clusters.erase(clstr);       // Erase the current Cluster
						clstr--;
					}
					else            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						if (inPixel.x > clstr->xMax) clstr->xMax = inPixel.x;
						else if (inPixel.x < clstr->xMin) clstr->xMin = inPixel.x;
						if (inPixel.y > clstr->yMax) clstr->yMax = inPixel.y;
						else if (inPixel.y < clstr->yMin) clstr->yMin = inPixel.y;
						if (clstr->minToA > inPixel.ToA) clstr->minToA = inPixel.ToA; // Save ToA min
						if (clstr->maxToA < inPixel.ToA) clstr->maxToA = inPixel.ToA; // Save ToA max

						OnePixel newPixel = OnePixel{ inPixel.x, inPixel.y, inPixel.ToT, inPixel.ToA };
						clstr->pix.emplace_back(std::move(newPixel));
						lastAddCluster = clstr - clusters.begin();
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}
			}
		}

		if (prevAdded)    // Reset FLAGS
		{
			rel = false;
			prevAdded = false;
		}
		else        // Pixel doesnt match to any Cluster - Place new cluster
		{
			ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)inPixel.x, (uint16_t)inPixel.y, inPixel.ToT, inPixel.ToA} }, inPixel.ToA, inPixel.ToA, (uint16_t)inPixel.x, (uint16_t)inPixel.x, (uint16_t)inPixel.y, (uint16_t)inPixel.y };
			clusters.emplace_back(std::move(cluster));    // Add new cluster
		}
	}

	timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end());      // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	assert(stat_lines_processed == stat_lines_saved, "Error in number of lines");
	stat_save(timer.ElapsedMs(), doneClusters.size());
	pixelData = pixelDataTemp;
	return;
}