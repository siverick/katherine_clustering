/**
 * @cluster_minmax.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include <offline_clustering.h>
#include <queue>
#include "string.h"

void offline_clustering::do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;
	stat_reset();

	doneClusters.clear();
	doneClusters.shrink_to_fit();
	clusters.clear();
	clusters.shrink_to_fit();

	// PARAMETERS
	uint16_t upFilter = 255 - params.filterSize;
	uint16_t doFilter = params.filterSize;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;

	// Loop variables
	bool prevAdded = false;
	bool rel {}, relX, relY = false;
	size_t lastAddCluster = 0;

	std::string oneLine;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };
	std::queue<OnePixel> pixelData;		// FIFO for pixelData

	while (get_my_line(lines, oneLine, params.rn_delim))
	{      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;

		if (abort)
		{
			return;
		}

		stat_lines_sorted++;

		rows[0] = strtok_r((char*) oneLine.data(), "\t", &context);
		rows[1] = strtok_r(NULL, "\t", &context);
		rows[2] = strtok_r(NULL, "\t", &context);
		rows[3] = strtok_r(NULL, "\t", &context);

		// ATOI 6300 ms vs STRTOINT 6100 ms with 20 PIX filter
		// ATOI CCA 200 ms slower than my Fast strtoint
		int32_t coordX = strtoint(rows[0]) / 256; // std::atoi(rows[0]) / 256;
		int32_t coordY = strtoint(rows[0]) % 256; // std::atoi(rows[0]) % 256;
		double toaAbsTime = (double) ((strtolong(rows[1]) * toaLsb) - (strtolong(rows[2]) * toaFineLsb)); // (ns) TimeFromBeginning = 25 * ToA - 1.5625 * fineToA
		int32_t ToTValue = strtoint(rows[3]);  //std::atoi(rows[3]);
		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue; // This is faster after the inlined funkctions, not after coordY

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
				if ((clstr - clusters.begin()) > 1)
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
						lastAddCluster = static_cast<size_t>(clstr - clusters.begin());
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

	double elapsed = timer.Stop();

	/* POSTPROCESS Clusters */
	doneClusters.insert(doneClusters.end(), clusters.begin(), clusters.end()); // Remaining move to DONE
	clusters.clear();
	clusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	stat_save(elapsed, doneClusters.size());
	return;
}
