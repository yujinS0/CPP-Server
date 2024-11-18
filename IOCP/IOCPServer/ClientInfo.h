#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex> // send 작업 시 mutex 필요

// 클라이언트 정보를 담기위한 구조체
// 연결된 클라이언트 세션 정보를 담는.. ClientInfo 클래스
class stClientInfo // 이름 수정
{
public:
	stClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx)); //
		mSock = INVALID_SOCKET;
	}

	~stClientInfo() = default;

	void Init(const UINT32 index, HANDLE iocpHandle_)
	{
		mIndex = index;
		mIOCPHandle = iocpHandle_;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnectd() { return mSock != INVALID_SOCKET; } // mIsConnect == 1;

	SOCKET GetSock() { return mSock; }

	char* RecvBuffer() { return mRecvBuf; }

	UINT64 GetLatestClosedTimeSec() { return mLatestClosedTimeSec; }


	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_)
	{
		mSock = socket_;

		Clear();

		//I/O Completion Port객체와 소켓을 연결시킨다.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER로 설정

		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(mSock, SD_BOTH); // socketClose 소켓의 데이터 송수신을 모두 중단

		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		//mIsConnect = 0;
		mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear()
	{
		mSendPos = 0;
		mIsSending = false;
	}

	bool PostAccept(SOCKET listenSock_, const UINT64 curTimeSec_)
	{
		printf_s("PostAccept. client Index: %d\n", GetIndex());

		mLatestClosedTimeSec = UINT32_MAX;

		mSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (mSock == INVALID_SOCKET)
		{
			printf_s("client Socket WSASocket Error : %d\n", GetLastError());
			return false;
		}

		ZeroMemory(&mAcceptContext, sizeof(stOverlappedEx));

		DWORD bytes = 0;
		DWORD flags = 0;
		mAcceptContext.m_wsaBuf.len = 0;
		mAcceptContext.m_wsaBuf.buf = nullptr;
		mAcceptContext.m_eOperation = IOOperation::ACCEPT;
		mAcceptContext.SessionIndex = mIndex;

		if (AcceptEx(listenSock_, mSock, mAcceptBuf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)) == FALSE)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf_s("AcceptEx Error : %d\n", GetLastError());
				return false;
			}
		}

		return true;
	}

	bool AcceptCompletion()
	{
		printf_s("AcceptCompletion : SessionIndex(%d)\n", mIndex);

		if (OnConnect(mIOCPHandle, mSock) == false)
		{
			return false;
		}

		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);
		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)mSock);

		return true;
	}



	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0); // socket과 pClientInfo를 CompletionPort 객체와 연결

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		// Overlapped I/O을 위한 정보 셋팅
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(mSock, &(mRecvOverlappedEx.m_wsaBuf), 1, &dwRecvNumBytes, &dwFlag, (LPWSAOVERLAPPED) & (mRecvOverlappedEx), NULL);

		// socket_error일 경우 : client socket이 끊어진 것으로 처리
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[에러] WSARecv()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	// 1-send 구현
	// 1개의 스레드에서만 호출해야 함!
	bool SendMsg(const UINT32 dataSize_, char* pMsg_)
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		if ((mSendPos + dataSize_) > MAX_SOCK_SENDBUF)
		{
			mSendPos = 0;
		}

		auto pSendBuf = &mSendBuf[mSendPos];

		// 전송될 메세지를 복사 (SendBuf에 데이터를 넣어줌)
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

		//Overlapped I/O을 위해 각 정보를 셋팅해 준다.
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

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[에러] WSASend()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		mSendPos = 0;
		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		mIsSending = false;
		printf("[송신 완료] bytes : %d\n", dataSize_);
	}


private:
	INT32 mIndex = 0;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	SOCKET mSock;						// Cliet와 연결되는 소켓
	stOverlappedEx mRecvOverlappedEx;	// RECV Overlapped I/O작업을 위한 변수
	stOverlappedEx mSendOverlappedEx;	//SEND Overlapped I/O작업을 위한 변수

	//INT64 mIsConnect = 0;
	UINT64 mLatestClosedTimeSec = 0;

	// 데이터 버퍼	
	char mRecvBuf[MAX_SOCKBUF];	
	char mSendBuf[MAX_SOCK_SENDBUF];	
	char mSendingBuf[MAX_SOCK_SENDBUF];

	stOverlappedEx	mAcceptContext;
	char mAcceptBuf[64];

	std::mutex mSendLock;
	bool mIsSending = false;
	UINT64 mSendPos = 0;
};