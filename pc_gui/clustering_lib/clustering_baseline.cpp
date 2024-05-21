
/**
 * @clustering_baseline.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "clustering_baseline.h"
#include <queue>

/*
*  IMPORTANT PERFORMANCE COMMENT
* - inlining getline and strtoint and strtolong improves performance
*   by circa 4%. But in Bruteforce, it worsens performance.
*/
void clustering_baseline::do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;
	stat_reset();

	// PARAMETERS
	int upFilter = 255 - params.outerFilterSize;
	int doFilter = params.outerFilterSize;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	std::string oneLine;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };
	std::queue<OnePixel> pixelData;		// FIFO for pixelData

	while (get_my_line(lines, oneLine, params.rn_delim)) {      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;

		if (oneLine[0] == 'C') return;

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
		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue;   // This is faster after the inlined funkctions, not after coordY

		if (params.calibReady)
		{
			ToTValue = energy_calc(coordX, coordY, ToTValue);
		}

		pixelData.push(std::move(OnePixel(coordX, coordY, ToTValue, toaAbsTime)));
		stat_lines_processed++;
	}

	OnePixel inPixel(0, 0, 0, 0);

	ContinualTimer timer;
	timer.Start();
	
	while (!pixelData.empty())
	{
		inPixel = pixelData.front();
		pixelData.pop();

		for (auto clstr = clusters.begin(); clstr != clusters.end(); clstr++) {

			if ((inPixel.ToA - clstr->maxToA) > static_cast<double>(params.maxClusterDelay)) // Close Old cluster
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
			if ((inPixel.ToA - clstr->minToA) > static_cast<double>(params.maxClusterSpan) || (inPixel.ToA - clstr->minToA) < static_cast<double>(-params.maxClusterSpan)) {   // Cant add to this cluster
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
	doneClusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	stat_save(timer.ElapsedMs(), doneClusters.size());
	return;
}

void clustering_baseline::parse_file_clusters(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	std::string oneLine;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };
	bool newCluster = true;

	while (get_my_line(lines, oneLine, params.rn_delim)) {      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;

		// Beginning of cluster
		if (oneLine[0] == 'C')
		{
			newCluster = true;
			continue;
		}

		/* Split line into rows */
		rows[0] = strtok_s(&oneLine.front(), "\t", &context); //strtok(oneLine.data(), "\t");
		rows[1] = strtok_s(NULL, "\t", &context);
		rows[2] = strtok_s(NULL, "\t", &context);
		rows[3] = strtok_s(NULL, "\t", &context);

		// Convert rows to variables
		int x = strtoint(rows[0]);
		int y = strtoint(rows[1]);
		int ToT = strtoint(rows[2]);
		double ToA = (double)strtolong(rows[3]);

		// Emplace pixels to clusters
		if (newCluster == true)
		{
			// Emplace whole new cluster
			doneClusters.emplace_back(ClusterType{ std::vector<OnePixel> { OnePixel {(uint16_t)x, (uint16_t)y, ToT, ToA} }, ToA, ToA, (uint16_t)x, (uint16_t)x, (uint16_t)y, (uint16_t)y });
			newCluster = false;
		}
		else
		{
			// emplace one pixel to last cluster
			doneClusters.back().pix.emplace_back(OnePixel{ (uint16_t)x, (uint16_t)y, ToT, ToA });

			/* Update Min Max coord values */
			if (x > doneClusters.back().xMax) doneClusters.back().xMax = x;
			else if (x < doneClusters.back().xMin) doneClusters.back().xMin = x;
			if (y > doneClusters.back().yMax) doneClusters.back().yMax = y;
			else if (y < doneClusters.back().yMin) doneClusters.back().yMin = y;
			if (doneClusters.back().minToA > ToA) doneClusters.back().minToA = ToA; // Save ToA min
			if (doneClusters.back().maxToA < ToA) doneClusters.back().maxToA = ToA; // Save ToA max
		}
	}
}

void clustering_baseline::do_online_file_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;
	stat_reset();

	// PARAMETERS
	int upFilter = 255 - params.outerFilterSize;
	int doFilter = params.outerFilterSize;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;

	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;

	std::string oneLine;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };
	std::queue<OnePixel> pixelData;		// FIFO for pixelData

	while (get_my_line(lines, oneLine, params.rn_delim)) {      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;

		// File contains complete clusters - return after parsing
		if (oneLine[0] == 'C')
		{
			parse_file_clusters(lines, params, abort);
			return;
		}

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
		int coordX = strtoint(rows[0]);
		int coordY = strtoint(rows[1]);
		int ToTValue = strtoint(rows[2]);
		double toaAbsTime = (double)(strtolong(rows[3]));

		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue;   // This is faster after the inlined funkctions, not after coordY

		if (params.calibReady)
		{
			ToTValue = energy_calc(coordX, coordY, ToTValue);
		}

		pixelData.push(std::move(OnePixel(coordX, coordY, ToTValue, toaAbsTime)));
		stat_lines_processed++;
	}

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
	doneClusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	stat_save(timer.ElapsedMs(), doneClusters.size());
	return;
}
