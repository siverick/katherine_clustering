
/**
 * @plugin_definition.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#ifndef PLUGIN_MAIN_PLUGIN_DEFINITION_H_
#define PLUGIN_MAIN_PLUGIN_DEFINITION_H_

#include <cstdint>
#include <string>

class plugin_definition
{
public:
	const std::string command_simple_recv = "-RECV";
	const std::string command_clustering = "-CLSTR";
	const std::string command_clustering_energy = "-TOT";
	const std::string command_pixel_counting = "-PCNT";
	const std::string command_idle = "-IDLE";
	const std::string command_shutdown = "-SHUT";
	const std::string command_help = "-help";

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

	plugins get_command_ack(const std::string& command)
	{
		if (command == command_simple_recv)
		{
			return plugins::simple_receiver;
		}
		else if (command == command_clustering_energy)
		{
			return plugins::clustering_energies;
		}
		else if (command == command_clustering)
		{
			return plugins::clustering_clusters;
		}
		else if (command == command_idle)
		{
			return plugins::idle;
		}
		else if (command == command_pixel_counting)
		{
			return plugins::pixel_counting;
		}
		else
			return plugins::idle;	// Uknown command, expect idle
	}
};

#endif /* PLUGIN_MAIN_PLUGIN_DEFINITION_H_ */
