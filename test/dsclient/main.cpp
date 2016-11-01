/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#include "dsclientbase.h"

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
	*buf = uv_buf_init((char*)POOLED_ALLOC(suggested_size), suggested_size);
}

void on_connect(uv_connect_t* connection, int status);
void on_stream_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_stream_close(uv_handle_t* handle);


struct uvIOHandler
{
	uv_connect_t	m_connection;
	uv_tcp_t		m_socket;

	struct _Writer
	{
		void*	buf;
		size_t	len;
		DSCPP::LPFN_SEND_COMPLETE cb;
		inline _Writer(void* argbuf, size_t arglen, DSCPP::LPFN_SEND_COMPLETE argcb): cb(argcb), buf(argbuf), len(arglen)
		{ 
			this->write_req.data = this;
		}
		uv_write_t write_req;
	};

	///@param buf the data that need to be sent. memory is owned and managed by caller
	///@param len the size of buf to be sent
	///@param cb the callback on completion. Called with buf and len as parameters
	int send(void* buf, size_t len, DSCPP::LPFN_SEND_COMPLETE cb = release_send_buffer)
	{
		uv_stream_t* stream = m_connection.handle;		// same as m_socket
		_Writer* writer = _NEW3(_Writer, buf, len, cb);	// gets deleted in on_send_done()
		uv_buf_t bufs[] = { uv_buf_init((char*)buf, len) };
		return uv_write(&writer->write_req, stream, bufs, 1, on_send_done);
	}
	static void on_send_done(uv_write_t* write_req, int status)
	{
		_Writer* writer = (_Writer*)write_req->data;
		// call the completion callback
		(*writer->cb)(writer->buf, writer->len);
		// free the memory allocated in the send()
		_DELETE(writer);
		// Notes: write_req->handle == m_socket / stream. 
		// You can use uv_close(write_req->handle,[](){}) here to close the socket and end the uv_run() loop.
	}
	// allocates a buffer that has to be owned and managed by the caller
	static inline void* alloc_send_buffer(size_t size)
	{
		return POOLED_ALLOC(size);
	}
	static inline void release_send_buffer(void* buf, size_t s = 0)
	{
		POOLED_FREE(buf);	// buf should have been allocated with alloc_send_buffer()
	}
	int recv(char* buf, size_t buflen)
	{
		return fgets(buf, buflen, stdin) != nullptr ? 0 : -1;
	}
	int disconnect()
	{
		uv_read_stop(m_connection.handle); // m_connection.handle is same as m_socket
		if (!uv_is_closing((uv_handle_t*)m_connection.handle))
			uv_close((uv_handle_t*)m_connection.handle, on_stream_close);
		return 0;
	}
};

class _dsclientUVDriver : public DSCPP::_dsclientBase<uvIOHandler, DSCPP::simpleCredentialsSupplier>
{
protected:
	uv_loop_t*		m_uvLoop = nullptr;

public:
	~_dsclientUVDriver()
	{
		stop();
	}
	int connect(const char* szServer = "127.0.0.1", int nPort = 6021, const char* szUsername = "userA", const char* szPassword = "password", int nTimeoutDelay = 60)
	{
		strUsername = szUsername;
		strPassword = szPassword;

		m_uvLoop = uv_default_loop();

		m_connection.data = this;

		struct sockaddr_in dest;
		if (uv_ip4_addr(szServer, nPort, &dest) < 0 ||
			uv_tcp_init(m_uvLoop, &m_socket) < 0 ||
			uv_tcp_keepalive(&m_socket, 1, nTimeoutDelay) < 0 ||
			uv_tcp_connect(&m_connection, &m_socket, (const struct sockaddr*)&dest, on_connect) < 0)
			return -1;

		return 0;
	}

	int run()
	{
		return uv_run(m_uvLoop, UV_RUN_DEFAULT);
	}
	void stop()
	{
		disconnect();
		if (m_uvLoop != nullptr)
		{
			uv_loop_close(m_uvLoop);
			m_uvLoop = nullptr;
		}
	}
};

void on_connect(uv_connect_t* connection, int status)
{
	if (status < 0)
		return;	// some error happened, just return (not sure if we have to clean this uv_connection stuff)

	_dsclientUVDriver* pdscUV = (_dsclientUVDriver*)connection->data;

	uv_stream_t* stream = connection->handle;	// this stream is nothing but the _dsclientUVDriver::m_socket
	uv_stream_set_blocking(stream, false);
	stream->data = pdscUV;

	pdscUV->register_rpc_provider("echo", [](DSCPP::_rpcCall& sz) {
		std::cerr << "echo called";
		return 0;
	});

	/* Start reading */
	int r = uv_read_start(stream, alloc_cb, on_stream_read);
}

void on_stream_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	if (nread >= 0)
	{
		std::cout << "\nread done: " << buf->base;
		_dsclientUVDriver* pdscUV = (_dsclientUVDriver*) stream->data;
		pdscUV->handle_server_directive(_dsclientUVDriver::unique_bufptr(buf->base), nread); // buf->base is allocated through alloc_cb(), will be owned by handle_server_directive()
	}
	else
	{
		std::cout << "\nSocket Read Failure: connection lost with server";
		uv_close((uv_handle_t*)stream, on_stream_close); // closes the stream and stops the uv_run (endgame !!); this stream == _dsclientUVDriver::m_socket;
		if (buf->base != nullptr && buf->len > 0) 
			POOLED_FREE(buf->base); // this was allocated through alloc_cb() by libuv from uv_read_start()
	}
}

void on_stream_close(uv_handle_t* handle)
{
	// nothing to delete here because the socket (==handle) is stack allocated (m_socket)
}

_dsclientUVDriver gClientDriver;

void interrupt_handler(int sig)
{
	std::cerr << "\n Received Interrupt. Stopping the server loop";
	gClientDriver.stop();
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

int main()
{
	if (gClientDriver.connect() >= 0)
	{
		setup_signal_handlers();
		gClientDriver.run();
		std::cerr << "\nClient loop stopped";
	}
	else
		std::cerr << "\nCould not establish connection";
}