/**
 * @plugin_definition.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */


#ifndef PLUGIN_MAIN_PLUGIN_DEFINITION_H_
#define PLUGIN_MAIN_PLUGIN_DEFINITION_H_

#include "cluster_definition.h"

// Every command has '-' sign to distignuish, if more commands were given
// Remember: Use '-' prefix in every command!
const std::string command_simple_recv = "-RECV";
const std::string command_clustering = "-CLSTR";
const std::string command_clustering_energy = "-TOT";
const std::string command_pixel_counting = "-PCNT";
const std::string command_idle = "-IDLE";
const std::string command_shutdown = "-SHUT";
const std::string command_help = "-help";

// Size of pixel feeder buffer
#define FEEDER_BUFF_SIZE 10000

/* Used for example
 * if (state = plugin_states::ready) start_something(); */
enum plugin_status
{
	loading,
	ready,
	running,
	loading_error
};

/* Used for example
 * set_plugin_state(plugins plug, bool on_off); */
enum plugins
{
	simple_receiver,
	clustering_clusters,
	clustering_energies,
	pixel_counting,
	idle
};

struct pixel_item {
	uint32_t coord;
	uint16_t x;
	uint16_t y;
	uint64_t timestamp;
	uint8_t fToa;
	uint32_t ToT;
	double absTime;
};

class plugin_definition : public cluster_definition
{

public:
	inline pixel_item pixel_process(uint64_t& pxl_data, uint32_t& time_offset)
	{
		/*struct pixel_data_struct *tmp;
		 tmp = (struct pixel_data_struct *) &tmp1;*/

		uint32_t x = (pxl_data >> 28) & 0xFF;
		uint32_t y = (pxl_data >> 36) & 0xFF;
		uint32_t tot = (pxl_data >> 4) & 0x3FF;
		uint32_t toa = (pxl_data >> 14) & 0x3FFF;
		uint32_t ftoa = (pxl_data) & 0xF;

		pixel_item temp_output;
		temp_output.x = x;
		temp_output.y = y;
		temp_output.coord = (256 * y) + x;
		temp_output.ToT = tot;
		temp_output.fToa = ftoa;
		temp_output.timestamp = (uint64_t) toa + ((uint64_t) time_offset * 16384);
		temp_output.absTime = (temp_output.timestamp * 25.0e-9) - (ftoa * 1.5625e-9);

		return temp_output;
	}

	inline OnePixel to_one_pixel(pixel_item&& pix)
	{
		return  OnePixel{pix.x, pix.y, static_cast<uint16_t>(pix.ToT), static_cast<int64_t>(pix.absTime)};
	}

	inline OnePixel pixel_process_directly(uint64_t& pxl_data, uint32_t& time_offset)
	{
		/*struct pixel_data_struct *tmp;
		 tmp = (struct pixel_data_struct *) &tmp1;*/

		const int64_t toaLsb = 25;
		// toaFineLsb = 1.5625; -> 15625 / 10000 in calculation to avoid float

		uint32_t x = (pxl_data >> 28) & 0xFF;
		uint32_t y = (pxl_data >> 36) & 0xFF;
		uint32_t tot = (pxl_data >> 4) & 0x3FF;
		uint64_t toa = (pxl_data >> 14) & 0x3FFF;
		uint64_t ftoa = (pxl_data) & 0xF;

		OnePixel ret = OnePixel{0,0,0,0};
		ret.x = x;
		ret.y = y;
		ret.ToT = tot;
		ret.ToA = (((static_cast<int64_t>(toa) + (static_cast<int64_t>(time_offset) * 16384)) * toaLsb) - ((static_cast<int64_t>(ftoa) * 15625) / 10000));

		return ret;
	}
};


#endif /* PLUGIN_MAIN_PLUGIN_DEFINITION_H_ */
