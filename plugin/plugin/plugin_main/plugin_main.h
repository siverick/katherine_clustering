/**
 * @plugin_main.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#ifndef PLUGIN_MAIN_PLUGIN_MAIN_H_
#define PLUGIN_MAIN_PLUGIN_MAIN_H_

#include <MTQueue.h>
#include <offline_clustering.h>
#include "networking.h"
#include "clustering_main.h"
#include "pixel_feeder.h"
#include "plugin_definition.h"
#include <thread>
#include <vector>
#include <iostream>
#include <memory>
#include "serializer.h"

class plugin_main
{

public:
	plugin_main(networking* net);
	~plugin_main();

	size_t pixels_num = 0;

	// Middle vars - feeders
	std::shared_ptr<MTQueueBuffered<OnePixel, FEEDER_BUFF_SIZE>> pixel_feed;

	// Outputs
	std::shared_ptr<MTVector<CompactClusterType>> out_clusters;
	std::shared_ptr<MTVector<uint16_t>> out_energies;
	std::shared_ptr<MTVariable<size_t>> pixel_count_for_energy;
	std::shared_ptr<MTVector<OnePixel>> out_pixels;
	std::shared_ptr<MTVector<OnePixelCount>> out_pixel_counts;


	int plugin_start(plugins plug);
	void check_err_state();

#define MIN_FRAME_BYTE_SIZE 5

	// Is feeder finished - indicates whether measurement ended
	bool is_finished()
	{
		return feeder->finished;
	}

	bool is_cl_finished()
	{
		return clustering->is_finished;
	}

	// Is clustering finished - indicates whether clustering processes all the data
	bool is_clustering_finished()
	{
		return (pixel_feed->sizeOut() < 10);
	}

	size_t get_mes()
	{
		return feeder->messages;
	}

	size_t get_pix()
	{
		return feeder->real_pixels;
	}

	size_t get_reads()
	{
		return feeder->reads;
	}

	bool is_done_clusters_big()
	{
		// Dont emplace rest of unfinished clusters after clustering finished

		if(out_clusters->Size()*sizeof(CompactClusterType) > MIN_FRAME_BYTE_SIZE) return true;
		else
		{
			// If finished and is not big, delete the rest - because no more data will be gathered
			if (feeder->finished == true && is_clustering_finished())
			{
				clustering->clear_and_free_memory();
			}

			return false;
		}
	}

	std::vector<CompactClusterType> get_done_clusters()
	{
		return out_clusters->Get_All_And_Erase();
	}

	bool is_done_pixels_big()
	{
		if(out_pixels->Size()*sizeof(OnePixel) > MIN_FRAME_BYTE_SIZE) return true;
		else
		{
			// If finished and is not big, delete the rest - because no more data will be gathered
			if (feeder->finished == true && is_clustering_finished())
			{
				clustering->clear_and_free_memory();
			}

			return false;
		}
	}

	std::vector<OnePixel> get_done_pixels()
	{
		pixels_num += out_pixels->Size();
		return out_pixels->Get_All_And_Erase();
	}

	bool is_done_counts_big()
	{
		if (out_pixel_counts->Size()*sizeof(OnePixelCount) > MIN_FRAME_BYTE_SIZE) return true;
		else
		{
			// If finished and is not big, delete the rest - because no more data will be gathered
			if (feeder->finished == true && is_clustering_finished())
			{
				clustering->clear_and_free_memory();
			}

			return false;
		}
	}

	std::vector<OnePixelCount> get_done_counts()
	{
		return out_pixel_counts->Get_All_And_Erase();
	}

	bool is_done_histograms_big()
	{
		// Dont emplace rest of unfinished energies after clustering finished

		if(out_energies->Size()*sizeof(uint16_t) > MIN_FRAME_BYTE_SIZE) return true;
		else
		{
			// If finished and is not big, delete the rest - because no more data will be gathered
			if (feeder->finished == true && is_clustering_finished())
			{
				clustering->clear_and_free_memory();
			}

			return false;
		}
	}

	std::vector<uint16_t> get_done_histograms()
	{
		return out_energies->Get_All_And_Erase();
	}

	// Get pixel counts -> how many pixels were included in the energies now gathered
	size_t get_pixel_counts_for_energies()
	{
		size_t ret = pixel_count_for_energy->Get_Value();
		pixel_count_for_energy->Set_Value(0);	// Reset the counter to 0

		return ret;
	}

	plugin_status get_status()
	{
		return status;
	}

	plugins get_running_plugins()
	{
		return mode;
	}

	// Only while in plugins == IDLE
	void set_params(ClusteringParamsOnline par)
	{
		params = par;

		// Only when in idle -> protect from race condition
		if (mode == plugins::idle)
		{
			clustering->set_clustering_params(params);
			feeder->set_filtering(params.outerFilterSize);	// feeder does the outer filtering
		}
	}

private:
	networking* network;
	clustering_main* clustering;
	pixel_feeder* feeder;

	// Threads for modules
	std::thread t_feeder;
	std::thread t_clustering;

	// State enums
	plugin_status status;
	plugins mode;

	// Clustering params
	ClusteringParamsOnline params;
};

#endif /* PLUGIN_MAIN_PLUGIN_MAIN_H_ */
