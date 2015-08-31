// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "TcpChannel.h"
#pragma comment(lib, "ws2_32.lib")

class MyTcpListener : public TcpListener
{
public:
	virtual void OnConnected(TcpChannel* channel, bool success)
	{
		int i;
		i = 0;
	}
	virtual void OnBufferReceived(TcpChannel* channel, const char* input_data, int input_data_len)
	{
		int i;
		i = 0;
	}
	virtual void OnError(TcpChannel* channel, DWORD err)
	{
		int i;
		i = 0;
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsd;

	int rc;

	// Load Winsock
	rc = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (rc != 0) {
		assert(0);
		return 1;
	}

	MyTcpListener listener;
	IOThread thread;
	TcpChannel chanel(&listener, &thread);

	thread.Start();


	chanel.Connect("127.0.0.1", 33333);

	thread.Wait(INFINITE);

	WSACleanup();

	return 0;
}

