//��ó: �����ߴ��� ���� '�¶��� ���Ӽ���'����
#pragma once
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#define MAX_SOCKBUF 1024	//��Ŷ ũ��
#define MAX_WORKERTHREAD 4  //������ Ǯ�� ���� ������ ��

enum class IOOperation
{
	RECV,
	SEND
};

//WSAOVERLAPPED����ü�� Ȯ�� ���Ѽ� �ʿ��� ������ �� �־���.
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;		//Overlapped I/O����ü
	SOCKET		m_socketClient;			//Ŭ���̾�Ʈ ����
	WSABUF		m_wsaBuf;				//Overlapped I/O�۾� ����
	char		m_szBuf[MAX_SOCKBUF]; //������ ����
	IOOperation m_eOperation;			//�۾� ���� ����
};

//Ŭ���̾�Ʈ ������ ������� ����ü
struct stClientInfo
{
	SOCKET			m_socketClient;			//Cliet�� ����Ǵ� ����
	stOverlappedEx	m_stRecvOverlappedEx;	//RECV Overlapped I/O�۾��� ���� ����
	stOverlappedEx	m_stSendOverlappedEx;	//SEND Overlapped I/O�۾��� ���� ����

	stClientInfo()
	{
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};


class IOCompletionPort
{
public:
	IOCompletionPort(void) {}

	~IOCompletionPort(void)
	{
		//������ ����� ������.
		WSACleanup();
	}

	//������ �ʱ�ȭ�ϴ� �Լ�
	bool InitSocket()
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[����] WSAStartup()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		//���������� TCP , Overlapped I/O ������ ����
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket)
		{
			printf("[����] socket()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		printf("���� �ʱ�ȭ ����\n");
		return true;
	}


	//------������ �Լ�-------//
	//������ �ּ������� ���ϰ� �����Ű�� ���� ��û�� �ޱ� ���� 
	//������ ����ϴ� �Լ�
	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN		stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort); //���� ��Ʈ�� �����Ѵ�.		
		//� �ּҿ��� ������ �����̶� �޾Ƶ��̰ڴ�.
		//���� ������� �̷��� �����Ѵ�. ���� �� �����ǿ����� ������ �ް� �ʹٸ�
		//�� �ּҸ� inet_addr�Լ��� �̿��� ������ �ȴ�.
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		//������ ������ ���� �ּ� ������ cIOCompletionPort ������ �����Ѵ�.
		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (0 != nRet)
		{
			printf("[����] bind()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		//���� ��û�� �޾Ƶ��̱� ���� cIOCompletionPort������ ����ϰ� 
		//���Ӵ��ť�� 5���� ���� �Ѵ�.
		nRet = listen(mListenSocket, 5);
		if (0 != nRet)
		{
			printf("[����] listen()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		printf("���� ��� ����..\n");
		return true;
	}

	//���� ��û�� �����ϰ� �޼����� �޾Ƽ� ó���ϴ� �Լ�
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		// [1]. �Ϸ� ��Ʈ(Completion Port) ����
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD); // windows �����ϴ� api (IOCP ����)
		// INVALID_HANDLE_VALUE : ���ο�	�Ϸ���Ʈ �����Ѵٴ� �ǹ�.
		if (NULL == mIOCPHandle)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}


		bool bRet = CreateWokerThread(); // IOCP �ٷ�� ������ ����
		if (false == bRet) {
			return false;
		}

		bRet = CreateAccepterThread(); // Accepter ó���� �ٷ�� ������ ����
		if (false == bRet) {
			return false;
		}

		printf("���� ����\n");
		return true;
	}

	//�����Ǿ��ִ� �����带 �ı��Ѵ�.
	void DestroyThread() // ���� �� ������� �� �ı�
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

		//Accepter �����带 �����Ѵ�.
		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}
	}


private:
	void CreateClient(const UINT32 maxClientCount) // maxClientCount : ���ڼ����� �ִ�� ���� �� �ִ� Ŭ�� �� 
	{
		for (UINT32 i = 0; i < maxClientCount; ++i)
		{
			mClientInfos.emplace_back(); // ����� remote client�� ����Ű�� ��ü�� poll�� �̸� �����ΰ� �ִ�.
		}
	}

	//WaitingThread Queue���� ����� ��������� ����
	bool CreateWokerThread()
	{
		unsigned int uiThreadId = 0;
		//WaingThread Queue�� ��� ���·� ���� ������� ���� ����Ǵ� ���� : (cpu���� * 2) + 1 
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() { WokerThread(); });	// ������ ó�� !
		}

		printf("WokerThread ����..\n");
		return true;
	}

	//accept��û�� ó���ϴ� ������ ����
	bool CreateAccepterThread() // Accepter �����尡 �񵿱� IO�� �ƴ϶� ����� ó���ϰ� �ִ�. (���� �� �ܰ迡�� �񵿱�� ������ ����)
	{
		mAccepterThread = std::thread([this]() { AccepterThread(); });	// ������ ó�� !

		printf("AccepterThread ����..\n");
		return true;
	}

	//������� �ʴ� Ŭ���̾�Ʈ ���� ����ü�� ��ȯ�Ѵ�.
	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (INVALID_SOCKET == client.m_socketClient)
			{
				return &client;
			}
		}

		return nullptr;
	}

	//CompletionPort��ü�� ���ϰ� CompletionKey�� �����Ű�� ������ �Ѵ�.
	bool BindIOCompletionPort(stClientInfo* pClientInfo) // Bind IO
	{
		// [2]. I/O ��ü�� �Ϸ� ��Ʈ ����
				// ���� ������ �Ϸ� ��Ʈ�� ��Ʈ��ũ �����̳� ���� �ڵ� ���� I/O ��ü�� �����մϴ�. 
				// �� �ܰ迡�� �ش� I/O ��ü�� ���� �񵿱� �۾��� �Ϸ�Ǹ�, �Ϸ� ��Ʈ�� �ڵ����� �˸��� ��ϵ˴ϴ�. 
		//socket�� pClientInfo�� CompletionPort��ü�� �����Ų��.
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient
			, mIOCPHandle
			, (ULONG_PTR)(pClientInfo), 0);

		if (NULL == hIOCP || mIOCPHandle != hIOCP)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}

		return true;
	}

	//WSARecv Overlapped I/O �۾��� ��Ų��.
	bool BindRecv(stClientInfo* pClientInfo)
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stRecvOverlappedEx.m_szBuf;
		pClientInfo->m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		// [3]. �񵿱� I/O �۾� ����
			// I/O ��ü�� ���� �񵿱� �۾��� ����
			// Ŭ���̾�Ʈ�κ��� �����͸� �񵿱������� �����ϴ� ��� WSARecv �Լ�(�񵿱� ������ ������ ���� �Լ�)
		int nRet = WSARecv(pClientInfo->m_socketClient,
			&(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (pClientInfo->m_stRecvOverlappedEx), // ����ü�� ���� �񵿱� �۾� ���� ����?
			NULL);
		// WSARecv�� �񵿱� �۾��� ��û�� �� ��� ��ȯ�Ǹ�, I/O �۾��� �Ϸ�� ������ ������� �ʽ��ϴ�.

	//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSARecv()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	//WSASend Overlapped I/O�۾��� ��Ų��.
	bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen)
	{
		DWORD dwRecvNumBytes = 0;

		//���۵� �޼����� ����
		CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, nLen);


		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = nLen;
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stSendOverlappedEx.m_szBuf;
		pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOperation::SEND;

		int nRet = WSASend(pClientInfo->m_socketClient,
			&(pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx),
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}

	// �� �ڵ忡�� ���� �߿��� ������ �Լ� �κ� ! 
	//Overlapped I/O�۾��� ���� �Ϸ� �뺸�� �޾� 
	//�׿� �ش��ϴ� ó���� �ϴ� �Լ�
	void WokerThread() // IOCP�� ó���ϴ� ��Ŀ ������
	{
		//CompletionKey�� ���� ������ ����
		stClientInfo* pClientInfo = NULL;
		//�Լ� ȣ�� ���� ����
		BOOL bSuccess = TRUE;
		//Overlapped I/O�۾����� ���۵� ������ ũ��
		DWORD dwIoSize = 0;
		//I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			//////////////////////////////////////////////////////
			//�� �Լ��� ���� ��������� WaitingThread Queue��
			//��� ���·� ���� �ȴ�.
			//�Ϸ�� Overlapped I/O�۾��� �߻��ϸ� IOCP Queue����
			//�Ϸ�� �۾��� ������ �� ó���� �Ѵ�.
			//�׸��� PostQueuedCompletionStatus()�Լ������� �����
			//�޼����� �����Ǹ� �����带 �����Ѵ�.
			////////////////////////////////////////////////////// �Ϸ�(����)�� ������ GetQueuedCompletionStatus�� ��� ȣ���ϰ� ����..
			////////////////////////////////////////////////////// ���� �񵿱� �Ϸᰡ �ȴٸ�, ���⼭ � ó�������� ����, ó���ϰ� ����.
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,		// [4]. �۾��� ������ ��� �� ó��
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

			//client�� ������ ��������..			
			if (FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess))
			{
				printf("socket(%d) ���� ����\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}


			stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			//Overlapped I/O Recv�۾� ��� �� ó��
			if (IOOperation::RECV == pOverlappedEx->m_eOperation) // 
			{
				pOverlappedEx->m_szBuf[dwIoSize] = NULL;
				printf("[����] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

				//Ŭ���̾�Ʈ�� �޼����� �����Ѵ�.
				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
				BindRecv(pClientInfo);
			}
			//Overlapped I/O Send�۾� ��� �� ó��
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
			{
				printf("[�۽�] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
			}
			//���� ��Ȳ
			else
			{
				printf("socket(%d)���� ���ܻ�Ȳ\n", (int)pClientInfo->m_socketClient);
			}
		}
	}

	//������� ������ �޴� ������
	void AccepterThread()
	{
		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun)
		{
			//������ ���� ����ü�� �ε����� ���´�.
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (NULL == pClientInfo)
			{
				printf("[����] Client Full\n");
				return;
			}

			//Ŭ���̾�Ʈ ���� ��û�� ���� ������ ��ٸ���.
			pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen); // ������ �߻��ϸ� accept ȣ��
			if (INVALID_SOCKET == pClientInfo->m_socketClient)
			{
				continue;
			}

			//I/O Completion Port��ü�� ������ �����Ų��.
			bool bRet = BindIOCompletionPort(pClientInfo);
			if (false == bRet)
			{
				return;
			}

			//Recv Overlapped I/O�۾��� ��û�� ���´�.
			bRet = BindRecv(pClientInfo);
			if (false == bRet)
			{
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

			//Ŭ���̾�Ʈ ���� ����
			++mClientCnt;
		}
	}

	//������ ������ ���� ��Ų��.
	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER�� ����

		// bIsForce�� true�̸� SO_LINGER, timeout = 0���� �����Ͽ� ���� ���� ��Ų��. ���� : ������ �ս��� ������ ���� 
		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		//socketClose������ ������ �ۼ����� ��� �ߴ� ��Ų��.
		shutdown(pClientInfo->m_socketClient, SD_BOTH);

		//���� �ɼ��� �����Ѵ�.
		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		//���� ������ ���� ��Ų��. 
		closesocket(pClientInfo->m_socketClient);

		pClientInfo->m_socketClient = INVALID_SOCKET;
	}



	//Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<stClientInfo> mClientInfos;

	//Ŭ���̾�Ʈ�� ������ �ޱ����� ���� ����
	SOCKET		mListenSocket = INVALID_SOCKET;

	//���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
	int			mClientCnt = 0;

	//IO Worker ������
	std::vector<std::thread> mIOWorkerThreads;

	//Accept ������
	std::thread	mAccepterThread;

	//CompletionPort��ü �ڵ�
	HANDLE		mIOCPHandle = INVALID_HANDLE_VALUE;

	//�۾� ������ ���� �÷���
	bool		mIsWorkerRun = true;

	//���� ������ ���� �÷���
	bool		mIsAccepterRun = true;
	//���� ����
	char		mSocketBuf[1024] = { 0, };
};