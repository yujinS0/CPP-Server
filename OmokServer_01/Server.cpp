#include <thread>
#include <chrono>

#include "NetLib/ServerNetErrorCode.h"
#include "NetLib/Define.h"
#include "NetLib/TcpNetwork.h"
#include "ConsoleLogger.h"
#include "RoomManager.h"
#include "PacketProcess.h"
#include "UserManager.h"
#include "Server.h"

using NET_ERROR_CODE = NServerNetLib::NET_ERROR_CODE;
using LOG_TYPE = NServerNetLib::LOG_TYPE;

Server::Server()
{
}

Server::~Server()
{
	Release();
}

ERROR_CODE Server::Init() // 처음 호출되는 함수
{
	m_pLogger = std::make_unique<ConsoleLog>(); // 로그 클래스의 인스턴스화

	LoadConfig(); // 설정 정보 로드

	// 네트워크 초기화
	m_pNetwork = std::make_unique<NServerNetLib::TcpNetwork>();
	auto result = m_pNetwork->Init(m_pServerConfig.get(), m_pLogger.get());

	if (result != NET_ERROR_CODE::NONE)
	{
		m_pLogger->Write(LOG_TYPE::L_ERROR, "%s | Init Fail. NetErrorCode(%s)", __FUNCTION__, (short)result);
		return ERROR_CODE::MAIN_INIT_NETWORK_INIT_FAIL;
	}

	// 서버에서 사용할 객체 생성 및 초기화
	m_pUserMgr = std::make_unique<UserManager>();
	m_pUserMgr->Init(m_pServerConfig->MaxClientCount);

	m_pRoomMgr = std::make_unique<RoomManager>();
	m_pRoomMgr->Init(m_pServerConfig->MaxRoomCount, m_pServerConfig->MaxRoomUserCount);
	m_pRoomMgr->SetNetwork(m_pNetwork.get(), m_pLogger.get());

	m_pPacketProc = std::make_unique<PacketProcess>();
	m_pPacketProc->Init(m_pNetwork.get(), m_pUserMgr.get(), m_pRoomMgr.get(), m_pServerConfig.get(), m_pLogger.get());

	m_IsRun = true;

	m_pLogger->Write(LOG_TYPE::L_INFO, "%s | Init Success. Server Run", __FUNCTION__);
	return ERROR_CODE::NONE;
}

void Server::Release() // 서버 종료 시
{
	if (m_pNetwork) {
		m_pNetwork->Release();
	}
}

void Server::Stop()
{
	m_IsRun = false;
}

void Server::Run() // Run 함수(main에서 호출되는)
{
	while (m_IsRun)
	{
		m_pNetwork->Run();  // 네트워크 클래스의 Run 호출 시, 내부적으로 Select api 호출
							// => 따라서 네트워크 이벤트 발생 전까지 대기가 발생함
							// => 이 때문에 별도의 스레드를 만들어서 Run 함수를 호출해야 함	

		while (true)
		{	
			// [packetInfo 구조]
			/*struct RecvPacketInfo
			{
				int SessionIndex = 0; // 세션 인덱스(네트워크 측 클라이언트의 인덱스 번호)
				short PacketId = 0; // 패킷 아이디(어떤 패킷인지)
				short PacketBodySize = 0; // 패킷 바디 사이즈
				char* pRefData = 0; // 패킷 바디 데이터(바디 사이즈가 0이 아니면 내용이 담겨있다)
			};*/

			auto packetInfo = m_pNetwork->GetPacketInfo();
				// GetPacketInfo 함수를 통해 패킷 정보를 가져옴
				// 네트워크 이벤트가 있다면, 계속 호출하며 받을 수 있고
				// 없다면(즉, 새로운 연결도 없고 끊어진 것도 없다면) 0 값을 주고 있음

			if (packetInfo.PacketId == 0) // 패킷 정보가 없다면 (즉, 네트워크 이벤트가 없다면) break
			{
				break;
			}
			else // 패킷 정보가 있다면
			{
				m_pPacketProc->Process(packetInfo); 
				// PacketProcess 클래스의 Process 함수를 호출하여 패킷 처리
				// packetInfo 즉 받아온 패킷	정보를 넘겨줌
			}
		}
	}
}

ERROR_CODE Server::LoadConfig() // 설정 정보 로드 (포트번호 등등) 
{
	// TODO : 현재 하드코딩 -> 수정하기
	m_pServerConfig = std::make_unique<NServerNetLib::ServerConfig>();

	m_pServerConfig->Port = 11021;
	m_pServerConfig->BackLogCount = 128;
	m_pServerConfig->MaxClientCount = 1000;

	m_pServerConfig->MaxClientSockOptRecvBufferSize = 10240; // 커널 쪽의 리시브 버퍼
	m_pServerConfig->MaxClientSockOptSendBufferSize = 10240; // 커널	쪽의 센드 버퍼
	m_pServerConfig->MaxClientRecvBufferSize = 8192; // 클라이언트 쪽, 즉 애플리케이션 서버에서의 리시브 버퍼
	m_pServerConfig->MaxClientSendBufferSize = 8192;

	m_pServerConfig->ExtraClientCount = 64; // 짜르도록 MaxClientCount + 여유분을 준비한다. (다 찼다고 알려줘야하니)
	m_pServerConfig->MaxRoomCount = 20;
	m_pServerConfig->MaxRoomUserCount = 4;
	    
	m_pLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "%s | Port(%d), Backlog(%d)", __FUNCTION__, m_pServerConfig->Port, m_pServerConfig->BackLogCount);
	return ERROR_CODE::NONE;
}
		
