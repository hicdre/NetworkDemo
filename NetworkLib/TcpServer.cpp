#include "stdafx.h"
#include "TcpServer.h"
#include <MSWSock.h>
#include <assert.h>

static bool GetAcceptEx(SOCKET sock, LPFN_ACCEPTEX& pFunc)
{
	DWORD dwBytes;
	int rc;

	/* Dummy socket needed for WSAIoctl */

	{
		GUID guid = WSAID_ACCEPTEX;
		rc = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&pFunc, sizeof(pFunc),
			&dwBytes, NULL, NULL);
		if (rc != 0)
			return false;
	}
	return true;
}

static LPFN_ACCEPTEX myAcceptEx = NULL;
static LPFN_GETACCEPTEXSOCKADDRS myGetAcceptExSockaddrs = NULL;

static bool GetAcceptExSockaddrs(SOCKET sock, LPFN_GETACCEPTEXSOCKADDRS& pFunc)
{
	DWORD dwBytes;
	int rc;

	/* Dummy socket needed for WSAIoctl */

	{
		GUID guid = WSAID_GETACCEPTEXSOCKADDRS;
		rc = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&pFunc, sizeof(pFunc),
			&dwBytes, NULL, NULL);
		if (rc != 0)
			return false;
	}
	return true;
}

TcpServer::State::State(TcpServer* channel)
	: is_pending(false)
{
	memset(&context.overlapped, 0, sizeof(context.overlapped));
	context.handler = channel;
}

TcpServer::State::~State()
{

}

TcpServer::TcpServer(TcpServerListener* listener, IOThread* thread)
	: socket_(INVALID_SOCKET)
	, listener_(listener)
	, thread_(thread)
	, accept_state_(this)
{
	memset(accept_buf_, 0, sizeof(accept_buf_));
	CreateSocket();
}

TcpServer::~TcpServer()
{
	Close();
}

void TcpServer::Close()
{

}

bool TcpServer::Listen(unsigned port)
{
	struct sockaddr_in addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(port);
	int rc = bind(socket_, (SOCKADDR*)&addr, sizeof(addr));
	assert(rc == 0);


	listen(socket_, SOMAXCONN);
	
	if (!GetAcceptEx(socket_, myAcceptEx) || !GetAcceptExSockaddrs(socket_, myGetAcceptExSockaddrs))
		return false;

	Accept();
	return true;
}

void TcpServer::OnIOCompleted(IOThread::IOContext* context, DWORD bytes_transfered, DWORD error)
{
	bool ok = true;
	if (context == &accept_state_.context)
	{
		assert(accept_state_.is_pending);
		accept_state_.is_pending = false;
		ProcessConnetion(bytes_transfered);
	}
}

void TcpServer::CreateSocket()
{
	socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert(socket_ != INVALID_SOCKET);

	thread_->RegisterIOHandler((HANDLE)socket_, this);
}

SOCKET TcpServer::CreatePeddingSocket()
{
	return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void TcpServer::Accept()
{
	if (myAcceptEx == NULL)
		return;
	

	accept_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	DWORD dwRead;
	BOOL ret = myAcceptEx(socket_, accept_socket_,
		accept_buf_, 0,
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
		&dwRead, &accept_state_.context.overlapped);
	if (!ret)
	{
		DWORD err = GetLastError();
		if (err == WSA_IO_PENDING) {
			accept_state_.is_pending = true;
		}
		else {
			assert(0);
		}
		
		return;
	}
	
	accept_state_.is_pending = true;
	
}

bool TcpServer::ProcessConnetion(int bytes_read)
{
// 	assert(myGetAcceptExSockaddrs);
// 	myGetAcceptExSockaddrs(accept_buf_, bytes_read,
// 		kAcceptBufferSize - ((sizeof(sockaddr_in) + 16) * 2),
// 		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, )

	setsockopt(accept_socket_,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&socket_,
		sizeof(socket_));

	TcpChannel* newChannel = new TcpChannel(accept_socket_, this, thread_);
	channels_.push_back(newChannel);
	newChannel->ProcessIncomingMessages();


	accept_socket_ = NULL;
	Accept();
	return true;	
}

void TcpServer::OnConnected(TcpChannel* channel, bool success)
{
	//ÎÞÓÃ
}

void TcpServer::OnBufferReceived(TcpChannel* channel, const char* input_data, int input_data_len)
{
	listener_->OnBufferReceived(channel, input_data, input_data_len);
}

void TcpServer::OnError(TcpChannel* channel, DWORD err)
{

}

