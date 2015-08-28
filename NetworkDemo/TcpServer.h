#pragma once
#include "TcpChanel.h"

class TcpServerListener
{
public:
	virtual ~TcpServerListener() {}
	virtual void OnConnected(TcpChanel* chanel) = 0;
	virtual void OnBufferReceived(TcpChanel* chanel, const char* input_data, int input_data_len) = 0;
	virtual void OnError(DWORD err) = 0;
};


class TcpServer : public IOThread::IOHandler
{
public:
	TcpServer(TcpServerListener* listener, IOThread* thread);
	~TcpServer();

	void Close();

	bool Listen(unsigned port);

	virtual void OnIOCompleted(IOThread::IOContext* context, DWORD bytes_transfered,
		DWORD error) override;
private:
	struct State {
		explicit State(TcpServer* channel);
		~State();
		IOThread::IOContext context;
		bool is_pending;
	};

	enum ReadState { READ_SUCCEEDED, READ_FAILED, READ_PENDING };

	void CreateSocket();

	SOCKET CreatePeddingSocket();

	State accept_state_;

	SOCKET socket_;
	IOThread* thread_;

	SOCKET pedding_socket_;

	TcpServerListener* listener_;
};