#pragma once
#pragma comment(lib, "ws2_32")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>

class IOCPServer
{
public:
	IOCPServer(void) {}

	virtual ~IOCPServer(void)
	{
		// 윈도우 소켓(Windows Socket API)의 사용을 끝낸다.
		WSACleanup();
	}

	// 소켓을 초기화하는 함수
	bool InitSocket()
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[에러] WSAStartup()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		// 연결지향형 TCP, Overlapped I/O 소켓을 생성
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket)
		{
			printf("[에러] socket()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		printf("소켓 초기화 성공\n");
		return true;
	}

	bool Init(const UINT32 maxIOWorkerThreadCount_)
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[에러] WSAStartup()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		// 연결지향형 TCP, Overlapped I/O 소켓을 생성
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket)
		{
			printf("[에러] socket()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		MaxIOWorkerThreadCount = maxIOWorkerThreadCount_;

		printf("소켓 초기화 성공\n");
		return true;
	}

	// 서버의 주소정보를 소켓과 연결시키고 접속 요청을 받기 위해 소켓을 등록하는 함수
	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort);			// 서버 포트 설정 
																// 만약, 한 아이피에서만 접속을 받는다면, inet_addr함수를 통해 해당 주소 넣기
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);	// 어떤 IP에서든 접속을 받는다.

		// 위에서 지정한 서버 주소 정보와 cIOCOmpletionPort 소켓을 연결
		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (0 != nRet)
		{
			printf("[에러] bind()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		// 접속 요청을 받아들이기 위해서 -> cIOCompletionPort 소켓을 등록, 접속 대기 큐를 생성(5개)
		nRet = listen(mListenSocket, 5);
		if (nRet != 0)
		{
			printf("[에러] listen()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		//CompletionPort객체 생성 요청을 한다.
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MaxIOWorkerThreadCount);
		if (NULL == mIOCPHandle)
		{
			printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
			return false;
		}

		auto hIOCPHandle = CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0);
		if (nullptr == hIOCPHandle)
		{
			printf("[에러] listen socket IOCP bind 실패 : %d\n", WSAGetLastError());
			return false;
		}

		printf("서버 등록 성공..\n");
		return true;
	}

	// 접속 요청 수락 후 메세지 받아서 처리하는 함수 
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		//// CompletionPort 객체 생성 요청
		//mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		//if (NULL == mIOCPHandle)
		//{
		//	printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
		//	return false;
		//}

		bool bRet = CreateWokerThread(); // 접속 된 클라 주소 정보를 저장하는 구조체
		if (false == bRet) {
			return false;
		}

		bRet = CreateAccepterThread();
		if (false == bRet) {
			return false;
		}

		printf("서버 시작\n");
		return true;
	}

	// 생성되어있는 스레드 파괴 
	void DestroyThread()
	{
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThreads)
		{
			if (th.joinable())
			{
				th.join();
			}
		}

		// Accepter Thread 종료
		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}
	}

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(sessionIndex_); // 누가 보냈는지 GetClientInfo로 알아오고
		return pClient->SendMsg(dataSize_, pData); // 실질적으로 I/O Send를 하는 부분은 이곳이다.
	}

	// 네트워크 이벤트를 처리할 함수들 (가상함수) : 상속 받는 곳(Client)에서 재정의할 수 있도록 가상함수로 선언
	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}


private:
	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; ++i)
		{
			auto client = new stClientInfo();
			client->Init(i, mIOCPHandle); //

			mClientInfos.push_back(client);
		}
	}

	// WaitingThread Queue에서 대기할 스레드 생성
	bool CreateWokerThread()
	{
		unsigned int uiThreadId = 0;
		mIsWorkerRun = true;

		// WaingThread Queue에 대기 상태로 넣을 스레드의 생성 
														// 권장되는 개수 : (cpu 수 * 2) + 1 
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() { WokerThread(); });
		}

		printf("WokerThread 시작..\n");
		return true;
	}

	// accept 요청을 처리하는 스레드 생성
	bool CreateAccepterThread()
	{
		mIsAccepterRun = true;
		mAccepterThread = std::thread([this]() { AccepterThread(); });

		printf("AccepterThread 시작..\n");
		return true;
	}

	// send 처리하는 스레드 생성
	void CreateSendThread()
	{
		mIsSenderRun = true;
		mSendThread = std::thread([this]() { SendThread(); });
		printf("SendThread 시작..\n");
	}

	// 사용하지 않는 클라이언트 정보 구조체 반환
	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client->IsConnectd() == false)
			{
				return client;
			}
		}

		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 sessionIndex)
	{
		return mClientInfos[sessionIndex];
	}


	// Overlapped I/O작업에 대한 완료 통보를 받아 그에 해당하는 처리를 하는 함수 
	// (네트워크 IO 처리를 전담하는 스레드)
	void WokerThread()
	{
		stClientInfo* pClientInfo = nullptr; // CompletionKey 받을 포인터 변수
		BOOL bSuccess = TRUE; // 함수 호출 성공 여부
		DWORD dwIoSize = 0; // 전송된 데이터 크기
		LPOVERLAPPED lpOverlapped = NULL; // IO 작업을 위해 요청한 Overlapped 구조체를 받을 포인터

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,					// 실제로 전송된 바이트
				(PULONG_PTR)&pClientInfo,		// CompletionKey
				&lpOverlapped,				// Overlapped IO 객체
				INFINITE);					// 대기할 시간

			//사용자 쓰레드 종료 메세지 처리..
			if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			// client가 접속을 끊었을 때 
			//if (bSuccess == FALSE || (dwIoSize == 0 && bSuccess == TRUE))
			if (bSuccess == FALSE || (dwIoSize == 0 && IOOperation::ACCEPT != pOverlappedEx->m_eOperation))
			{
				//printf("socket(%d) 접속 끊김\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			// Overlapped I/O Accept 작업 결과 뒤 처리
			if (IOOperation::ACCEPT == pOverlappedEx->m_eOperation)
			{
				pClientInfo = GetClientInfo(pOverlappedEx->SessionIndex);
				if (pClientInfo->AcceptCompletion())
				{
					//클라이언트 갯수 증가
					++mClientCnt;

					OnConnect(pClientInfo->GetIndex());
				}
				else
				{
					CloseSocket(pClientInfo, true);
				}
			}

			// Overlapped I/O Recv 작업 결과 뒤 처리
			else if (IOOperation::RECV == pOverlappedEx->m_eOperation)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());

				pClientInfo->BindRecv();
			}

			// Overlapped I/O Send 작업 결과 뒤 처리
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) // Send 완료가 되었을 때. 
			{
				pClientInfo->SendCompleted(dwIoSize); // Send 작업 완료시 호출되는 함수

				//delete[] pOverlappedEx->m_wsaBuf.buf;  // 3단계(IOCPServer.h)와 달라진 점. 동적 할당한 메모리를 Delete 해제해준다.
				//delete pOverlappedEx;
				//pClientInfo->SendCompleted(dwIoSize);
			}
			// 예외 상황
			else
			{
				printf("Client Index(%d)에서 예외상황\n", pClientInfo->GetIndex());
			}
		}
	}

	//// 사용자의 접속을 받는 스레드 
	//void AccepterThread()
	//{
	//	SOCKADDR_IN	stClientAddr;
	//	int nAddrLen = sizeof(SOCKADDR_IN);

	//	while (mIsAccepterRun)
	//	{
	//		// 접속을 받을 구조체의 인덱스 Get
	//		stClientInfo* pClientInfo = GetEmptyClientInfo();
	//		if (NULL == pClientInfo)
	//		{
	//			printf("[에러] Client Full\n");
	//			return;
	//		}

	//		// 클라이언트 접속 요청이 들어올 때까지 기다림
	//		auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);

	//		if (INVALID_SOCKET == newSocket)
	//		{
	//			continue;
	//		}

	//		//I/O Completion Port객체와 소켓을 연결시킨다.
	//		if (pClientInfo->OnConnect(mIOCPHandle, newSocket) == false)
	//		{
	//			pClientInfo->Close(true);
	//			return;
	//		}

	//		//Recv Overlapped I/O작업을 요청해 놓는다.

	//		OnConnect(pClientInfo->GetIndex()); // 클라이언트 접속시(즉 연결 시) 호출되는 함수 = 호출되면서 EchoServer의 OnConnect()함수가 호출된다. = 그러면서 새로운 연결이 발생한 것을 알 수 있다.

	//		//클라이언트 갯수 증가
	//		++mClientCnt;
	//	}
	//}
	//사용자의 접속을 받는 쓰레드
	void AccepterThread()
	{
		while (mIsAccepterRun)
		{
			auto curTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			for (auto client : mClientInfos)
			{
				if (client->IsConnectd())
				{
					continue;
				}

				if ((UINT64)curTimeSec < client->GetLatestClosedTimeSec())
				{
					continue;
				}

				auto diff = curTimeSec - client->GetLatestClosedTimeSec();
				if (diff <= RE_USE_SESSION_WAIT_TIMESEC)
				{
					continue;
				}

				client->PostAccept(mListenSocket, curTimeSec);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(32));
		}
	}

	// Send 처리하는 스레드
	void SendThread()
	{
		while (mIsSenderRun)
		{
			for (auto client : mClientInfos)
			{
				if (client->IsConnectd() == false)
				{
					continue;
				}

				client->SendIO();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		}
	}


	// 소켓의 연결을 종료 시킨다.
	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->GetIndex();

		pClientInfo->Close(bIsForce);

		OnClose(clientIndex);
	}

	UINT32 MaxIOWorkerThreadCount = 0;

	// 클라이언트 정보 저장 구조체
	std::vector<stClientInfo*> mClientInfos;

	// 클라이언트의 접속을 받기위한 리슨 소켓
	SOCKET mListenSocket = INVALID_SOCKET;

	// 접속 되어있는 클라이언트 수
	int	mClientCnt = 0;

	// IO Worker 스레드
	std::vector<std::thread> mIOWorkerThreads;

	// Accept 스레드
	std::thread	mAccepterThread;

	std::thread	mSendThread;


	// CompletionPort 객체 핸들
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	// 작업 스레드 동작 플래그
	bool mIsWorkerRun = true;

	// 접속 스레드 동작 플래그
	bool mIsAccepterRun = true;

	// Send 스레드 동작 플래그
	bool mIsSenderRun = false;
};