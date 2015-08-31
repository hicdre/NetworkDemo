// MyConsoleApplication.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "TcpServer.h"
#pragma comment(lib, "ws2_32.lib")

class MyTcpListener : public TcpServerListener
{
public:
	virtual void OnConnected(TcpChanel* chanel)
	{
		int i;
		i = 0;
	}
	virtual void OnBufferReceived(TcpChanel* chanel, const char* input_data, int input_data_len)
	{
		int i;
		i = 0;
	}
	virtual void OnError(DWORD err)
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
	TcpServer server(&listener, &thread);

	thread.Start();


	server.Listen(33333);

	thread.Wait(INFINITE);

	WSACleanup();

	return 0;
}

