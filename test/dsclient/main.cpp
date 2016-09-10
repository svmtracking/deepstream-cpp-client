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

	int send(const char* buf, size_t len)
	{
		uv_stream_t* stream = m_connection.handle; // same as m_socket
		uv_write_t* write_req = _NEW(uv_write_t);
		write_req->data = (void*)buf;
		uv_buf_t bufs[] = { uv_buf_init((char*)buf, len) };
		return uv_write(write_req, stream, bufs, 1, on_send_done);
	}
	static void on_send_done(uv_write_t* write_req, int status)
	{
		_DELETE(write_req);
		POOLED_FREE(write_req->data);	// this was allocated inside alloc_send_buffer() by _dsclientBase
		// write_req->handle == m_socket / stream. 
		// close it with uv_close(write_req->handle,[](){}) to close the socket and end the uv_run().
	}
	void* alloc_send_buffer(size_t size)
	{
		return POOLED_ALLOC(size);
	}
	int recv(char* buf, size_t buflen)
	{
		return fgets(buf, buflen, stdin) != nullptr ? 0 : -1;
	}
	int disconnect()
	{
		uv_read_stop(m_connection.handle);
		if (!uv_is_closing((uv_handle_t*)m_connection.handle))
			uv_close((uv_handle_t*)m_connection.handle, on_stream_close);
		return 0;
	}
};

class _dsclientUVDriver : public DSCPP::_dsclientBase<DSCPP::HandlerJunction<uvIOHandler, DSCPP::simpleCredentialsSupplier>>
{
protected:
	uv_loop_t*		m_uvLoop = nullptr;

public:
	~_dsclientUVDriver()
	{
		disconnect();
		if (m_uvLoop != nullptr)
		{
			uv_loop_close(m_uvLoop);
			m_uvLoop = nullptr;
		}
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

};

void on_connect(uv_connect_t* connection, int status)
{
	if (status < 0)
		return;	// some error happened, just return (not sure if we have to clean this uv_connection stuff)

	_dsclientUVDriver* pdscUV = (_dsclientUVDriver*)connection->data;

	uv_stream_t* stream = connection->handle;	// this stream is nothing but the _dsclientUVDriver::m_socket
	uv_stream_set_blocking(stream, false);
	stream->data = pdscUV;

	pdscUV->register_rpc_provider("echo", [](const char* sz) {
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
		pdscUV->handle_server_directive(buf->base, nread);
	}
	else
	{
		std::cout << "\nSocket Read Failure: connection lost with server";
		uv_close((uv_handle_t*)stream, on_stream_close); // closes the stream and stops the uv_run (endgame !!); this stream == _dsclientUVDriver::m_socket;
	}
	if (buf->base != nullptr && buf->len > 0) POOLED_FREE(buf->base); // this was allocated through alloc_cb() by libuv from uv_read_start()
}

void on_stream_close(uv_handle_t* handle)
{
}

int main()
{
	_dsclientUVDriver clientDriver;
	if (clientDriver.connect() >= 0)
	{
		clientDriver.run();
		std::cerr << "\nClient loop stopped";
	}
	else
		std::cerr << "\nCould not establish connection";
}