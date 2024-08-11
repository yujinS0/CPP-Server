#include <thread>
#include <chrono>

#include "NetLib/Define.h"
#include "Server.h"


bool Server::Init()
{
	NServerNetLib::ServerConfig config;
	config.Port = 11021;
	config.BackLogCount = 128;
	config.MaxClientCount = 1000;
	config.MaxClientRecvBufferSize = 8192;
	config.MaxClientSendBufferSize = 8192; // 현재 테스트에서는 사용 X

	m_pNetwork = std::make_unique<NServerNetLib::TcpNetwork>(); // TcpNetwork 객체 생성
	m_pNetwork->Init(&config); // TcpNetwork 객체 초기화
	m_IsRun = true;

	return true;
}

void Server::Stop() // 서버 종료
{
	m_IsRun = false;
}

void Server::Run() // 서버 동작
{
	while (m_IsRun)
	{
		m_pNetwork->Run(); // TcpNetwork 객체의 Run 함수 호출 (Select API 사용)

		while (true) //
		{				
			auto packetInfo = m_pNetwork->GetPacketInfo();
			if (packetInfo.PacketId == 0) // 패킷이 없으면 루프 탈출
			{
				break;
			}
			
			auto packetId = packetInfo.PacketId;

			// 패킷 처리
			switch (packetId)
			{
			case (int)NServerNetLib::PACKET_ID::NTF_SYS_CONNECT_SESSION:
				printf("[Connect Session] - SessionID(%d)\n", packetInfo.SessionIndex);
				break;
			case (int)NServerNetLib::PACKET_ID::NTF_SYS_CLOSE_SESSION:
				printf("[Disconnect Session] - SessionID(%d)\n", packetInfo.SessionIndex);
				break;
			default:
				//패킷을 받음
				break;
			}
		}
	}
}
	
