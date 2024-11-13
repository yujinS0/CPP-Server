#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex> // send �۾� �� mutex �ʿ�

// Ŭ���̾�Ʈ ������ ������� ����ü
// ����� Ŭ���̾�Ʈ ���� ������ ���.. ClientInfo Ŭ����
class stClientInfo // �̸� ����
{
public:
	stClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	~stClientInfo() = default;

	void Init(const UINT32 index)
	{
		mIndex = index;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnectd() { return mSock != INVALID_SOCKET; }

	SOCKET GetSock() { return mSock; }

	char* RecvBuffer() { return mRecvBuf; }


	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_)
	{
		mSock = socket_;

		Clear();

		//I/O Completion Port��ü�� ������ �����Ų��.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER�� ����

		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(mSock, SD_BOTH);

		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear()
	{
		mSendPos = 0;
		mIsSending = false;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0); // socket�� pClientInfo�� CompletionPort ��ü�� ����

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		// Overlapped I/O�� ���� ���� ����
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(mSock, &(mRecvOverlappedEx.m_wsaBuf), 1, &dwRecvNumBytes, &dwFlag, (LPWSAOVERLAPPED) & (mRecvOverlappedEx), NULL);

		// socket_error�� ��� : client socket�� ������ ������ ó��
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSARecv()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	//// 1���� �����忡���� ȣ���ؾ� ��!
	//bool SendMsg(const UINT32 dataSize_, char* pMsg_) // �������� I/O Send �۾�
	//{
	//	auto sendOverlappedEx = new stOverlappedEx;
	//	ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));
	//	sendOverlappedEx->m_wsaBuf.len = dataSize_;
	//	sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];			// 3�ܰ�(IOCPServer.h)�� �޶��� ��. ����� ���� �Ҵ��ؼ� �޾ƿͼ� �־��ش�. -> ���Ŀ� delete[]�� �������ش�.
	//	// ���� ������ �Ϸ�Ǳ� ���� �� Send�� �����ص� �����Ͱ� �������� �ʴ´�!! ������.	
	//	CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
	//	sendOverlappedEx->m_eOperation = IOOperation::SEND;

	//	DWORD dwRecvNumBytes = 0;
	//	int nRet = WSASend(mSock,			// �񵿱� IO ó��
	//		&(sendOverlappedEx->m_wsaBuf),
	//		1,
	//		&dwRecvNumBytes,
	//		0,
	//		(LPWSAOVERLAPPED)sendOverlappedEx,
	//		NULL);

	//	//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
	//	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	//	{
	//		printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
	//		return false;
	//	}

	//	return true;
	//}
	// 
	// -----------------------------------------------
	// �� ������ �Ʒ��� ���� ���� (1-send ������ ����)
	// 1���� �����忡���� ȣ���ؾ� ��!
	bool SendMsg(const UINT32 dataSize_, char* pMsg_)
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		if ((mSendPos + dataSize_) > MAX_SOCK_SENDBUF)
		{
			mSendPos = 0;
		}

		auto pSendBuf = &mSendBuf[mSendPos];

		// ���۵� �޼����� ���� (SendBuf�� �����͸� �־���)
		CopyMemory(pSendBuf, pMsg_, dataSize_);
		mSendPos += dataSize_;

		return true;
	}

	bool SendIO()
	{
		if (mSendPos <= 0 || mIsSending)
		{
			return true;
		}

		std::lock_guard<std::mutex> guard(mSendLock);

		mIsSending = true;

		CopyMemory(mSendingBuf, &mSendBuf[0], mSendPos);

		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		mSendOverlappedEx.m_wsaBuf.len = mSendPos;
		mSendOverlappedEx.m_wsaBuf.buf = &mSendingBuf[0];
		mSendOverlappedEx.m_eOperation = IOOperation::SEND;

		DWORD dwRecvNumBytes = 0;
		int nRet = WSASend(mSock,
			&(mSendOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED) & (mSendOverlappedEx),
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		mSendPos = 0;
		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		mIsSending = false;
		printf("[�۽� �Ϸ�] bytes : %d\n", dataSize_);
	}


private:
	INT32 mIndex = 0;
	SOCKET mSock;						// Cliet�� ����Ǵ� ����
	stOverlappedEx mRecvOverlappedEx;	// RECV Overlapped I/O�۾��� ���� ����
	stOverlappedEx mSendOverlappedEx;	//SEND Overlapped I/O�۾��� ���� ����

	// ������ ����	
	char mRecvBuf[MAX_SOCKBUF];	
	char mSendBuf[MAX_SOCK_SENDBUF];	
	char mSendingBuf[MAX_SOCK_SENDBUF];

	std::mutex mSendLock;
	bool mIsSending = false;
	UINT64 mSendPos = 0;
};