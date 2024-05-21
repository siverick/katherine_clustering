/**
 * @clustering_main.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#ifndef PLUGIN_MAIN_CLUSTERING_MAIN_H_
#define PLUGIN_MAIN_CLUSTERING_MAIN_H_

#include <MTQueue.h>
#include <online_clustering_baseline.h>
#include "plugin_definition.h"
#include <memory>
#include <thread>
#include <unistd.h>
#include <atomic>

class clustering_main : clustering_base
{
public:
	// TODO: Ted to nejak rozdelim na ruzny pluginy a kazdy plugin bude mit svoji classu
	clustering_main(std::shared_ptr<MTQueueBuffered<OnePixel, FEEDER_BUFF_SIZE>> shared_buf,
			std::shared_ptr<MTVector<CompactClusterType>> done_cl,
			std::shared_ptr<MTVector<uint16_t>> done_ene,
			std::shared_ptr<MTVector<OnePixel>> out_pix,
			std::shared_ptr<MTVector<OnePixelCount>> out_count,
			std::shared_ptr<MTVariable<size_t>> out_count_for_energy)
	{
		in_pixels = shared_buf;
		out_clusters = done_cl;
		out_energies = done_ene;
		pixel_count_for_energy = out_count_for_energy;
		out_pixels = out_pix;
		out_pixel_counts = out_count;
		plugin_running = plugins::idle;
		running = false;

		// Initialize parameters to default values
		params.clusterFilterSize = 0;
		params.filterBiggerClusters = false;
		params.outerFilterSize = 0;
		params.maxClusterSpan = 200;
		params.maxClusterDelay = 200000;
	}

	void set_plugin(plugins pl)
	{
		plugin_running = pl;
	}

	plugins get_plugin()
	{
		return plugin_running;
	}

	void run();
	void stop();

	void set_clustering_params(ClusteringParamsOnline par)
	{
		params = par;
	}

	// Note: Inaccurate -> this emplaces open_clusters right into done_clusters, although they are not eligible to be placed in there
	void get_rest_of_clusters()
	{
		clustering.get_rest_of_clusters(out_clusters);
	}

	void clear_and_free_memory()
	{
		// Clear and free memory
		clustering.reset_open_clusters();	// Free the memory of open clusters
		out_clusters->EraseAll();
		out_clusters->ShrinkToFit();
		out_energies->EraseAll();
		out_energies->ShrinkToFit();
		out_pixel_counts->EraseAll();
		out_pixel_counts->ShrinkToFit();
		out_pixels->EraseAll();
		out_pixels->ShrinkToFit();
		in_pixels->ClearOut();
	}

	volatile std::atomic<bool> is_finished;

private:
	// Plugin classes
	online_clustering_baseline clustering;	// Performs enhanced bruteforce clustering - best performance for smaller clusters
	// Note: Quadtree clustering performs better only for bigger clusters - big heavy ions etc.

	// Inputs
	std::shared_ptr<MTQueueBuffered<OnePixel, FEEDER_BUFF_SIZE>> in_pixels;

	// Outputs
	std::shared_ptr<MTVector<CompactClusterType>> out_clusters;		/* Cluster output */
	std::shared_ptr<MTVector<uint16_t>> out_energies;			/* Energy output for histogram */
	std::shared_ptr<MTVariable<size_t>> pixel_count_for_energy;			// Pixel counter for energies
	std::shared_ptr<MTVector<OnePixel>> out_pixels;			// Pixel output direct
	std::shared_ptr<MTVector<OnePixelCount>> out_pixel_counts;	// Pixel count output
	// more...

	// Plugin for state machine
	volatile plugins plugin_running;
	volatile std::atomic<bool> running;
	ClusteringParamsOnline params;

	void state_machine();

	void cluster_pixel();

	void simply_receive();

	void receive_pixel_count();

	void cluster_energy();
};

#endif /* PLUGIN_MAIN_CLUSTERING_MAIN_H_ */
