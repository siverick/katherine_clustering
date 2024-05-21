/**
 * @networking.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */


#include <networking.h>

networking::networking()
{
	// TODO Auto-generated constructor stub
	sock = 0;
	fd_pipe_plugin = -1;
	last_port = 0;
	last_ip = "";
	BUFF_SIZE = 50;
	buffer = new char[BUFF_SIZE];
}

networking::~networking()
{
	delete[] buffer;
}

/*
 * -----------------------------------
 * 		PIPE DETECTOR NETWORKING
 * -----------------------------------
 * */

int networking::connect_detector()
{
	// Open fifo/pipe to read
	fd_pipe_plugin = -1;

	while (fd_pipe_plugin < 0) {
		fd_pipe_plugin = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);

		if (fd_pipe_plugin < 0) {
			printf("\nFifo not found....");
			fflush(stdout);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	printf("\nFifo found....");
	fflush(stdout);

	return 0;
}

int networking::disconnect_detector()
{
	const int c_timeout = 3;
	int timeout = 0;

	// auto remaining = read(fd_pipe_plugin, &buf, 6); for later

	while (timeout < c_timeout)
	{
		int result = close(fd_pipe_plugin);

		if (result != -1) break;	// FIFO closed!
		else
		{
			timeout++;
			printf("\nError closing fifo");
			fflush(stdout);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	if(c_timeout == timeout)
	{
		printf("\nFIFO close timed out");
		fflush(stdout);
		return -1;
	}

	return 0;
}

// One pixel is currently a 6 Byte data
// Max throughput is ~10 MHits
// loops = 1.000.000 * 6 / BUFFIZE = 22.000
int networking::flush_detector_data(int ms_timeout)
{
	static std::chrono::time_point<std::chrono::steady_clock> last_time = std::chrono::steady_clock::now();
	char buffer[264];

	last_time = std::chrono::steady_clock::now();
	while(read(fd_pipe_plugin, buffer, 264) > 0)
	{
		// Timeout after 100 ms to maintain responsivity towards LAN
		if ((std::chrono::steady_clock::now() - last_time) > std::chrono::milliseconds(ms_timeout))
			return -1;
	}

	return 0;
}

/*
 * ---------------------------------------
 * 			LAN/SERVER NETWORKING
 * ---------------------------------------
 * */

int networking::connect_lan(std::string ip, uint16_t port)
{
	// Create socket which connects to server
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Failed to create socket");
		return -1;
	}

	// Define server to connect to
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(ip.c_str());
	server.sin_port = htons(port);

	// TODO: Search IP adresses in the 192.168.1.X network

	printf("\n\nConnecting to server...");
	while (connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0)// Waiting until server online
	{
		printf(".");
		fflush(stdout);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	printf("\nConnected to server!....\n");
	fflush(stdout);

	// Save adresses after connecting
	last_ip = ip;
	last_port = port;

	return 0;
}

int networking::reconnect_lan()
{
	if ((last_ip == "") || (last_port == 0)) return -1;		// Sanity check

	// Close socket
	close(sock);
	perror("connection closed, reconnecting");
	fflush(stdout);

	// Make new socket and connect to the server
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Failed to create socket");
		fflush(stdout);
		return -1;
	}

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(last_ip.c_str());
	server.sin_port = htons(last_port);

	printf("\n\nConnecting to server...");
	fflush(stdout);

	// Wait for reconnection
	while (connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0)	// Waiting here until windows forms is started
	{
		printf(".");
		fflush(stdout);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	printf("\nConnected to server!....\n");
	fflush(stdout);

	return 0;
}


int networking::disconnect_lan()
{
	int result = close(sock);	// Close closed socket
	return result;		// -1 in case of error
}

/*				ABOUT send()
 * Depending on if the socket is non-blocking,or not, it is NOT guaranteed that a single
 * send will actually send all data. You must check return value and might have to call
 * send again (with correct buffer offsets).

	For instance if you want to send 10 bytes of data (len=10), you call send(sock, buf, len, 0).
	However lets say it only manages to send 5 bytes, then send(..) will return 5, meaning that
	you will have to call it again later like send(sock, (buf + 5), (len - 5), 0). Meaning, skip
	first five bytes in buffer, they're already sent, and withdraw five bytes from the total number
	of bytes (len) we want to send.
*/
// return number of bytes sent
int networking::send_to_lan(const std::string& message)
{
	size_t rem, done = 0;
	int len = 0;
	size_t length = message.size();
	rem = length;

	while (length > done)
	{
		len = send(sock, message.c_str() + done, rem, 0);
		done += len;
		rem = length - done;

		if (len == -1) return -1;
	}

	return static_cast<int>(done);
}

int networking::send_to_lan(const char* message, size_t length)
{
	size_t rem, done = 0;
	int len = 0;
	rem = length;

	while (length > done)
	{
		len = send(sock, message + done, rem, 0);
		done += len;
		rem = length - done;

		if (len == -1) return -1;
	}

	return static_cast<int>(done);
}

std::string networking::read_from_lan()
{
	std::string message = "";
	int bytesRead = 0;

	// Peek if data incoming
	fd_set readfds;
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	// TADY SE TO HRYZNE - timeout nefunguje
	bytesRead = select((sock + 1), &readfds, NULL, NULL, &tv);

	// return if no data are waiting, or if error
	if (bytesRead == 0)	return message;	// return empty string
	else if (bytesRead == -1)	return "ERROR";	// error

	// Read everything until no more data incoming
	char tmp_buff[MAX_BUF_SIZE+1];
	memset(tmp_buff, 0,MAX_BUF_SIZE+1);
	while ((bytesRead = select(sock + 1, &readfds, NULL, NULL, &tv)) > 0)
	{
		bytesRead = recv(sock, tmp_buff, MAX_BUF_SIZE, 0);

		// Reset select() variables - they may change with select()
		tv.tv_usec = 0;
		tv.tv_sec = 0;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);

		if (bytesRead > 0) message.append(tmp_buff, bytesRead);
		else if (bytesRead == -1) break;
		else reconnect_lan();	// When bytesRead == 0 means we were disconnected
	}

	if (bytesRead == -1)	// error occured
		message = "ERROR";

	return message;
}

// Blocking until certain amount
int networking::recv_data(std::string& data, int length)
{
	int done = recv(sock, buffer, length, 0);
	if (done == -1) return -1;

	data.append(buffer, done);
	return done;
}

// Non blocking
int networking::recv_data_packet(std::string& data)
{
	int len = 0;
	size_t done = 0;

	// Peek if data incoming
	fd_set readfds;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	len = select(sock + 1, &readfds, NULL, NULL, &tv);
	// return if no data are waiting, or if error
	if (len == 0)	return 0;
	else if (len == -1)	return -1;

	// Read until we get minimum of the frame size
	while (done < 50)
	{
		len = recv_data(data, 2);
		if (len == -1) return -1;
		else if (len == 0)	// Client disconnected when len == 0
		{
			disconnect_lan();
			return -1;
		}
		done += len;

		if (data.find(';') != std::string::npos) break;	// break if end of header ';' was found
	}

	if (data.find(';') == std::string::npos) return 0;	// return error if nothing was found

	// parse number of bytes to receive
	size_t start = data.find('#');
	size_t end = data.find(';');
	if (start == std::string::npos || end == std::string::npos) return -1;	// error if end or start wasnt found
	size_t length, rem = 0;
	std::string number = data.substr((start + 1), (end - 1));
	length = std::atoll(number.c_str());
	int doneBefore = done;
	rem = length - done;

	// increase buffer size in case necessary
	if (length > BUFF_SIZE)
	{
		char* temp = new char[BUFF_SIZE];
		memcpy(temp, buffer, BUFF_SIZE);
		delete[] buffer;
		size_t OLD_BUFF_SIZE = BUFF_SIZE;
		BUFF_SIZE = 2 * length;
		buffer = new char[BUFF_SIZE];
		memset(buffer, 0, BUFF_SIZE);
		memcpy(buffer, temp, OLD_BUFF_SIZE);	// Note: Buffer will not overrrun because BUFF_SIZE is always bigger than OLD_BUFF_SIZE
		delete[] temp;
	}

	// Receive more data until we received the whole dataframe
	while (length > done)
	{
		len = recv(sock, buffer + done, rem, 0);
		done += len;
		rem = length - done;

		if (len == -1) return -1;
	}

	data.append(buffer + doneBefore, done - doneBefore);	// append remaning data from buffer

	// Don't go higher than 50 KB
	if (BUFF_SIZE > 50000)
	{
		delete[] buffer;
		BUFF_SIZE = 50000;
		buffer = new char[BUFF_SIZE];
		memset(buffer, 0, BUFF_SIZE);
	}

	return done;
}
