#pragma once

//NOTE: much of the sockets code herein is based on http-tweak's single-header http server
// see: https://github.com/ixchow/http-tweak

//--------- OS-specific socket-related headers ---------
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1 //so we can use strerror()
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#undef APIENTRY
#include <winsock2.h>
#include <ws2tcpip.h> //for getaddrinfo
#undef max
#undef min

#pragma comment(lib, "Ws2_32.lib") //link against the winsock2 library

#define MSG_DONTWAIT 0 //on windows, sockets are set to non-blocking with an ioctl
typedef int ssize_t;

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>

#define closesocket close
typedef int SOCKET;
constexpr const SOCKET INVALID_SOCKET = -1;
#endif
//--------- ---------------------------------- ---------

#include <vector>
#include <list>
#include <string>
#include <functional>

//Thin wrapper around a (polling-based) TCP socket connection:
struct Connection {
	//Helper that will append any type to the send buffer:
	template< typename T >
	void send(T const &t) {
		send_raw(&t, sizeof(T));
	}
	//Helper that will append raw bytes to the send buffer:
	void send_raw(void const *data, size_t size) {
		send_buffer.insert(send_buffer.end(), reinterpret_cast< uint8_t const * >(data), reinterpret_cast< uint8_t const * >(data) + size);
	}

	//Call 'close' to mark a connection for discard:
	void close() {
		if (socket != INVALID_SOCKET) {
			::closesocket(socket);
			socket = INVALID_SOCKET;
		}
	}

	//so you can if(connection) ... to check for validity:
	explicit operator bool() { return socket != INVALID_SOCKET; }

	//To send data over a connection, append it to send_buffer:
	std::vector< uint8_t > send_buffer;
	//When the connection receives data, it is appended to recv_buffer:
	std::vector< uint8_t > recv_buffer;

	//internals:
	SOCKET socket = INVALID_SOCKET;

	enum Event {
		OnOpen,
		OnRecv,
		OnClose
	};
};

struct Server {
	Server(std::string const &port); //pass the port number to listen on, as a string (servname, really)

	//poll() updates the list of active connections and provides information to your callbacks:
	void poll(
		std::function< void(Connection *, Connection::Event event) > const &connection_event = nullptr,
		double timeout = 0.0 //timeout (seconds)
	);

	std::list< Connection > connections;
	SOCKET listen_socket = INVALID_SOCKET;
};


struct Client {
    Connection* connect(std::string const &host, std::string const &port);

	//poll() checks the status of the active connection and provides information to your callbacks:
	void poll(
		std::function< void(Connection *, Connection::Event event) > const &connection_event = nullptr,
		double timeout = 0.0 //timeout (seconds)
	);

	std::list< Connection > connections;
};
