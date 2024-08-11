#pragma once

#define FD_SETSIZE 5096 // http://blog.naver.com/znfgkro1/220175848048

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>


#include <vector>
#include <deque>

#include "Define.h"


namespace NServerNetLib
{
	class TcpNetwork
	{
	public:
		TcpNetwork() = default;
		~TcpNetwork();

		// Server class에서 사용하는 함수 4개
		NET_ERROR_CODE Init(const ServerConfig* pConfig);
		
		NET_ERROR_CODE SendData(const int sessionIndex, const short dataSize, const char* pData);
		
		void Run();
		
		RecvPacketInfo GetPacketInfo();
			

	private:
		NET_ERROR_CODE InitServerSocket();		
		NET_ERROR_CODE BindListen(short port, int backlogCount);
		int CreateSessionPool(const int maxClientCount);

		int AllocClientSessionIndex();		
		
		void ConnectedSession(const int sessionIndex, const SOCKET fd);
		void CloseSession(const SOCKET_CLOSE_CASE closeCase, const SOCKET sockFD, const int sessionIndex);

		NET_ERROR_CODE RecvSocket(const int sessionIndex);
		
		void AddPacketQueue(const int sessionIndex, const short pktId, const short bodySize, char* pDataPos);
	private:
		ServerConfig m_Config;
				
		SOCKET m_ServerSockfd;
        
		fd_set m_Readfds;
		
		std::vector<ClientSession> m_ClientSessionPool;
		
		std::deque<RecvPacketInfo> m_PacketQueue; 	// 네트워크 이벤트가 발생한다면, deque에 저장
														// 네트워크 이벤트 : 새로운 연결 발생, 끊어짐, 데이터 수신 등등..
	};
}
