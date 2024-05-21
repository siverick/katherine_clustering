/**
 * @clustering_main.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include <clustering_main.h>

// state machine and flow control
void clustering_main::run()
{
	running = true;
	is_finished = false;

	while (running)
	{
		state_machine();
	}

	// Clear and free memory
	clear_and_free_memory();
	is_finished = true;
}

void clustering_main::stop()
{
	running = false;
}

void clustering_main::state_machine()
{
	switch(plugin_running)
	{
	case plugins::clustering_clusters:
		cluster_pixel();
		break;
	case plugins::simple_receiver:
		simply_receive();
		break;
	case plugins::clustering_energies:
		cluster_energy();
		break;
	case plugins::pixel_counting:
		receive_pixel_count();
		break;
	case plugins::idle:
		clear_and_free_memory();	// Free the memory of open clusters
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		break;
	default:
		plugin_running = plugins::idle;	 // Idle should be default state
		break;
	}
}

void clustering_main::simply_receive()
{
	// Variable to determine how many loops there were no data
	static uint16_t loops_wo_data = 0;
	static uint16_t loops_timeout = 10;

	// If empty, move to out buffer and return - then, next call to this fun can have at least some pixels
	if (in_pixels->isEmpty() == true)
	{

		// Sleep if still no data incoming
		loops_wo_data++;
		if (loops_wo_data > loops_timeout)
		{
			//in_pixels->Flush();	// Move from IN to OUT buffer inside pix_input buffered Queue
			loops_wo_data = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return;
	}

	loops_wo_data = 0;
	out_pixels->Emplace_Back(std::move(in_pixels->Pop()));
}

// Process the pixel and place it in cluster - save to out_clusters
void clustering_main::cluster_pixel()
{
	static uint16_t loops_wo_data = 0;
	static uint16_t loops_timeout = 10;

	// If empty, move to out buffer and return - then, next call to this fun can have at least some pixels
	if (in_pixels->isEmpty() == true)
	{

		// Sleep if still no data incoming
		loops_wo_data++;
		if (loops_wo_data > loops_timeout)
		{
			//in_pixels->Flush();	// Move from IN to OUT buffer inside pix_input buffered Queue
			loops_wo_data = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return;
	}


	// Do the clustering on the pixel - done clusters are moved to out_clusters
	loops_wo_data = 0;
	clustering.cluster_pixel(std::move(in_pixels->Pop()), out_clusters, params);
}


void clustering_main::receive_pixel_count()
{
	// Variable to determine how many loops there were no data
	static uint16_t loops_wo_data = 0;
	static uint16_t loops_timeout = 10;
	static OnePixel pixel = {0,0,0,0};

	// If empty, move to out buffer and return - then, next call to this fun can have at least some pixels
	if (in_pixels->isEmpty() == true)
	{

		// Sleep if still no data incoming
		loops_wo_data++;
		if (loops_wo_data > loops_timeout)
		{
			//in_pixels->Flush();	// Move from IN to OUT buffer inside pix_input buffered Queue
			loops_wo_data = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return;
	}

	loops_wo_data = 0;

	// Emplace only x and y coord of a pixel
	pixel = in_pixels->Pop();
	out_pixel_counts->Emplace_Back(OnePixelCount(pixel.x, pixel.y));
}

// Process the pixel and place it in cluster -> get only the energy - save to out_energies
void clustering_main::cluster_energy()
{
	static uint16_t loops_wo_data = 0;
	static uint16_t loops_timeout = 10;

	// If empty, move to out buffer and return - then, next call to this fun can have at least some pixels
	if (in_pixels->isEmpty() == true)
	{

		// Sleep if still no data incoming
		loops_wo_data++;
		if (loops_wo_data > loops_timeout)
		{
			//in_pixels->Flush();	// Move from IN to OUT buffer inside pix_input buffered Queue
			loops_wo_data = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return;
	}

	loops_wo_data = 0;

	// Do the clustering on the pixel - done clusters are moved to out_energies
	clustering.cluster_for_energy(std::move(in_pixels->Pop()), out_energies, pixel_count_for_energy, params);
}
