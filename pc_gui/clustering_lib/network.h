
/**
 * @network.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#ifdef __linux__ 
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#elif _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#define WIN32_LEAN_AND_MEAN 
// #pragma comment indicates to the linker that the Ws2_32.lib file is needed.
#pragma comment(lib, "Ws2_32.lib")

#else
#endif

#include <fcntl.h>
#define DEFAULT_PORT "21000"
#define MIN_FRAME_BYTE_SIZE 50
#include <string>
#include "utility.h"
#include <thread>

class networking
{
public:
	std::atomic<bool> isConnected = false;
	std::atomic<bool> abort = false;
	int listenFD = 0;
	int clientFD = 0;

	// Use addr 127.0.0.1 for LOOPBACK .. or ::1 for IPV6
	networking()
	{
#ifdef _WIN32
		WSA_init();		// startup WSA to enable socket function in windows
#endif
		isConnected = false;
	}

	~networking()
	{
		if (isConnected)
		{
			close();
		}
		abort = true;

		// We have to wait, because if socket is still connecting, we have to wait till it times out and return to the worker.cpp thread. Else it prevent application from shutting down
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	void connect(uint16_t port)
	{
		listenFD = 0;

		listenFD = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFD < 0) return;

		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		// We skip the IP - because our IP adress is DHCP and not static, 
		// so by not setting IP adress, its chosen automatically
		server.sin_port = htons(port);
		server.sin_family = AF_INET;

		if (bind(listenFD, (struct sockaddr*)&server, sizeof(server)) == -1)
		{
			utility::print_info("Error bind: ", WSAGetLastError());
			close();
			return;
		}

		if (listen(listenFD, 20) == -1)
		{
			utility::print_info("Error listen: ", WSAGetLastError());
			close();
			return;
		}

		struct sockaddr_storage client_addr;
		socklen_t size = sizeof(client_addr);

		// Setup select to be non blocking
		int bytesRead = -1;
		fd_set readfds;
		struct timeval tv;
		tv.tv_usec = 200000;
		tv.tv_sec = 0;
		
		// sniff the port until there is data -> client is waiting to be accepted
		// This makes non blocking waiting for client -> accept() is hard blocking syscall
		while (bytesRead <= 0)
		{
			FD_ZERO(&readfds);
			FD_SET(listenFD, &readfds);
			bytesRead = select((listenFD + 1), &readfds, NULL, NULL, &tv);

			// Abort flag in case we shut down application
			if (abort == true)
			{
				close();
				return;
			}
		}

		clientFD = accept(listenFD, (struct sockaddr*)&client_addr, &size);
		if (clientFD == -1)
		{
			utility::print_info("Error accept: ", WSAGetLastError());
			close();
			return;
		}
		else
		{
			utility::print_info("Connected sucessfuly: ", 1);
		}

		closesocket(listenFD);
		buffer = new char[BUFF_SIZE];
		isConnected = true;
	}

	void close()
	{
#ifdef _WIN32
		closesocket(listenFD);
		WSACleanup();
#else 
		close(sockFD);
#endif
		isConnected = false;
	}

	int sendData(const char* data, const size_t length)
	{
		size_t rem, done = 0;
		int len = 0;
		rem = length;

		// Send data till the whole "data" array was sent
		while (length > done)
		{
			len = send(clientFD, data + done, rem, 0);
			done += len;
			rem = length - done;

			if (len == -1) return -1;
		}

		return done;
	}

	// Blocking
	int recvData(std::string& data)
	{
		int done = recv(clientFD, buffer, BUFF_SIZE, 0);
		if (done == -1) return -1;

		data.append(buffer, done);
		return done;
	}

	// Blocking until certain amount
	int recvData(std::string& data, int length)
	{
		int done = recv(clientFD, buffer, length, 0);
		if (done == -1) return -1;

		data.append(buffer, done);
		return done;
	}

	// Non blocking
	int recvDataPacket(std::string& data)
	{
		int len = 0;
		size_t done = 0;

		// Peek if data incoming
		fd_set readfds;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(clientFD, &readfds);
		len = select(clientFD + 1, &readfds, NULL, NULL, &tv);
		// return if no data are waiting, or if error
		if (len == 0)	return 0;
		else if (len == -1)	return -1;

		// Read until we get minimum of the frame size
		while (done < (MIN_FRAME_BYTE_SIZE-1))
		{
			// TODO: Tady musim precist jen data jednoho framu, takze v recvData musim cist min, treba jen par bajtu
			// A nebo to musim delat nejak jinak.. je to docela problem, takze musim stanovit v katherine, 
			// ze se nebudou odesilat data, ktera jsou kratsi nez 100 bajtu!!
			len = recvData(data, 2);
			if (len == -1) return -1;
			else if (len == 0)	// Client disconnected when len == 0
			{
				isConnected = false;
				close();
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
			len = recv(clientFD, buffer + done, rem, 0);
			done += len;
			rem = length - done;

			if (len == -1) return -1;
		}

		data.append(buffer + doneBefore, done - doneBefore);	// append remaning data from buffer
		return done;
	}

	// Utility function for sendign the dta immediately back - for tests
	int loopback()
	{
		int status = 0;

		// Peek if data incoming
		fd_set readfds;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10;
		FD_ZERO(&readfds);
		FD_SET(clientFD, &readfds);
		status = select(clientFD + 1, &readfds, NULL, NULL, &tv);
		// return if no data are waiting, or if error
		if (status == 0)	return 0;
		else if (status == -1)	return -1;

		while ((status = select(clientFD + 1, &readfds, NULL, NULL, &tv)) > 0)
		{
			status = recv(clientFD, buffer, BUFF_SIZE, 0);
			if (status == -1)
			{
				utility::print_info("Error receiving:", WSAGetLastError());
				close();
				return -1;
			}

			status = send(clientFD, buffer, status, 0);
			if (status == -1)
			{
				utility::print_info("Error sending:", WSAGetLastError());
				close();
				return -1;
			}

			// Reset select() variables - they may change with select()
			tv.tv_sec = 0;
			tv.tv_usec = 10;
			FD_ZERO(&readfds);
			FD_SET(clientFD, &readfds);
		}

		if (status == -1) return -1;

		return 0;
	}

private:
	size_t BUFF_SIZE = 10000;
	char* buffer;

	void WSA_init()
	{
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			fprintf(stderr, "WSAStartup failed.\n");
			exit(1);
		}

		if (LOBYTE(wsaData.wVersion) != 2 ||
			HIBYTE(wsaData.wVersion) != 2)
		{
			fprintf(stderr, "Versiion 2.2 of Winsock is not available.\n");
			WSACleanup();
			exit(2);
		}
	}
};