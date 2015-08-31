#pragma once
#include "TcpChannel.h"

class TcpServerListener
{
public:
	virtual ~TcpServerListener() {}
	virtual void OnConnected(TcpChannel* chanel) = 0;
	virtual void OnBufferReceived(TcpChannel* chanel, const char* input_data, int input_data_len) = 0;
	virtual void OnError(DWORD err) = 0;
};


class TcpServer : public IOThread::IOHandler, public TcpListener
{
public:
	TcpServer(TcpServerListener* listener, IOThread* thread);
	~TcpServer();

	void Close();

	bool Listen(unsigned port);

	virtual void OnIOCompleted(IOThread::IOContext* context, DWORD bytes_transfered,
		DWORD error) override;

	virtual void OnConnected(TcpChannel* channel, bool success) override;
	virtual void OnBufferReceived(TcpChannel* channel, const char* input_data, int input_data_len) override;
	virtual void OnError(TcpChannel* channel, DWORD err) override;
private:	
	struct State {
		explicit State(TcpServer* channel);
		~State();
		IOThread::IOContext context;
		bool is_pending;
	};

	struct TcpChanelWrapper : public TcpListener{
		TcpChannel* channel;

	};

	enum ReadState { READ_SUCCEEDED, READ_FAILED, READ_PENDING };

	void CreateSocket();
	void Accept();

	SOCKET CreatePeddingSocket();
	bool ProcessConnetion(int bytes_read);

	State accept_state_;

	SOCKET socket_;
	IOThread* thread_;

	SOCKET accept_socket_;

	TcpServerListener* listener_;

	static const size_t kAcceptBufferSize = 1024;
	char accept_buf_[kAcceptBufferSize];

	std::vector<TcpChannel*> channels_;

};