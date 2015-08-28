#include "stdafx.h"
#include "TcpChanel.h"
#include <MSWSock.h>
#include <assert.h>

// #ifndef LPFN_CONNECTEX
// typedef
// BOOL
// (PASCAL FAR * LPFN_CONNECTEX) (
// 							   IN SOCKET s,
// 							   IN const struct sockaddr FAR *name,
// 							   IN int namelen,
// 							   IN PVOID lpSendBuffer OPTIONAL,
// 							   IN DWORD dwSendDataLength,
// 							   OUT LPDWORD lpdwBytesSent,
// 							   IN LPOVERLAPPED lpOverlapped
// 							   );
// 
// #define WSAID_CONNECTEX \
// {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
// 
// typedef
// BOOL
// (PASCAL FAR * LPFN_DISCONNECTEX) (
// 								  IN SOCKET s,
// 								  IN LPOVERLAPPED lpOverlapped,
// 								  IN DWORD  dwFlags,
// 								  IN DWORD  dwReserved
// 								  );
// 
// #define WSAID_DISCONNECTEX \
// {0x7fda2e11,0x8630,0x436f,{0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}}
// #endif

static bool GetConnectEx(SOCKET sock, LPFN_CONNECTEX& pFunc)
{
	DWORD dwBytes;
	int rc;

	/* Dummy socket needed for WSAIoctl */
	
	{
		GUID guid = WSAID_CONNECTEX;
		rc = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&pFunc, sizeof(pFunc),
			&dwBytes, NULL, NULL);
		if (rc != 0)
			return false;
	}
	return true;
}

static bool GetDisConnectEx(SOCKET sock, LPFN_DISCONNECTEX& pFunc)
{
	DWORD dwBytes;
	int rc;

	/* Dummy socket needed for WSAIoctl */

	{
		GUID guid = WSAID_DISCONNECTEX;
		rc = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&pFunc, sizeof(pFunc),
			&dwBytes, NULL, NULL);
		if (rc != 0)
			return false;
	}
	return true;
}
TcpChanel::TcpChanel( TcpListener* listener, IOThread* thread )
: socket_(INVALID_SOCKET)
, listener_(listener)
, thread_(thread)
, connected_(false)
, input_state_(this)
, output_state_(this)
, connect_state_(this)
, disconnect_state_(this)
, processing_incoming_(false)
{
	memset(input_buf_, 0, sizeof(input_buf_));
	CreateSocket();
}

TcpChanel::TcpChanel(SOCKET socket, TcpListener* listener, IOThread* thread)
: socket_(socket)
, listener_(listener)
, thread_(thread)
, connected_(false)
, input_state_(this)
, output_state_(this)
, connect_state_(this)
, disconnect_state_(this)
, processing_incoming_(false)
{
	thread_->RegisterIOHandler((HANDLE)socket_, this);
}

TcpChanel::~TcpChanel()
{
	Close();
}

void TcpChanel::Close()
{
	if (input_state_.is_pending || output_state_.is_pending)
		CancelIo((HANDLE)socket_);

	// Closing the handle at this point prevents us from issuing more requests
	// form OnIOCompleted().
	if (socket_ != INVALID_SOCKET) {
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
	}

	// Make sure all IO has completed.
	//base::Time start = base::Time::Now();
	while (input_state_.is_pending || output_state_.is_pending) {
		thread_->WaitForIOCompletion(INFINITE, this);
	}

	while (!output_queue_.empty()) {
		IOBuffer* m = output_queue_.front();
		output_queue_.pop();
		m->Release();
	}
}

bool TcpChanel::Connect(const char* addr, unsigned port)
{
	if (socket_ == INVALID_SOCKET)
		return false;

	sockaddr_in clientService;
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = inet_addr(addr);
	clientService.sin_port = htons(port);
	
	LPFN_CONNECTEX myConnectEx;
	if (!GetConnectEx(socket_, myConnectEx))
		return false;
	//----------------------
	// Connect to server.
	//::connect(socket_, (SOCKADDR *) & clientService, sizeof (clientService));
	if (TRUE == myConnectEx(socket_, (SOCKADDR *)&clientService, sizeof(clientService), NULL, 0, NULL, 
		&connect_state_.context.overlapped)) {
			connected_ = true;
			return true;
	}		

	DWORD err = WSAGetLastError();
	if (err == ERROR_IO_PENDING) {
		connect_state_.is_pending = true;
		return true;
	}

	return false;
}

bool TcpChanel::Disconnect()
{
	if (socket_ == INVALID_SOCKET)
		return true;

	LPFN_DISCONNECTEX myDisConnectEx;
	if (!GetDisConnectEx(socket_, myDisConnectEx))
		return false;

	if (TRUE == myDisConnectEx(socket_, &disconnect_state_.context.overlapped, TF_REUSE_SOCKET, NULL))
	{
		connected_ = false;
		return true;
	}

	DWORD err = WSAGetLastError();
	if (err == ERROR_IO_PENDING) {
		input_state_.is_pending = true;
		return true;
	} else if (err == WSAENOTCONN) {
		connected_ = false;
		return true;
	}

	return false;
}

bool TcpChanel::Send( IOBuffer* buffer )
{
	buffer->AddRef();
	output_queue_.push(buffer);

	if (connected_){
		if (!output_state_.is_pending) {
			if (!ProcessOutgoingMessages(NULL, 0))
				return false;
		}
	}
	return true;
}

void TcpChanel::CreateSocket()
{
	socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert(socket_ != INVALID_SOCKET);

	thread_->RegisterIOHandler((HANDLE)socket_, this);

	{
		struct sockaddr_in addr;
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = 0;
		int rc = bind(socket_, (SOCKADDR*)&addr, sizeof(addr));
		assert(rc == 0);
	}
}

void TcpChanel::OnIOCompleted( IOThread::IOContext* context, DWORD bytes_transfered, DWORD error )
{
	bool ok = true;
	//assert(thread_check_->CalledOnValidThread());
	if (context == &connect_state_.context 
		|| context == &input_state_.context )
	{
		if (context == &connect_state_.context)
		{
			if (error == 0) {
				connected_ = true;
				listener_->OnConnected(true);
			}
			else {
				listener_->OnConnected(false);
				return;
			}

			//连接成功
			if (!output_queue_.empty() && !output_state_.is_pending)
				ProcessOutgoingMessages(NULL, 0);
		}
		
		// We don't support recursion through OnMessageReceived yet!
		assert(!processing_incoming_);
		assert(connected_);
		processing_incoming_ = true;

		// Process the new data.
		if (input_state_.is_pending) {
			// This is the normal case for everything except the initialization step.
			input_state_.is_pending = false;
			if (!bytes_transfered)
				ok = false;
			else if (socket_ != INVALID_SOCKET)
				ok = AsyncReadComplete(bytes_transfered);
		} else {
			assert(!bytes_transfered);
		}

		// Request more data.
		if (ok)
			ok = ProcessIncomingMessages();

		processing_incoming_ = false;
	} else {
		assert(context == &output_state_.context);
		ok = ProcessOutgoingMessages(context, bytes_transfered);
	}
	if (!ok && INVALID_SOCKET != socket_) {
		// We don't want to re-enter Close().
		Close();
		listener_->OnError(error);
		//listener()->OnChannelError();
	}
}

bool TcpChanel::ProcessOutgoingMessages( IOThread::IOContext* context, DWORD bytes_written )
{
	assert(connected_);  // Why are we trying to send messages if there's
	// no connection?
	//DCHECK(thread_check_->CalledOnValidThread());

	if (output_state_.is_pending) {
		assert(context);
		output_state_.is_pending = false;
		if (!context || bytes_written == 0) {
			DWORD err = GetLastError();
			//LOG(ERROR) << "pipe error: " << err;
			return false;
		}
		// Message was sent.
		assert(!output_queue_.empty());
		IOBuffer* m = output_queue_.front();
		output_queue_.pop();
		m->Release();
	}

	if (output_queue_.empty())
		return true;

	if (INVALID_SOCKET == socket_)
		return false;

	// Write to pipe...
	IOBuffer* m = output_queue_.front();
	assert(m->GetLen() <= INT_MAX);
	WSABUF buf;
	buf.buf = const_cast<char*>(m->GetBuffer());
	buf.len = m->GetLen();
	int ret = ::WSASend(socket_, &buf, 1,
		NULL, 0,
		&output_state_.context.overlapped, NULL);
	if (ret) {
		DWORD err = GetLastError();
		if (err == WSA_IO_PENDING) {
			output_state_.is_pending = true;

			//DVLOG(2) << "sent pending message @" << m << " on channel @" << this
			//         << " with type " << m->type();

			return true;
		}
		//LOG(ERROR) << "pipe error: " << err;
		return false;
	}

	//DVLOG(2) << "sent message @" << m << " on channel @" << this
	//         << " with type " << m->type();

	output_state_.is_pending = true;
	return true;
}

bool TcpChanel::ProcessIncomingMessages()
{
	while (true) {
		int bytes_read = 0;
		ReadState read_state = InternalReadData(input_buf_, kReadBufferSize,
			&bytes_read);
		if (read_state == READ_FAILED)
			return false;
		if (read_state == READ_PENDING)
			return true;

		assert(bytes_read > 0);
		if (!DispatchInputData(input_buf_, bytes_read))
			return false;
	}
}

bool TcpChanel::AsyncReadComplete( int bytes_read )
{
	return DispatchInputData(input_buf_, bytes_read);
}

TcpChanel::ReadState TcpChanel::InternalReadData( char* buffer, int buffer_len, int* bytes_read )
{
	if (INVALID_SOCKET == socket_)
		return READ_FAILED;
	
	DWORD flags = 0;
	WSABUF buf;
	buf.buf = buffer;
	buf.len = buffer_len;
	int ret = ::WSARecv(socket_, &buf, 1,
		NULL, &flags, &input_state_.context.overlapped, NULL);
	if (ret) {
		DWORD err = GetLastError();
		if (err == WSA_IO_PENDING) {
			input_state_.is_pending = true;
			return READ_PENDING;
		}
		//LOG(ERROR) << "pipe error: " << err;
		return READ_FAILED;
	}

	// We could return READ_SUCCEEDED here. But the way that this code is
	// structured we instead go back to the message loop. Our completion port
	// will be signalled even in the "synchronously completed" state.
	//
	// This allows us to potentially process some outgoing messages and
	// interleave other work on this thread when we're getting hammered with
	// input messages. Potentially, this could be tuned to be more efficient
	// with some testing.
	input_state_.is_pending = true;
	return READ_PENDING;
}

bool TcpChanel::DispatchInputData( const char* input_data, int input_data_len )
{
	listener_->OnBufferReceived(input_data, input_data_len);
	return true;
}



TcpChanel::State::State( TcpChanel* channel )
: is_pending(false)
{
	memset(&context.overlapped, 0, sizeof(context.overlapped));
	context.handler = channel;
}

TcpChanel::State::~State()
{

}
