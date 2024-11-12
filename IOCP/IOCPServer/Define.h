#pragma once

#include <winsock2.h>
#include <Ws2tcpip.h>

const UINT32 MAX_SOCKBUF = 256;			// 패킷 크기
const UINT32 MAX_WORKERTHREAD = 4;		// 쓰레드 풀에 넣을 쓰레드 수
const UINT32 MAX_SOCK_SENDBUF = 4096;	// 소켓 버퍼의 크기


enum class IOOperation
{
	RECV,
	SEND
};

// WSAOVERLAPPED 구조체 확장
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;		// Overlapped I/O구조체
	SOCKET m_socketClient;				// 클라이언트 소켓
	WSABUF m_wsaBuf;					// Overlapped I/O작업 버퍼
	IOOperation m_eOperation;			// 작업 동작 종류
};

//// 클라이언트 정보를 담는 구조체 -> ClientInfo.h로 분리
//struct stClientInfo
//{
//	INT32 mIndex = 0;					    // 클라이언트 인덱스
//	SOCKET m_socketClient;					// Cliet와 연결되는 소켓
//	stOverlappedEx m_stRecvOverlappedEx;	// RECV Overlapped I/O작업을 위한 변수
//	stOverlappedEx m_stSendOverlappedEx;	// SEND Overlapped I/O작업을 위한 변수
//
//	char mRecvBuf[MAX_SOCKBUF]; // 데이터 버퍼
//	char mSendBuf[MAX_SOCKBUF]; // 데이터 버퍼
//
//	stClientInfo()
//	{
//		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
//		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
//		m_socketClient = INVALID_SOCKET;
//	}
//};
