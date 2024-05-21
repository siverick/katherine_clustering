/**
 * @networking.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */


#ifndef PLUGIN_MAIN_NETWORKING_H_
#define PLUGIN_MAIN_NETWORKING_H_

#define FIFO_PATH "/tmp/myfifo"
#define MAX_BUF_SIZE 128

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <string>
#include <thread>
#include <chrono>

class networking
{
public:
	networking();
	virtual ~networking();

	/* Detector networking - incoming data hits */
	int fd_pipe_plugin = -1;	// File descriptor for pipe
	int connect_detector();
	int disconnect_detector();
	int flush_detector_data(int ms_timeout);

	/* Lan networking - with PC etc. */
	int connect_lan(std::string ip, uint16_t port);
	int reconnect_lan();
	int disconnect_lan();
	int send_to_lan(const std::string& message);
	int send_to_lan(const char* message, size_t length);
	std::string read_from_lan();	// Blocking read from LAN until everything read
	int recv_data(std::string& data, int length);
	int recv_data_packet(std::string& data);

private:
	std::string last_ip;
	uint16_t last_port;

	size_t BUFF_SIZE;
	char* buffer;

	/* Lan networking - with PC etc. */
	int sock = 0;	// socket file descriptor
	struct sockaddr_in server;	// server's network address information - server's IP address and port number
};

#endif /* PLUGIN_MAIN_NETWORKING_H_ */
