#pragma once

#include "IOThread.h"



class TcpListener
{
public:
	virtual ~TcpListener() {}
	virtual void OnConnected(bool success) = 0;
	virtual void OnBufferReceived(const char* input_data, int input_data_len) = 0;
	virtual void OnError(DWORD err) = 0;
};

class TcpChanel : public IOThread::IOHandler
{
public:
	TcpChanel(TcpListener* listener, IOThread* thread);
	TcpChanel(SOCKET socket, TcpListener* listener, IOThread* thread);
	~TcpChanel();

	void Close();

	bool Connect(const char* addr, unsigned port);
	bool Disconnect();
	bool Send(IOBuffer* buffer);

	virtual void OnIOCompleted(IOThread::IOContext* context, DWORD bytes_transfered,
		DWORD error) override;
private:
	struct State {
		explicit State(TcpChanel* channel);
		~State();
		IOThread::IOContext context;
		bool is_pending;
	};	
	enum ReadState { READ_SUCCEEDED, READ_FAILED, READ_PENDING };

	void CreateSocket();

	bool ProcessOutgoingMessages(IOThread::IOContext* context,
		DWORD bytes_written);

	bool ProcessIncomingMessages();
	bool AsyncReadComplete(int bytes_read);
	ReadState InternalReadData(char* buffer, int buffer_len, int* bytes_read);
	bool DispatchInputData(const char* input_data, int input_data_len);

	State input_state_;
	State output_state_;
	State connect_state_;
	State disconnect_state_;
	bool connected_;
	bool processing_incoming_;

	SOCKET socket_;
	IOThread* thread_;

	std::queue<IOBuffer*> output_queue_;
	TcpListener* listener_;

	static const size_t kReadBufferSize = 4 * 1024;
	char input_buf_[kReadBufferSize];
};