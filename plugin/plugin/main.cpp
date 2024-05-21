
/**
 * @main.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include <sys/mman.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <offline_clustering.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include "file_loader.h"
#include <iostream>
#include <thread>
#include "plugin_main.h"

//-static-libstdc++

void calculate_clustering()
{
	offline_clustering clstr;
	std::string lines = file_loader::loadDefaultFile();
	ClusteringParams params = {false, 200000, 200, 0, true};
	volatile bool _abort = false;

	auto start = std::chrono::high_resolution_clock::now();

	clstr.do_clustering(lines, params, _abort);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    float milliseconds = duration.count() * 1000.0f;

	std::cout << "CURRENT THREAD Elapsed time chrono: " << milliseconds << " ms" << std::endl;
	printf(clstr.stat_print().c_str());
	fflush(stdout);

	/* Antoher thread test */

	start = std::chrono::high_resolution_clock::now();

	offline_clustering th_clstr;

	auto call = [&]() { th_clstr.do_clustering(lines, params, _abort); };
	std::thread worker(call);
	worker.join();

	end = std::chrono::high_resolution_clock::now();
	duration = end - start;
	milliseconds = duration.count() * 1000.0f;

	std::cout << "ANOTHER THREAD Elapsed time chrono: " << milliseconds << " ms" << std::endl;
	printf(th_clstr.stat_print().c_str());
	fflush(stdout);
}

void benchmark_clustering_files()
{
	offline_clustering clstr;
	ClusteringParams params = {false, 200000, 200, 0, true};
	volatile bool _abort = false;


	// File Konvick
	std::string lines = file_loader::loadDefaultFile();
	clstr.do_clustering(lines, params, _abort);
	std::cout << "FILE A: ";
	printf(clstr.stat_print().c_str());
	fflush(stdout);

	// FILE DEG
	file_loader::loadPixelData("/bin/katherine/deg60_5_embedded.txt", lines);
	clstr.do_clustering(lines, params, _abort);
	std::cout << "FILE B: ";
	printf(clstr.stat_print().c_str());
	fflush(stdout);

	// FILE PROTONS
	file_loader::loadPixelData("/bin/katherine/protons_embedded.txt", lines);
	clstr.do_clustering(lines, params, _abort);
	std::cout << "FILE C: ";
	printf(clstr.stat_print().c_str());
	fflush(stdout);

	// FILE ATLAS
	file_loader::loadPixelData("/bin/katherine/atlas_mix_embedded.txt", lines);
	clstr.do_clustering(lines, params, _abort);
	std::cout << "FILE D: ";
	printf(clstr.stat_print().c_str());
	fflush(stdout);

	// FILE ELECTRONS
	file_loader::loadPixelData("/bin/katherine/electrons_angle30.txt", lines);
	clstr.do_clustering(lines, params, _abort);
	std::cout << "FILE E: ";
	printf(clstr.stat_print().c_str());
	fflush(stdout);
}


//-----------------------------------PLUGIN BEGINS HERE---------------------------------------------------------

plugin_main* plugin;
bool program_running;

size_t sent_pixels = 0;

template <typename T>
void send_to_lan(networking* netw, std::vector<T> output, dataframe_types type);
void send_to_lan(networking* netw, std::string message, dataframe_types type);

std::string get_help()
{
	std::stringstream help_stream;
	return "";
	/* Disabled for now */


	help_stream << "Command descriptions:\n";
	help_stream << "'" << command_simple_recv << "'" << " - Simple receiver of hits.\n";
	help_stream << "'" << command_clustering_energy << "'" << " - Get clusters energies.\n";
	help_stream << "'" << command_clustering << "'" << " - Get clusters with all info.\n";
	help_stream << "'" << command_pixel_counting << "'" << " - Get only pixel counts.\n";
	help_stream << "'" << command_idle << "'" << " - Disable all modes and idle.\n";
	help_stream << "'" << command_shutdown << "'" << " - End the program.\n";
	help_stream << "'" << command_help << "'" << " - Display this help message.\n";

	return help_stream.str();
}

// return latest command from possible array of commands
std::string get_latest_command(const std::string& command)
{
	if (command == "") return command;

	// command can be: -TOT-help-CLSTR .. we have to get the last command separated by '-'
	std::string delim = "-";
	auto pos_last = command.find_last_of(delim);	// last delimiters position

	if (pos_last == std::string::npos) return "";	// No suitable command found

	return command.substr(pos_last);
}

// send command acknowledgement
void acknowledge_command(std::string command, networking* netw)
{
	if (command == "") return;

	send_to_lan(netw, command, dataframe_types::acknowledge);
}

// handle incoming commands from socket
void handle_incoming_commands(const std::string& command, networking* netw)
{
	int status = 0;

	if (command == command_simple_recv)
	{
		status = plugin->plugin_start(plugins::simple_receiver);
		if (status >= 0) acknowledge_command(command, netw);
	}
	else if (command == command_clustering_energy)
	{
		status = plugin->plugin_start(plugins::clustering_energies);
		if (status >= 0) acknowledge_command(command, netw);
	}
	else if (command == command_clustering)
	{
		status = plugin->plugin_start(plugins::clustering_clusters);
		if (status >= 0) acknowledge_command(command, netw);
	}
	else if (command == command_idle)
	{
		status = plugin->plugin_start(plugins::idle);
		if (status >= 0) acknowledge_command(command, netw);
	}
	else if (command == command_pixel_counting)
	{
		status = plugin->plugin_start(plugins::pixel_counting);
		if (status >= 0) acknowledge_command(command, netw);
	}
	else if (command == command_shutdown)
	{
		program_running = false;
		return;
	}
	else if (command == command_help)
	{
		std::string help = get_help();
		send_to_lan(netw, help, dataframe_types::messages);
		return;
	}
	else
	{
		std::string message = "Uknown command! Type help to view commands.";
		send_to_lan(netw, message, dataframe_types::messages);
		return;
	}

	// If starting of plugin failed, it defaulted to IDLE mode
	if (status < 0)
	{
		acknowledge_command(command_idle, netw);
	}
}

// configure clustering parameters - and check boundaries - if successful, send acknowledgement
void set_config(std::string config, networking* netw)
{
	ClusteringParamsOnline params = serializer::deserialize_params(config);

	// Sanity check all parameters and reset them to default values if they are not meaningful
	if (params.clusterFilterSize < 0 || params.clusterFilterSize > 65000)
	{
		params.clusterFilterSize = 0;
	}
	if (params.maxClusterSpan < 1 || params.maxClusterSpan > 100000000)
	{
		params.maxClusterSpan = 200;
	}
	if (params.outerFilterSize < 0 || params.outerFilterSize > 120)
	{
		params.outerFilterSize = 0;
	}
	if (params.maxClusterDelay < 1)
	{
		params.maxClusterDelay = 200000;
	}

	plugin->set_params(params);

	std::string ack = serializer::serialize_params(params);
	serializer::attach_header(ack, dataframe_types::config);
	netw->send_to_lan(ack);
}

// read data from socket and handle it accordingly
void read_lan(networking* netw)
{
	std::string message = "";
	int status = netw->recv_data_packet(message);

	// Serves disconnected somehow - try to reconnect
	if (status < 0)	// Reading from LAN was unsuccesful
	{
		netw->reconnect_lan();
		return;
	}

	if (message == "") return;

	dataframe_types type = serializer::get_type(message);
	serializer::deattach_header(message);

	switch(type)
	{
	case dataframe_types::command:
		if (message.find('-', 0) == std::string::npos) return;	// Dummy check
		message = get_latest_command(message);	// latest command
		handle_incoming_commands(message, netw);
		break;
	case dataframe_types::errors:
		// Possible error handling in future
		message.insert(0, "SERVER ERROR: ");
		utility::print_info(message, 0);
		break;
	case dataframe_types::acknowledge:
		break;
	case dataframe_types::config:
		set_config(message, netw);
		break;
	default:
		message.insert(0, "UNEXPECTED MES: ");
		utility::print_info(message, 0);
		break;
	}
	return;
}

// send data to socket
void send_to_lan(networking* netw, std::string message, dataframe_types type)
{
	serializer::attach_header(message, type);
	int bytes = netw->send_to_lan(message);
	if (bytes < 0) perror("message not sent\n");
}

/* Output - ClusterType | Energies(uint16_t) | OnePixel */
template <typename T>
void send_to_lan(networking* netw, std::vector<T> output, dataframe_types type)
{
	int bytes = netw->send_to_lan(reinterpret_cast<const char*>(output.data()), output.size() * sizeof(T));
	if (bytes < 0) perror("vector not sent\n");
}

/*
 *-----------------------------------
 *-----------------------------------
 * 			MAIN THREAD
 * - connected to LAN server (PC)
 * - processes commands from server
 * - controls plugin threads flow
 * - sends output to server
 * - plugin_main is extension to main, the same thread
 *-----------------------------------
 *-----------------------------------
 * */

int main(int argc, char **argv) {

	// * CLUSTERING BENCHMARK
	// * Note: This could be a self test.
	//calculate_clustering();
	//benchmark_clustering_files();
	//printf("benchmark done\n");
	//fflush(stdout);
	//return 0;

	networking* network = new networking();
	plugin = new plugin_main(network);

	/* Connect to LAN network - with timeout */
	uint16_t timeout = 0;

	const std::string SERVER_IP = "192.168.1.10";
	const uint16_t PORT = 21000;

	if(network->connect_lan(SERVER_IP, PORT) < 0)
	{
		timeout++;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (timeout > 10)	// Create new networking after a second if its failing to create socket
		{
			delete network;
			perror("\nError: Server not connected\n");
			fflush(stdout);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			network = new networking();
			timeout = 0;
		}
	}

	/* MAIN LOOP */
	uint32_t pending_timer = 0;
	program_running = true;
	bool last_meas_state = true;
	bool pending_meas_finished = false;

	while(program_running)
	{
		// Check change in the state of measurement - running or finished
		if (last_meas_state != plugin->is_finished())
		{
			printf("State changed now\n");
			fflush(stdout);
			last_meas_state = plugin->is_finished();
			pending_meas_finished = true;
		}

		// Handle the change of measurement - send to socket whether measurement has started or finsihed
		if (pending_meas_finished)
		{
			pending_timer++;

			if (last_meas_state == false)
			{
				send_to_lan(network, "MEAS STARTED", dataframe_types::messages);
				pending_timer = 0;
				pending_meas_finished = false;
			}

			if (plugin->is_clustering_finished() == true && last_meas_state == true)
			{
				printf("Finished measurement\n");
				fflush(stdout);

				// Debug output
				char buf_string[200];
				snprintf(buf_string, sizeof(buf_string), "\nReads = %d, Messages: %d, Pixels = %d, real out Pixels = %d", plugin->get_reads(), plugin->get_mes(), plugin->get_pix(), plugin->pixels_num);
				printf(buf_string);
				fflush(stdout);

				plugin->pixels_num = 0;

				pending_timer = 0;
				pending_meas_finished = false;

				send_to_lan(network, "MEAS FINISHED", dataframe_types::messages);
			}
		}


		// Read commands/messages from socket
		read_lan(network);

		plugin->check_err_state();	// Check network and reconnect if necessary
		plugins mode = plugin->get_running_plugins();

		// State machine - send output data according to the running mode
		switch (mode) {
		case plugins::simple_receiver:
			if (plugin->is_done_pixels_big())
				send_to_lan(network, serializer::serialize_pixels(plugin->get_done_pixels()), dataframe_types::pixels);
			break;
		case plugins::clustering_clusters:
			if(plugin->is_done_clusters_big())
				send_to_lan(network, serializer::serialize_clusters(plugin->get_done_clusters()), dataframe_types::clusters);
			break;
		case plugins::clustering_energies:
			if (plugin->is_done_histograms_big())
				send_to_lan(network, serializer::serialize_histograms(plugin->get_done_histograms(), plugin->get_pixel_counts_for_energies()), dataframe_types::energies);
			break;
		case plugins::pixel_counting:
			if (plugin->is_done_counts_big())
				send_to_lan(network, serializer::serialize_pixel_counts(plugin->get_done_counts()), dataframe_types::pixel_counts);
			break;
		case plugins::idle:
			// Flush the pipe - Do nothing - < 0 means timeout, in that case, dont sleep
			if (network->flush_detector_data(100) < 0)
			{
				// Continue when timeout -> NO SLEEP, we dont care in IDLE mode
				perror("Flushing during idle timed out - too much MHits/s!\n");
				fflush(stdout);
				continue;
			}
			break;
		default:
			utility::print_info("Error: Running unexpected mode, switching to idle!",0);
			send_to_lan(network, "Running unexpected mode! Switch mode to fix.", dataframe_types::errors);
			plugin->plugin_start(plugins::idle);
			perror("Running in unexpected mode\n");
			fflush(stdout);
			break;
		}
		fflush(stdout);

		// Send data to PC every X milliseconds
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Cleanup the pointers
	delete network;
	delete plugin;

	/* End of program! */
	return 0;
}
