#include "IOCPServer.h"
#include <iostream>
#pragma comment (lib,"ws2_32.lib")
using namespace std;


void IOCPServer::InitServer(socketType type)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		ErrHandling("WSAStartup Error!");
	}
	InitSocket(type);
	m_hIOCP = CreateIOCPHandle(0);
	if (m_hIOCP == NULL) {
		ErrHandling("Ceate IOCP Handle Err");
	}
	CreateWorkerThread();
	ListnAndBindSocket();

	acceptThread = new std::thread([&]() {AcceptThread(); });
}

void IOCPServer::UpdateServer()
{
	while (true) {

		RecvFromClient();

	}
}

void IOCPServer::DestroyThread()
{
	// _beginThreadEx로 만들어진 스레드는 알아서 회수됨

	closesocket(m_listnSocket);
	if (acceptThread->joinable()) {
		acceptThread->join();
	}
}

void IOCPServer::InitSocket(socketType type)
{
	if (type == socketType::TCP)
		m_listnSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
	else
		m_listnSocket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (m_listnSocket == INVALID_SOCKET) {
		ErrHandling("init listnsocket error!");
	}
}

void IOCPServer::ErrHandling(const char* pErr)
{
	if (WSAGetLastError() != 0) {
		cout << "WSA Error No : " << WSAGetLastError() << endl;
	}
	cout << pErr << endl;
	exit(1);
}

HANDLE IOCPServer::CreateIOCPHandle(DWORD dwReleaseThreadListElemCount)
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwReleaseThreadListElemCount);
}

void IOCPServer::CreateWorkerThread()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	size_t threadCount = sysInfo.dwNumberOfProcessors * 2 + 1;
	for (int i = 0; i < threadCount; i++) {
		_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, NULL);
	}


}

u_int IOCPServer::WorkerThread(VOID* pvoid)
{
	HANDLE iocp = (HANDLE)pvoid;
	DWORD recvBytes = 0;
	DWORD completeKey = 0;
	SessionInfo sessionInfo;
	while (true) {

		if (GetQueuedCompletionStatus(iocp, &recvBytes, &completeKey, (LPOVERLAPPED*)(&sessionInfo), INFINITE) == FALSE)
		{
			closesocket(sessionInfo.socket);
			ZeroMemory(&sessionInfo.RcvOverlapped.overlapped, sizeof(OVERLAPPED));
			ZeroMemory(&sessionInfo.RcvOverlapped.wsaBuf, sizeof(sessionInfo.RcvOverlapped.wsaBuf));
			CloseHandle(iocp);
		}

		cout << "클라이언트의 메시지 : " << sessionInfo.RcvOverlapped.dataBuf << "  [전송된 Bytes] : " <<
			recvBytes << endl;

		// send과정 필요시 넣으면 에코서버처럼 동작

	}


	return u_int();
}

void IOCPServer::ListnAndBindSocket()
{
	sockaddr_in listn_addr;
	memset(&listn_addr, 0, sizeof(listn_addr));
	listn_addr.sin_family = AF_INET;
	listn_addr.sin_port = htons(static_cast<int>(PORT::PORT_NUM));
	listn_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(m_listnSocket, (sockaddr*)&listn_addr, sizeof(listn_addr)) == SOCKET_ERROR) {
		ErrHandling("socket bind error!");
	}
	
	if (listen(m_listnSocket, SOMAXCONN) == SOCKET_ERROR) {
		ErrHandling("socket listen error!");
	}
}

void IOCPServer::AcceptThread()
{
	// session이 dq에 들어감.. thread 작업임
	sockaddr_in client_addr;
	int addr_size = sizeof(client_addr);
	m_currentConnectedClientCount = 0;
	
	while (true) {
		
		SOCKET client = accept(m_listnSocket, (sockaddr*)&client_addr, &addr_size);
		if (client == INVALID_SOCKET) {
			ErrHandling("socket accept error!");
		}

		MyOverlapped overlapped;
		memset(&overlapped.overlapped, 0, sizeof(overlapped.overlapped));
		overlapped.wsaBuf.buf = overlapped.dataBuf;
		overlapped.wsaBuf.len = PCK_BUFSIZ;
		overlapped.socket = client;
		
		if (ConnectIOCPtoDevice((HANDLE)(&client), m_hIOCP, 0) == FALSE) {
			ErrHandling("Connect IOCP to Device Error!");
		}
		
		inet_ntop(AF_INET, &client_addr.sin_addr.S_un.S_addr, overlapped.IP, 32 - 1);
		AddClientIndq(overlapped);

		m_currentConnectedClientCount++;

	}
}

void IOCPServer::AddClientIndq(MyOverlapped& over)
{
	SessionInfo session;
	session.RcvOverlapped = over;
	session.socket = over.socket;
	m_dqSession.push_back(session);
}

BOOL IOCPServer::ConnectIOCPtoDevice(HANDLE socket, HANDLE iocp, DWORD completionKey)
{
	HANDLE handle = CreateIoCompletionPort(socket, iocp, completionKey, 0);
	return handle != NULL ? TRUE : FALSE;
}

void IOCPServer::RecvFromClient()
{
	if (m_dqSession.empty() == false) return;
	DWORD receiveBytes = 0;
	DWORD flags = 0;
	for (size_t i = 0; i < m_dqSession.size(); i++) {
		SessionInfo s = m_dqSession[i];
		
		if (WSARecv(s.socket, &s.RcvOverlapped.wsaBuf, 1, &receiveBytes, &flags, &s.RcvOverlapped.overlapped
			, NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				ErrHandling("WSARecv Error!");
			}
		}
	}
}

