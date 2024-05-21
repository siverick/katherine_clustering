/**
 * @pixel_feeder.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */


#include <pixel_feeder.h>

void pixel_feeder::run()
{
	running = true;
	messages = 0;
	real_pixels = 0;
	reads = 0;
	printf("Feeder start\n");
	fflush(stdout);

	while(running)	// running loop of the thread
	{
		pix_readout();
	}
}

void pixel_feeder::stop()
{
	printf("Feeder stop\n");
	fflush(stdout);
	running = false;
}

void pixel_feeder::pix_readout()
{
	// Variable to determine how many loops there were no data
	static uint16_t loops_wo_data = 0;
	static uint16_t loops_timeout = 10;
	static OnePixel pixel_tmp_direct = {0,0,0,0};
	uint64_t buf = 0;

	int num = read(pipe->fd_pipe_plugin, &buf, 6);
	reads++;

	if (num > 0) {
		loops_wo_data = 0;
		messages++;

		int data_type = (buf >> 44) & 0xF;

		switch (data_type) {
		case 0x7:	// Frame start
			finished = false;
			timeoffset_for_run = 0;
			pix_output->ClearIn();
			snprintf(buf_string, sizeof(buf_string), "\nFrame start: %d, numBytes = %d", data_type, num);
			printf(buf_string);
			fflush(stdout);
			break;

		case 0xC:	// Frame end
			snprintf(buf_string, sizeof(buf_string), "\nFrame END: %d, numBytes = %d", data_type, num);
			printf(buf_string);
			fflush(stdout);
			pix_output->Flush();
			pix_output->ClearIn();
			finished = true;
			break;

		case 0x5:	// Pixel timestamp offset
			timeoffset_for_run = buf & 0xFFFFFFFF;
			break;

		case 0x4:	// Pixel measurement data
		{
			// Handle too much data situation - dont emplace the pixel further
			if (is_stable() == false) return;

			pixel_tmp_direct = pixel_process_directly(buf, timeoffset_for_run);

			// Filtering
			if (doFilter > 0)
			{
				if (pixel_tmp_direct.x > upFilter || pixel_tmp_direct.x < doFilter || pixel_tmp_direct.y > upFilter || pixel_tmp_direct.y < doFilter) return;
			}

			pix_output->Emplace_Back((std::move(pixel_tmp_direct)));
			real_pixels++;
			break;
		}
		case 0xD: 	// Number of lost pixels
			no_lost_pixels += buf & 0x0FFFFFFFFFFF;	// First 44 bits, last 4 is masked (its data_type)
			break;

		case 0x0:
			// Pixels from detector 0, 0x01 would be from detector 1

			// Handle too much data situation - dont emplace the pixel further
			if (is_stable() == false) return;

			pixel_tmp_direct = pixel_process_directly(buf, timeoffset_for_run);

			// Filtering
			if (doFilter > 0)
			{
				if (pixel_tmp_direct.x > upFilter || pixel_tmp_direct.x < doFilter || pixel_tmp_direct.y > upFilter || pixel_tmp_direct.y < doFilter) return;
			}

			pix_output->Emplace_Back((std::move(pixel_tmp_direct)));
			real_pixels++;
			break;
		default:
			snprintf(buf_string, sizeof(buf_string), "\nUnknown data from pipe: %d, numBytes = %d", data_type, num);
			printf(buf_string);
			fflush(stdout);
			break;
		}

		return;
	}

	/* Wait for new data */
	loops_wo_data++;
	if (loops_wo_data > loops_timeout)
	{
		loops_wo_data = 0;
		std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Sleep, prevent busy waiting

		// This was a snapshot of nonlinear waiting time based on frequency of incoming data
		/*float dur_diff = (std::chrono::high_resolution_clock::now() - last_timeout).count();
		loops_timeout = static_cast<uint16_t>(1.0f / dur_diff);
		last_timeout = std::chrono::high_resolution_clock::now();*/
		// Sleep because no new data are incoming - this prevents constant busy waiting
	}

	return;
}
