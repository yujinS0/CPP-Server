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
		// ������ ����(Windows Socket API)�� ����� ������.
		WSACleanup();
	}

	// ������ �ʱ�ȭ�ϴ� �Լ�
	bool InitSocket()
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[����] WSAStartup()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		// ���������� TCP, Overlapped I/O ������ ����
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket)
		{
			printf("[����] socket()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		printf("���� �ʱ�ȭ ����\n");
		return true;
	}

	// ������ �ּ������� ���ϰ� �����Ű�� ���� ��û�� �ޱ� ���� ������ ����ϴ� �Լ�
	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort);			// ���� ��Ʈ ���� 
																// ����, �� �����ǿ����� ������ �޴´ٸ�, inet_addr�Լ��� ���� �ش� �ּ� �ֱ�
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);	// � IP������ ������ �޴´�.

		// ������ ������ ���� �ּ� ������ cIOCOmpletionPort ������ ����
		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (0 != nRet)
		{
			printf("[����] bind()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		// ���� ��û�� �޾Ƶ��̱� ���ؼ� -> cIOCompletionPort ������ ���, ���� ��� ť�� ����(5��)
		nRet = listen(mListenSocket, 5);
		if (0 != nRet)
		{
			printf("[����] listen()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		printf("���� ��� ����..\n");
		return true;
	}

	// ���� ��û ���� �� �޼��� �޾Ƽ� ó���ϴ� �Լ� 
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		// CompletionPort ��ü ���� ��û
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}

		bool bRet = CreateWokerThread();
		if (false == bRet) {
			return false;
		}

		bRet = CreateAccepterThread();
		if (false == bRet) {
			return false;
		}

		printf("���� ����\n");
		return true;
	}

	// �����Ǿ��ִ� ������ �ı� 
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

		// Accepter Thread ����
		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}
	}

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(sessionIndex_); // ���� ���´��� GetClientInfo�� �˾ƿ���
		return pClient->SendMsg(dataSize_, pData); // ���������� I/O Send�� �ϴ� �κ��� �̰��̴�.
	}

	// ��Ʈ��ũ �̺�Ʈ�� ó���� �Լ��� (�����Լ�) : ��� �޴� ��(Client)���� �������� �� �ֵ��� �����Լ��� ����
	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}


private:
	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; ++i)
		{
			mClientInfos.emplace_back();

			mClientInfos[i].Init(i);
		}
	}

	// WaitingThread Queue���� ����� ������ ����
	bool CreateWokerThread()
	{
		//unsigned int uiThreadId = 0;
		mIsWorkerRun = true;

		// WaingThread Queue�� ��� ���·� ���� �������� ���� 
														// ����Ǵ� ���� : (cpu �� * 2) + 1 
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() { WokerThread(); });
		}

		printf("WokerThread ����..\n");
		return true;
	}

	// accept ��û�� ó���ϴ� ������ ����
	bool CreateAccepterThread()
	{
		mIsAccepterRun = true;
		mAccepterThread = std::thread([this]() { AccepterThread(); });

		printf("AccepterThread ����..\n");
		return true;
	}

	// send ó���ϴ� ������ ����
	void CreateSendThread()
	{
		mIsSenderRun = true;
		mSendThread = std::thread([this]() { SendThread(); });
		printf("SendThread ����..\n");
	}

	// ������� �ʴ� Ŭ���̾�Ʈ ���� ����ü ��ȯ
	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client.IsConnectd() == false)
			{
				return &client;
			}
		}

		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 sessionIndex)
	{
		return &mClientInfos[sessionIndex];
	}


	// Overlapped I/O�۾��� ���� �Ϸ� �뺸�� �޾� �׿� �ش��ϴ� ó���� �ϴ� �Լ� 
	// (��Ʈ��ũ IO ó���� �����ϴ� ������)
	void WokerThread()
	{
		stClientInfo* pClientInfo = nullptr; // CompletionKey ���� ������ ����
		BOOL bSuccess = TRUE; // �Լ� ȣ�� ���� ����
		DWORD dwIoSize = 0; // ���۵� ������ ũ��
		LPOVERLAPPED lpOverlapped = NULL; // IO �۾��� ���� ��û�� Overlapped ����ü�� ���� ������

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,					// ������ ���۵� ����Ʈ
				(PULONG_PTR)&pClientInfo,		// CompletionKey
				&lpOverlapped,				// Overlapped IO ��ü
				INFINITE);					// ����� �ð�

			//����� ������ ���� �޼��� ó��..
			if (TRUE == bSuccess && 0 == dwIoSize && NULL == lpOverlapped)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (NULL == lpOverlapped)
			{
				continue;
			}

			// client�� ������ ������ �� 
			if (FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess))
			{
				//printf("socket(%d) ���� ����\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}


			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			// Overlapped I/O Recv �۾� ��� �� ó��
			if (IOOperation::RECV == pOverlappedEx->m_eOperation)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());

				pClientInfo->BindRecv();
			}
			// Overlapped I/O Send �۾� ��� �� ó��
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) // Send �Ϸᰡ �Ǿ��� ��. 
			{
				pClientInfo->SendCompleted(dwIoSize); // Send �۾� �Ϸ�� ȣ��Ǵ� �Լ�

				//delete[] pOverlappedEx->m_wsaBuf.buf;  // 3�ܰ�(IOCPServer.h)�� �޶��� ��. ���� �Ҵ��� �޸𸮸� Delete �������ش�.
				//delete pOverlappedEx;
				//pClientInfo->SendCompleted(dwIoSize);
			}
			// ���� ��Ȳ
			else
			{
				printf("Client Index(%d)���� ���ܻ�Ȳ\n", pClientInfo->GetIndex());
			}
		}
	}

	// ������� ������ �޴� ������ 
	void AccepterThread()
	{
		SOCKADDR_IN	stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun)
		{
			// ������ ���� ����ü�� �ε��� Get
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (NULL == pClientInfo)
			{
				printf("[����] Client Full\n");
				return;
			}

			// Ŭ���̾�Ʈ ���� ��û�� ���� ������ ��ٸ�
			auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);

			if (INVALID_SOCKET == newSocket)
			{
				continue;
			}

			//I/O Completion Port��ü�� ������ �����Ų��.
			if (pClientInfo->OnConnect(mIOCPHandle, newSocket) == false)
			{
				pClientInfo->Close(true);
				return;
			}

			//Recv Overlapped I/O�۾��� ��û�� ���´�.

			OnConnect(pClientInfo->GetIndex()); // Ŭ���̾�Ʈ ���ӽ�(�� ���� ��) ȣ��Ǵ� �Լ� = ȣ��Ǹ鼭 EchoServer�� OnConnect()�Լ��� ȣ��ȴ�. = �׷��鼭 ���ο� ������ �߻��� ���� �� �� �ִ�.

			//Ŭ���̾�Ʈ ���� ����
			++mClientCnt;
		}
	}

	// Send ó���ϴ� ������
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


	// ������ ������ ���� ��Ų��.
	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->GetIndex();

		pClientInfo->Close(bIsForce);

		OnClose(clientIndex);
	}



	// Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<stClientInfo> mClientInfos;

	// Ŭ���̾�Ʈ�� ������ �ޱ����� ���� ����
	SOCKET mListenSocket = INVALID_SOCKET;

	// ���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
	int	mClientCnt = 0;

	// IO Worker ������
	std::vector<std::thread> mIOWorkerThreads;

	// Accept ������
	std::thread	mAccepterThread;

	std::thread	mSendThread;


	// CompletionPort ��ü �ڵ�
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	// �۾� ������ ���� �÷���
	bool mIsWorkerRun = true;

	// ���� ������ ���� �÷���
	bool mIsAccepterRun = true;

	// Send ������ ���� �÷���
	bool mIsSenderRun = false;
};