/**
 * @pixel_feeder.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */


#ifndef PLUGIN_MAIN_PIXEL_FEEDER_H_
#define PLUGIN_MAIN_PIXEL_FEEDER_H_

#include <MTQueue.h>
#include "clustering_base.h"
#include "plugin_definition.h"
#include "networking.h"
#include "utility.h"
#include <chrono>
#include <thread>
#include <memory>
#include <atomic>

class pixel_feeder : private plugin_definition
{
public:
	pixel_feeder(networking* netw, std::shared_ptr<MTQueueBuffered<OnePixel, FEEDER_BUFF_SIZE>> shared)
	{
		printf("Feeder created\n");
		fflush(stdout);
		pipe = netw;
		pix_output = shared;
		finished = true;
	}

	~pixel_feeder()
	{
		printf("Feeder destroyed\n");
		fflush(stdout);

		if (pipe != nullptr)
			delete pipe;
	}

	void run();
	void stop();

	volatile std::atomic<bool> finished;

	void set_filtering(int filterSize)
	{
		// sanity check
		if (filterSize > 120 || filterSize < 0)
		{
			upFilter = 0;
			doFilter = 0;
			return;
		}

		// set up and down filter
		if (filterSize >= 0)
		{
			upFilter = 255 - filterSize;
			doFilter = filterSize;
		}
	}

	size_t reads = 0;
	size_t messages = 0;
	size_t real_pixels = 0;

private:
	volatile std::atomic<bool> running;
	char buf_string[200];

	// Inputs sources
	networking* pipe;

	// Outputs
	std::shared_ptr<MTQueueBuffered<OnePixel, FEEDER_BUFF_SIZE>> pix_output;	// Shared Queue for pixel data betweeen this and further processings

	// Network related variables
	uint32_t timeoffset_for_run = 0;
	uint64_t no_lost_pixels = 0;
	//char buf_string[100];

	// Outer filter variables
	uint16_t upFilter = 0;
	uint16_t doFilter = 0;

	// Private pix reading functions
	inline void pix_readout();

	// return whether we dont have too much data - we have to stop receiving pixels
	bool is_stable()
	{
		static bool tempWait = false;	// temp wait for half buffer to be emptied
		static uint32_t loops = 0;

		loops++;

		if (loops > 500000)
		{
			loops = 0;
			// When > 30 MB / 16B -> not stable
			if (tempWait == false && pix_output->sizeOut() > 1875000)
			{
				printf("Is not stable - more than 1.8M pix\n");
				fflush(stdout);
				tempWait = true;
			}

			// Return to normal mode when half of buffer was processed
			if (tempWait == true && pix_output->sizeOut() < (1875000/2))
			{
				printf("Is stable again\n");
				fflush(stdout);
				tempWait = false;
			}

			// is_stable is negation of tempWait
			return (!tempWait);
		}

		return true;
	}
};

#endif /* PLUGIN_MAIN_PIXEL_FEEDER_H_ */
