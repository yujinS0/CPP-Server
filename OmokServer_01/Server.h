#pragma once
#include <memory>

#include "Packet.h"
#include "ErrorCode.h"


namespace NServerNetLib
{
	struct ServerConfig;
	class ILog;
	class ITcpNetwork;
}


class UserManager;
class RoomManager;
class PacketProcess;

class Server
{
public:
	Server();
	~Server();

	ERROR_CODE Init();

	void Run();

	void Stop();


private:
	ERROR_CODE LoadConfig();

	void Release();


private:
	// 서버에서 사용할 모든 객체들을 Server 클래스가 다 가지고 있다.
	bool m_IsRun = false;

	std::unique_ptr<NServerNetLib::ServerConfig> m_pServerConfig; // 서버 설정 정보
	std::unique_ptr<NServerNetLib::ILog> m_pLogger; // 로그 정보

	std::unique_ptr<NServerNetLib::ITcpNetwork> m_pNetwork; // 네트워크 기능 클래스
	
	std::unique_ptr<PacketProcess> m_pPacketProc; // 패킷 처리 클래스
	
	std::unique_ptr<UserManager> m_pUserMgr; // 로그인된 유저 정보
	
	std::unique_ptr<RoomManager> m_pRoomMgr; // Room 정보 관리
		
};

