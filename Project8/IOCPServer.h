#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include <thread>
#include <deque>
const int PCK_BUFSIZ = 1500;
enum class socketType {
	UDP,
	TCP,
};

enum class SOCK_DIR{
	RECV,
	SND,
};
struct MyOverlapped {
	OVERLAPPED overlapped;
	SOCKET socket;
	WSABUF wsaBuf;
	char dataBuf[PCK_BUFSIZ];
	SOCK_DIR dir;
	char IP[32];

};
struct SessionInfo {
	SOCKET socket;
	
	MyOverlapped SndOverlapped;
	MyOverlapped RcvOverlapped;

	SessionInfo() {
		memset(&SndOverlapped, 0, sizeof(SndOverlapped));
		memset(&RcvOverlapped, 0, sizeof(RcvOverlapped));
		SndOverlapped.IP[0] = { 0, };
		RcvOverlapped.IP[0] = { 0, };
	}
};

class IOCPServer
{
public:
	void InitServer(socketType type);
	void UpdateServer();
	void DestroyThread();
private:
	void InitSocket(socketType type);
	void ErrHandling(const char* pErr);
	HANDLE CreateIOCPHandle(DWORD dwReleaseThreadListElemCount);
	void CreateWorkerThread();
	static u_int CALLBACK WorkerThread(VOID* pvoid);
	void ListnAndBindSocket();
	void AcceptThread();
	void AddClientIndq(MyOverlapped& over);
	BOOL ConnectIOCPtoDevice(HANDLE socket, HANDLE iocp, DWORD completionKey);
	void RecvFromClient();
	
private:
	HANDLE m_hIOCP;

	SOCKET m_listnSocket;
	
	enum class PORT{
		PORT_NUM =32000,
	};

	std::deque<SessionInfo> m_dqSession;

	size_t m_currentConnectedClientCount;

	std::thread* acceptThread;
};

