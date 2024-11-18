#pragma once
#pragma comment(lib, "Mswsock.lib")

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

const UINT32 MAX_SOCKBUF = 256;			// ��Ŷ ũ��
const UINT32 MAX_WORKERTHREAD = 4;		// ������ Ǯ�� ���� ������ ��
const UINT32 MAX_SOCK_SENDBUF = 4096;	// ���� ������ ũ��
const UINT64 RE_USE_SESSION_WAIT_TIMESEC = 3;


enum class IOOperation
{
	ACCEPT,
	RECV,
	SEND
};

// WSAOVERLAPPED ����ü Ȯ��
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;		// Overlapped I/O����ü
	WSABUF m_wsaBuf;					// Overlapped I/O�۾� ����
	IOOperation m_eOperation;			// �۾� ���� ����
	SOCKET m_socketClient;				// Ŭ���̾�Ʈ ����
	UINT32 SessionIndex = 0;
};

//// Ŭ���̾�Ʈ ������ ��� ����ü -> ClientInfo.h�� �и�
//struct stClientInfo
//{
//	INT32 mIndex = 0;					    // Ŭ���̾�Ʈ �ε���
//	SOCKET m_socketClient;					// Cliet�� ����Ǵ� ����
//	stOverlappedEx m_stRecvOverlappedEx;	// RECV Overlapped I/O�۾��� ���� ����
//	stOverlappedEx m_stSendOverlappedEx;	// SEND Overlapped I/O�۾��� ���� ����
//
//	char mRecvBuf[MAX_SOCKBUF]; // ������ ����
//	char mSendBuf[MAX_SOCKBUF]; // ������ ����
//
//	stClientInfo()
//	{
//		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
//		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
//		m_socketClient = INVALID_SOCKET;
//	}
//};
