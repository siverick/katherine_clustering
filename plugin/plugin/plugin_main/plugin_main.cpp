/**
 * @plugin_main.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */


#include <plugin_main.h>

// Connect to detector data a be in a neutral state
plugin_main::plugin_main(networking* net)
{
	// Init variables
	status = plugin_status::loading;
	mode = plugins::idle;
	network = net;

	// Connect to readout
	bool connected = true;
	if (network->connect_detector() < 0)
	{
		perror("\nDetector not connected\n");
		connected = false;
	}

	status = connected ? plugin_status::ready : plugin_status::loading_error;
	check_err_state();

	// Create shared variables
	pixel_feed = std::make_shared<MTQueueBuffered<OnePixel, FEEDER_BUFF_SIZE>>();
	out_clusters = std::make_shared<MTVector<CompactClusterType>>();
	out_energies = std::make_shared<MTVector<uint16_t>>();
	pixel_count_for_energy = std::make_shared<MTVariable<size_t>>();
	out_pixels = std::make_shared<MTVector<OnePixel>>();
	out_pixel_counts = std::make_shared<MTVector<OnePixelCount>>();
	params.clusterFilterSize = 0;
	params.filterBiggerClusters = false;
	params.maxClusterDelay = 200000;
	params.maxClusterSpan = 200;
	params.outerFilterSize = 0;

	// Create future threads objects
	feeder = new pixel_feeder(network, pixel_feed);
	clustering = new clustering_main(pixel_feed, out_clusters, out_energies, out_pixels, out_pixel_counts, pixel_count_for_energy);
}

plugin_main::~plugin_main()
{
	feeder->stop();
	clustering->stop();
	delete network;
	delete feeder;
	delete clustering;
}

// Check if there isnt error on feeder pipe
void plugin_main::check_err_state()
{
	// Error handling
	if (status == plugin_status::loading_error)
	{
		printf("Check error state found error\n");
		fflush(stdout);

		bool wasRunning = false;
		if (t_feeder.joinable() == true)	// If the thread is running
		{
			wasRunning = true;
			feeder->stop();		// Stop the feeder thread
			t_feeder.join();	// Wait for it to finish
		}
		std::cout << "error, reconnecting to redout" << std::endl;
		while (network->connect_detector() < 0) std::this_thread::sleep_for(std::chrono::seconds(1));	// Sleep until we are reconnected

		// after reconnection, start the feeder again if was running and is not running now
		if (wasRunning == true && t_feeder.joinable() == false)
		{
			auto call_f = [&]() { feeder->run(); };
			t_feeder = std::thread(call_f);
		}
	}
}

int plugin_main::plugin_start(plugins to_start)
{
	mode = to_start;
	clustering->set_plugin(mode);

	switch(mode)
	{
	case plugins::clustering_clusters:
		// Create their threads if they are not running
		if (t_feeder.joinable() == false)
		{
			auto call_f = [&]() { feeder->run(); };
			t_feeder = std::thread(call_f);
		}
		if (t_clustering.joinable() == false)
		{
			auto call_c = [&]() { clustering->run(); };
			t_clustering = std::thread(call_c);
		}

		break;
	case plugins::simple_receiver:
		// Create thread if its not running
		if (t_feeder.joinable() == false)
		{
			auto call_f = [&]() { feeder->run(); };
			t_feeder = std::thread(call_f);
		}
		if (t_clustering.joinable() == false)
		{
			auto call_c = [&]() { clustering->run(); };
			t_clustering = std::thread(call_c);
		}

		break;
	case plugins::clustering_energies:
		// Create thread if its not running
		if (t_feeder.joinable() == false)
		{
			auto call_f = [&]() { feeder->run(); };
			t_feeder = std::thread(call_f);
		}
		if (t_clustering.joinable() == false)
		{
			auto call_c = [&]() { clustering->run(); };
			t_clustering = std::thread(call_c);
		}

		break;
	case plugins::pixel_counting:
		// Create thread if its not running
		if (t_feeder.joinable() == false)
		{
			auto call_f = [&]() { feeder->run(); };
			t_feeder = std::thread(call_f);
		}
		if (t_clustering.joinable() == false)
		{
			auto call_c = [&]() { clustering->run(); };
			t_clustering = std::thread(call_c);
		}

		break;
	case plugins::idle:
		// Stop the feeder
		if (t_feeder.joinable() == true)
		{
			feeder->stop();		// Stop the feeder thread
			t_feeder.join();	// Wait for it to finish
		}
		// Stop the clustering
		if (t_clustering.joinable() == true)
		{
			clustering->clear_and_free_memory();	// Clear the memory
			clustering->stop();		// Stop the feeder thread
			t_clustering.join();	// Wait for it to finish
		}

		// INFO: Clustering Idle was already set (its sleeping)
		break;
	default:
		mode = plugins::idle;
		perror("Unknown command in plugin main!\n");
		return -1;
	}


	return 0;
}




