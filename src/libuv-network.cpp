/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#include "dsclientbase.h"
#include "bufPool.h"
#include "trie_array.h"
#include "g2log-timer.h"

#if defined(WIN32) || defined(_WIN32)
#define close(a) closesocket(a)
#endif

inline std::string getTimestamp()
{
	std::tm t1 = g2::localtime(g2::systemtime_now());
	return g2::put_time(&t1, "%c");
}

#include <map>
#include <algorithm>
#include <vector>



enum Constants { RECV_BUF_SIZE = 8192, HTTP_MAX_HEADER_COUNT = 24  };

struct _Config
{
	bool nSocketNonBlocking = true;      // O_NONBLOCK option
	bool nSocketReuse = true;            // SO_REUSEADDR option
	int nListenBacklog = 16;            // backlog for the listen socket

	int nClientTimeout = 15000;			// client timeout in milliseconds

	int nRecvBufSize = 8192;			// socket receive buffer size 8kb (should be large enough to hold URL + Headers including cookies etc. all)
	int nSendBufSize = 8192;			// socket send buffer size

	int nMaxURLLen = 256;				// maximum URI Path in the HTTP Header
} gConfigOptions;

inline void set_nonblocking(SOCKET sockfd, bool option = gConfigOptions.nSocketNonBlocking)
{
#ifdef _WIN32
	u_long mode = option;
	ioctlsocket(sockfd, FIONBIO, &mode);
#else
	int flags = fcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, option ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

inline void set_addrreuse(SOCKET sockfd, bool option = gConfigOptions.nSocketReuse)
{
	int val = option;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val));
}

SOCKET server_listen(const char* szHost, int port)
{
	SOCKET result = INVALID_SOCKET;
	struct addrinfo hints, *pAddrInfo = NULL;
	char buf[64];

	/* Get Address Information from the system */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	sprintf(buf, "%d", port);

	int err = getaddrinfo(szHost, buf, &hints, &pAddrInfo);
	if (!err)
	{
		SOCKET listenFd = socket(pAddrInfo->ai_family, pAddrInfo->ai_socktype, pAddrInfo->ai_protocol);
		// Make socket as non-blocking
		set_nonblocking(listenFd);
		// set SO_REUSEADDR so that it can be bound without waiting for any previously bound sockets to be closed
		set_addrreuse(listenFd);
		// Bind and Listen for Servers
		if (!bind(listenFd, pAddrInfo->ai_addr, pAddrInfo->ai_addrlen) && !listen(listenFd, gConfigOptions.nListenBacklog))
		{
			result = listenFd;
		}
	}
	if (pAddrInfo) freeaddrinfo(pAddrInfo);
	return result;
}

struct _knownStrings
{
	enum { MAX_HTTPVERB_STRLEN = 16 };
	enum VERBS { VERB_CONNECT=0, VERB_DELETE, VERB_GET, VERB_HEAD, VERB_OPTIONS, VERB_POST, VERB_PUT, VERB_TRACE };
	trie_prefixed_hash http_verbs;

	inline _knownStrings()
	{
		//Note: SPACE is important after the verb (for prefix matching correctly). This list should match the enum VERBS above
		const char* verbs[] = { "CONNECT ", "DELETE ", "GET ", "HEAD ", "OPTIONS ", "POST ", "PUT ", "TRACE " }; 
		http_verbs.hash(verbs, sizeof(verbs) / sizeof(verbs[0]));
	}
} gKnownStrings;

struct _client
{
	SOCKET		clientFd;
	uv_poll_t*	clientPoll;
	uv_timer_t* timer;

	inline _client(SOCKET clientFd, uv_loop_t* serverLoop)
	{
		this->clientFd = clientFd;
		this->clientPoll = _NEW(uv_poll_t);
		uv_poll_init_socket(serverLoop, clientPoll, clientFd);
		uv_poll_start(clientPoll, UV_READABLE, onReadable);
		clientPoll->data = (void*) this;

		timer = _NEW(uv_timer_t);
		uv_timer_init(serverLoop, timer);
		uv_timer_start(timer, onTimeout, gConfigOptions.nClientTimeout, 0);
		timer->data = (void*) this;
	}
	inline ~_client()
	{
		uv_poll_stop(clientPoll);
		uv_close((uv_handle_t *)clientPoll, [](uv_handle_t *handle) {
			_DELETE((uv_poll_t *)handle);
		});

		uv_timer_stop(timer);
		uv_close((uv_handle_t *)timer, [](uv_handle_t *handle) {
			_DELETE((uv_timer_t *)handle);
		});

		close(this->clientFd);
	}
	static void onTimeout(uv_timer_t* pTimer)
	{
		_client* pClient = (_client*)pTimer->data;
		_DELETE(pClient);
	}
	static void onReadable(uv_poll_t* clientPoll, int status, int events)
	{
		_client* pClient = (_client*)clientPoll->data;
		if (status < 0) { _DELETE(pClient); return; }

		char data[Constants::RECV_BUF_SIZE];
		int size = recv(pClient->clientFd, data, Constants::RECV_BUF_SIZE, 0);
		if (size <= 0) 
		{
			if (size == 0 || errno != EWOULDBLOCK)  /* Handle disconnect */
			{
				_DELETE(pClient);
				return;
			}
			else // no more data
				return;
		}

		size_t keyLen = _knownStrings::MAX_HTTPVERB_STRLEN;
		if(gKnownStrings.http_verbs.prefixMatch(data, keyLen) < 0) 
		{
			char error[] = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
			send(pClient->clientFd, error, strlen(error), 0);
			_DELETE(pClient);
			return;
		}

		char* pURI = data + keyLen;
		char* pSpace = (char*)std::memchr(pURI, ' ', gConfigOptions.nMaxURLLen + 8);
		if (pSpace == nullptr)
		{
			char error[] = "HTTP/1.1 414 URI Too Long\r\n\r\n";
			send(pClient->clientFd, error, strlen(error), 0);
			_DELETE(pClient);
			return;
		}		

		data[size] = 0;
		printf("\n %s ", data);

		char http_response[] =
			"HTTP/1.1 200 OK\n"
			"Date: Thu, 19 Feb 2009 12:27:04 GMT\n"
			"Server: Apache/2.2.3\n"
			"Last-Modified: Wed, 18 Jun 2003 16:05:58 GMT\n"
			"ETag: \"56d-9989200-1132c580\"\n"
			"Content-Type: text/html\n"
			"Content-Length: 15\n"
			"Accept-Ranges: bytes\n"
			"Connection: close\n"
			"\n"
			"Hello World!!  ";

		send(pClient->clientFd, http_response, strlen(http_response), 0);

	}
};


struct _server
{
	uv_loop_t* uvLoop = nullptr;       // the main default loop
	SOCKET listenFd = INVALID_SOCKET;  // the listening socket that accepts clients
	uv_poll_t* listenPoll = nullptr;   // the poll handler for listening 

	inline ~_server()
	{
		stop();
	}
	inline void stop()
	{
		if (listenPoll)
		{
			uv_poll_stop(listenPoll);
			uv_close((uv_handle_t *)listenPoll, [](uv_handle_t *handle) { });
		}
		if (listenFd != INVALID_SOCKET)
		{
			close(listenFd);
			listenFd = INVALID_SOCKET;
		}
		if (uvLoop) 
		{
			uv_loop_close(uvLoop); uvLoop = nullptr;
		}
		if (listenPoll)
		{
			_DELETE((uv_poll_t *)listenPoll);
			listenPoll = nullptr;
		}
	}
} gServer;

uv_poll_t* start_accepting(SOCKET listenFd, uv_loop_t* uvLoop, uv_poll_cb acceptHandler)
{
	auto listenPoll = _NEW(uv_poll_t);
	uv_poll_init_socket(uvLoop, listenPoll, listenFd);
	uv_poll_start(listenPoll, UV_READABLE, acceptHandler);
	return listenPoll;
}

void acceptHandler(uv_poll_t *listenPoll, int status, int events)
{
	if (status < 0) return;
	
	_server* pServer = (_server*)listenPoll->data;
	SOCKET clientFd = accept(pServer->listenFd, NULL, NULL);
	if (clientFd == INVALID_SOCKET) return;

#ifdef __APPLE__
	int noSigpipe = 1;
	setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(int));
#endif
	// create a new client object that takes care of itself. 
	_NEW2(_client, clientFd, pServer->uvLoop);	// this memory is self deleted by the class
}

void interrupt_handler(int sig)
{
	std::cerr << "\n[" << getTimestamp() << "] Received Interrupt. Stopping the server loop";
	gServer.stop();
}
void setup_signal_handlers(void)
{
	signal(SIGINT, &interrupt_handler);
	signal(SIGTERM, &interrupt_handler);
#if defined(WIN32) || defined(_WIN32)
#else
	/* Stops the SIGPIPE signal being raised when writing to a closed socket */
	signal(SIGPIPE, SIG_IGN);
#endif
}


/* sample main function for server */
void serverMain()
{
	setup_signal_handlers();

	gServer.uvLoop = uv_default_loop();
	gServer.listenFd = server_listen(NULL, 8080);
	gServer.listenPoll = start_accepting(gServer.listenFd, gServer.uvLoop, acceptHandler);
	gServer.listenPoll->data = (void*)&gServer;

	uv_run(gServer.uvLoop, UV_RUN_DEFAULT);

	std::cerr << "\n[" << getTimestamp() << "] Server loop stopped";
}