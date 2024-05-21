/**
 * @online_clustering_minmax.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include <online_clustering_baseline.h>

void online_clustering_baseline::cluster_pixel(OnePixel&& pix, std::shared_ptr<MTVector<CompactClusterType>>& done_clusters, ClusteringParamsOnline& params)
{
	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;
	auto clstr = open_clusters.begin();

	while (clstr != open_clusters.end()) {

		// Send complete clusters
		if ((pix.ToA - clstr->maxToA) > params.maxClusterDelay)
		{
			// Filter smaller/bigger clusters
			if (params.clusterFilterSize > 0)
			{
				// filter smaller clusters -> delete them and continues
				if (params.filterBiggerClusters == false && clstr->pix.size() < static_cast<uint32_t>(params.clusterFilterSize))
				{
					clstr = open_clusters.erase(clstr);
					continue;
				}

				// filter bigger clusters -> delete them and continue
				if (params.filterBiggerClusters == true && clstr->pix.size() > static_cast<uint32_t>(params.clusterFilterSize))
				{
					clstr = open_clusters.erase(clstr);
					continue;
				}
			}

			// done_clusters (CompactClusterType), gets only clstr->pix part of the cluster
			// Other part is thrown out and can be later deduced during postprocessing
			done_clusters->Emplace_Back(std::move(clstr->pix));
			clstr = open_clusters.erase(clstr);
			continue;
		}

		/* ToA range check */
		// NOTE: m_abs() or double-if makes maxClusterSpan to be double sided, downwards and upwards
		if ((pix.ToA - clstr->minToA) > params.maxClusterSpan || (pix.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
			clstr++;
			continue;
		}

		/* Decide if its worth to go through this cluster */
		/* IS TOO FAR UNDER || IS TOO FAR UP */
		if (pix.y < (clstr->yMin - 1) || pix.y >(clstr->yMax + 1)) {
			clstr++;
			continue;
		}
		if (pix.x < (clstr->xMin - 1) || pix.x >(clstr->xMax + 1)) {
			clstr++;
			continue;
		}

		for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
			//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

			relX = ((pix.x) == (pixs.x + 1)) || ((pix.x) == (pixs.x - 1)) || ((pix.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
			relY = ((pix.y) == (pixs.y + 1)) || ((pix.y) == (pixs.y - 1)) || ((pix.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
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
					if (pix.x > clstr->xMax) clstr->xMax = pix.x;
					else if (pix.x < clstr->xMin) clstr->xMin = pix.x;
					if (pix.y > clstr->yMax) clstr->yMax = pix.y;
					else if (pix.y < clstr->yMin) clstr->yMin = pix.y;
					if (clstr->minToA > pix.ToA) clstr->minToA = pix.ToA; // Save ToA min
					if (clstr->maxToA < pix.ToA) clstr->maxToA = pix.ToA; // Save ToA max

					OnePixel newPixel = OnePixel{ pix.x, pix.y, pix.ToT, pix.ToA };
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
		ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)pix.x, (uint16_t)pix.y, pix.ToT, pix.ToA} },
			pix.ToA, pix.ToA, (uint16_t)pix.x, (uint16_t)pix.x, (uint16_t)pix.y, (uint16_t)pix.y };
		open_clusters.emplace_back(std::move(cluster));    // Add new cluster
	}

	return;
}

// Actually slower than normal clustering, energy is postprocess of cluster
void online_clustering_baseline::cluster_for_energy(OnePixel&& pix, std::shared_ptr<MTVector<uint16_t>>& done_energies, std::shared_ptr<MTVariable<size_t>> pixel_count, ClusteringParamsOnline& params)
{
	// Loop variables
	bool prevAdded = false;
	bool rel{}, relX, relY = false;
	int lastAddCluster = 0;
	auto clstr = open_clusters.begin();

	while (clstr != open_clusters.end()) {

		// NOTE: This is working properly, tested!
		if ((pix.ToA - clstr->maxToA) > params.maxClusterDelay) // Close Old cluster
		{
			// Filter smaller/bigger clusters
			if (params.clusterFilterSize > 0)
			{
				// filter smaller clusters -> delete them and continues
				if (params.filterBiggerClusters == false && clstr->pix.size() < static_cast<uint32_t>(params.clusterFilterSize))
				{
					clstr = open_clusters.erase(clstr);
					continue;
				}

				// filter bigger clusters -> delete them and continue
				if (params.filterBiggerClusters == true && clstr->pix.size() > static_cast<uint32_t>(params.clusterFilterSize))
				{
					clstr = open_clusters.erase(clstr);
					continue;
				}
			}

			uint16_t energy = 0;
			for (auto &elem : clstr->pix)
			{
				energy += elem.ToT;
			}

			// count pixels from cluster -> very important for PC application
			pixel_count->Add_To_Value(clstr->pix.size());

			// Emplace a new energy
			done_energies->Emplace_Back(std::move(energy));
			clstr = open_clusters.erase(clstr);
			continue;
		}

		/* ToA range check */
		// NOTE: m_abs() or double-if makes maxClusterSpan to be double sided, downwards and upwards
		if ((pix.ToA - clstr->minToA) > params.maxClusterSpan || (pix.ToA - clstr->minToA) < -params.maxClusterSpan) {   // Cant add to this cluster
			clstr++;
			continue;
		}

		/* Decide if its worth to go through this cluster */
		/* IS TOO FAR UNDER || IS TOO FAR UP */
		if (pix.y < (clstr->yMin - 1) || pix.y >(clstr->yMax + 1)) {
			clstr++;
			continue;
		}
		if (pix.x < (clstr->xMin - 1) || pix.x >(clstr->xMax + 1)) {
			clstr++;
			continue;
		}

		for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster
			//for (auto& pixs : reverse(*clstr)) {  // Cycle through Pixels of Cluster

			relX = ((pix.x) == (pixs.x + 1)) || ((pix.x) == (pixs.x - 1)) || ((pix.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
			relY = ((pix.y) == (pixs.y + 1)) || ((pix.y) == (pixs.y - 1)) || ((pix.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
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
					if (pix.x > clstr->xMax) clstr->xMax = pix.x;
					else if (pix.x < clstr->xMin) clstr->xMin = pix.x;
					if (pix.y > clstr->yMax) clstr->yMax = pix.y;
					else if (pix.y < clstr->yMin) clstr->yMin = pix.y;
					if (clstr->minToA > pix.ToA) clstr->minToA = pix.ToA; // Save ToA min
					if (clstr->maxToA < pix.ToA) clstr->maxToA = pix.ToA; // Save ToA max

					OnePixel newPixel = OnePixel{ pix.x, pix.y, pix.ToT, pix.ToA };
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
		ClusterType cluster = ClusterType{ PixelCluster{ OnePixel {(uint16_t)pix.x, (uint16_t)pix.y, pix.ToT, pix.ToA} },
			pix.ToA, pix.ToA, (uint16_t)pix.x, (uint16_t)pix.x, (uint16_t)pix.y, (uint16_t)pix.y };
		open_clusters.emplace_back(std::move(cluster));    // Add new cluster
	}

	return;
}
