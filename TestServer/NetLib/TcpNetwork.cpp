#include <cstring>
#include <vector>
#include <deque>

#include "TcpNetwork.h"


namespace NServerNetLib
{
	TcpNetwork::~TcpNetwork() 
	{
		for (auto& client : m_ClientSessionPool)
		{
			if (client.pRecvBuffer) {
				delete[] client.pRecvBuffer;
			}			
		}

		WSACleanup();
	}

	NET_ERROR_CODE TcpNetwork::Init(const ServerConfig* pConfig)
	{
		std::memcpy(&m_Config, pConfig, sizeof(ServerConfig));

		auto initRet = InitServerSocket(); // 서버 소켓 초기화
		if (initRet != NET_ERROR_CODE::NONE)
		{
			return initRet;
		}
		
		auto bindListenRet = BindListen(m_Config.Port, m_Config.BackLogCount); // 바인드	리슨
		if (bindListenRet != NET_ERROR_CODE::NONE)
		{
			return bindListenRet;
		}

		FD_ZERO(&m_Readfds);
		FD_SET(m_ServerSockfd, &m_Readfds);
		
		CreateSessionPool(m_Config.MaxClientCount);
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TcpNetwork::InitServerSocket()
	{
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		WSAStartup(wVersionRequested, &wsaData); // WinSock 초기화

		m_ServerSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 리슨 소켓 생성
		if (m_ServerSockfd < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_CREATE_FAIL;
		}

		auto n = 1;
		if (setsockopt(m_ServerSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n)) < 0) // 옵션 클래스 사용
		{
			return NET_ERROR_CODE::SERVER_SOCKET_SO_REUSEADDR_FAIL;
		}

		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TcpNetwork::BindListen(short port, int backlogCount)
	{
		struct sockaddr_in server_addr;
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(port);

		if (bind(m_ServerSockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_BIND_FAIL;
		}

		if (listen(m_ServerSockfd, backlogCount) == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_LISTEN_FAIL;
		}

		return NET_ERROR_CODE::NONE;
	}

	int TcpNetwork::CreateSessionPool(const int maxClientCount)
	{
		for (int i = 0; i < maxClientCount; ++i) // 처음부터 최대 클라이언트 수 만큼 ClientSession 객체 만들고, 추후 연결되면 사용하지 않는 객체 중에서 할당하는 방식으로 진행
		{
			ClientSession session;  // 새로운 클라이언트가 네트워크에 연결되면, ClientSession 객체 부여
			session.Clear();
			session.Index = i;
			session.pRecvBuffer = new char[m_Config.MaxClientRecvBufferSize];
			
			m_ClientSessionPool.push_back(session);

			// ClientSession 객체 부여 이유?
				// 연결된 클라에서 데이터가 오면 데이터를 버퍼에 담아두고,
				// 서버에서 우리는 네트워크 이벤트가 발생하면 m_PacketQueue에 담아두는데
				// 이때 Receive한 데이터를 담아둘 버퍼가 필요하다. 그것이 pRecvBuffer
				// 따라서 새로운 연결이 생기면 ClientSession 객체를 할당하고, 
		}

		return maxClientCount;
	}

	int TcpNetwork::AllocClientSessionIndex()
	{
		int index = -1;

		for (auto& session : m_ClientSessionPool)
		{
			if (session.IsConnected() == false)
			{
				index = session.Index;
				break;
			}
		}
		
		return index;
	}


	RecvPacketInfo TcpNetwork::GetPacketInfo()
	{
		RecvPacketInfo packetInfo;

		if (m_PacketQueue.empty() == false)
		{
			packetInfo = m_PacketQueue.front();
			m_PacketQueue.pop_front();
		}
				
		return packetInfo;
	}
		
	
	void TcpNetwork::Run()
	{
		auto read_set = m_Readfds; //
		
		timeval timeout{ 0, 1000 }; //tv_sec, tv_usec

        auto selectResult = select(0, &read_set, 0, 0, &timeout); // [Select API 호출]
		if (selectResult == 0) // 네트워크 이벤트 없을 때
		{
			return;
		}
		
		int newSessionIndex = -1;

		// 네트워크 이벤트 있을 때 (Accept, Receive 2가지로 나뉜다)
		// Accept
		if (FD_ISSET(m_ServerSockfd, &read_set))
		{
			struct sockaddr_in client_adr;

			auto client_len = static_cast<int>(sizeof(client_adr));
			auto client_sockfd = accept(m_ServerSockfd, (struct sockaddr*)&client_adr, &client_len); // accept

			newSessionIndex = AllocClientSessionIndex(); // 새로운 연결이기 때문에, 사용하지 않는 ClientSession 객체 할당을 위해 
																				  // 사용 안하는 index 반환 중.
			if (newSessionIndex < 0)
			{
				CloseSession(SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY, client_sockfd, -1);
			} 
			else
			{
				FD_SET(client_sockfd, &m_Readfds); // m_Readfds에다가 마킹 (다음에 연결된 것들만 이벤트 체크해달라고 할 수 있)
				ConnectedSession(newSessionIndex, client_sockfd); // 새로운 연결 이므로 값 채워주는 중
			}
		}
		
		// receive
			// m_ServerSockfd이 아닌, 현재 연결된 클라이언트에게서 네트워크 이벤트가 발생 (연결이 끊어지거나, 데이터가 도착했다)
		for (int i = 0; i < m_ClientSessionPool.size(); ++i)
		{
			auto& session = m_ClientSessionPool[i];

			if (session.IsConnected() == false) { // 연결이 끊어진 세션은 체크할 필요 없다.
				continue;
			}
			else if (newSessionIndex == session.Index) { // 방금 접속한 세션이므로 receive 체크할 필요 없다.
				continue;
			}

			if (!FD_ISSET(session.SocketFD, &read_set)) // 연결이 된 경우만 체크해서, 
			{
				continue;
			}

			auto ret = RecvSocket(session.Index); // 네트워크 이벤트가 발생했다면, RecvSocket 호출
			if (ret != NET_ERROR_CODE::NONE)
			{
				CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_ERROR, session.SocketFD, session.Index);
			}
		}
	}
		
	NET_ERROR_CODE TcpNetwork::SendData(const int sessionIndex, const short dataSize, const char* pData)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		send(session.SocketFD, pData, dataSize, 0);
		return NET_ERROR_CODE::NONE;
	}
		
	void TcpNetwork::ConnectedSession(const int sessionIndex, const SOCKET fd)
	{	   	
		auto& session = m_ClientSessionPool[sessionIndex];
		session.SocketFD = fd;
				
		AddPacketQueue(sessionIndex, (short)PACKET_ID::NTF_SYS_CONNECT_SESSION, 0, nullptr); // 큐에 넣어서 이벤트 알림
	}

	void TcpNetwork::CloseSession(const SOCKET_CLOSE_CASE closeCase, const SOCKET sockFD, const int sessionIndex)
	{
		if (closeCase == SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY)
		{
			closesocket(sockFD);
			FD_CLR(sockFD, &m_Readfds);
			return;
		}

		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}


        closesocket(sockFD);
		FD_CLR(sockFD, &m_Readfds);

		m_ClientSessionPool[sessionIndex].Clear();
		
		AddPacketQueue(sessionIndex, (short)PACKET_ID::NTF_SYS_CLOSE_SESSION, 0, nullptr);
	}

	NET_ERROR_CODE TcpNetwork::RecvSocket(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		auto fd = static_cast<SOCKET>(session.SocketFD);
				
		int recvPos = 0;						
		auto recvSize = recv(fd, &session.pRecvBuffer[recvPos], m_Config.MaxClientRecvBufferSize, 0);
		if (recvSize <= 0)
		{
			return NET_ERROR_CODE::RECV_REMOTE_CLOSE;
		}

		// TODO 아래 구현해야 한다
		//AddPacketQueue(sessionIndex, pPktHeader->Id, bodySize, &session.pRecvBuffer[readPos]);

		/*if (session.RemainingDataSize > 0)
		{
			memcpy(session.pRecvBuffer, &session.pRecvBuffer[session.PrevReadPosInRecvBuffer], session.RemainingDataSize);
			recvPos += session.RemainingDataSize;
		}

		auto recvSize = recv(fd, &session.pRecvBuffer[recvPos], (MAX_PACKET_BODY_SIZE * 2), 0);

		if (recvSize == 0)
		{
			return NET_ERROR_CODE::RECV_REMOTE_CLOSE;
		}

		if (recvSize < 0)
		{
			auto netError = WSAGetLastError();

			if (netError != WSAEWOULDBLOCK)
			{
				return NET_ERROR_CODE::RECV_API_ERROR;
			}
			else
			{
				return NET_ERROR_CODE::NONE;
			}
		}

		session.RemainingDataSize += recvSize;*/
		
		return NET_ERROR_CODE::NONE;
	}

	void TcpNetwork::AddPacketQueue(const int sessionIndex, const short pktId, const short bodySize, char* pDataPos)
	{
		RecvPacketInfo packetInfo;
		packetInfo.SessionIndex = sessionIndex;
		packetInfo.PacketId = pktId;
		packetInfo.PacketBodySize = bodySize;
		packetInfo.pRefData = pDataPos;

		m_PacketQueue.push_back(packetInfo);
	}
		
	
	
	
}