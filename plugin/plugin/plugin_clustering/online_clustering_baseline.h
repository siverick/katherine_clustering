/**
 * @online_clustering_minmax.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#ifndef PLUGIN_CLUSTERING_ONLINE_CLUSTERING_BASELINE_H_
#define PLUGIN_CLUSTERING_ONLINE_CLUSTERING_BASELINE_H_

#include <clustering_base.h>
#include "MTQueue.h"
#include <memory>

/*
 * Baseline clustering algorithm
 */

class online_clustering_baseline : public clustering_base, public cluster_definition
{
public:
	// Note: Pass shared_ptr by reference because we dont want to take ownership of the ptr
	void cluster_pixel(OnePixel&& pix, std::shared_ptr<MTVector<CompactClusterType>>& done_clusters, ClusteringParamsOnline& params);
	void cluster_for_energy(OnePixel&& pix, std::shared_ptr<MTVector<uint16_t>>& done_energies, std::shared_ptr<MTVariable<size_t>> pixel_count, ClusteringParamsOnline& params);

	// Note: Inaccurate -> this emplaces open_clusters right into done_clusters, although they are not eligible to be placed in there
	void get_rest_of_clusters(std::shared_ptr<MTVector<CompactClusterType>>& done_clusters)
	{
		// Emplace clusters one by one
		for (auto& cluster : open_clusters)
		{
			done_clusters->Emplace_Back(cluster.pix);
		}
	}

	// Delete contents and free the memory
	void reset_open_clusters()
	{
		open_clusters.clear();
		open_clusters.shrink_to_fit();
	}

private:
	Clusters open_clusters;
};

#endif /* PLUGIN_CLUSTERING_ONLINE_CLUSTERING_BASELINE_H_ */
