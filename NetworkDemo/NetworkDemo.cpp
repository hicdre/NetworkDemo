// NetworkDemo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <assert.h>
#include "TcpChanel.h"

#pragma comment(lib, "ws2_32.lib")

class MyTcpListener : public TcpListener
{
public:
	virtual void OnConnected(bool success)
	{
		int i; 
		i = 0;
	}
	virtual void OnBufferReceived(const char* input_data, int input_data_len)
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
	TcpChanel chanel(&listener, &thread);

	thread.Start();


	chanel.Connect("180.97.33.107", 80);

	thread.Wait(INFINITE);

	WSACleanup();

	return 0;
}

