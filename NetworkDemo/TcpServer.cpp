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

	{
		LPFN_ACCEPTEX myAcceptEx;
		if (!GetAcceptEx(socket_, myAcceptEx))
			return false;
		//----------------------
		// Connect to server.
		//::connect(socket_, (SOCKADDR *) & clientService, sizeof (clientService));
		if (TRUE == myAcceptEx(socket_, (SOCKADDR *)&clientService, sizeof(clientService), NULL, 0, NULL,
			&connect_state_.context.overlapped)) {
			connected_ = true;
			return true;
		}
	}
}

void TcpServer::OnIOCompleted(IOThread::IOContext* context, DWORD bytes_transfered, DWORD error)
{

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

