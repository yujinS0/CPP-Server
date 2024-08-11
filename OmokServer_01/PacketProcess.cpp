#include <cstring>

#include "NetLib/ILog.h"
#include "NetLib/TcpNetwork.h"
#include "User.h"
#include "UserManager.h"
//#include "Room.h"
#include "RoomManager.h"
#include "PacketProcess.h"

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using ServerConfig = NServerNetLib::ServerConfig;

	
PacketProcess::PacketProcess() {}
PacketProcess::~PacketProcess() {}

void PacketProcess::Init(TcpNet* pNetwork, UserManager* pUserMgr, RoomManager* pLobbyMgr, ServerConfig* pConfig, ILog* pLogger)
{
	m_pRefLogger = pLogger;
	m_pRefNetwork = pNetwork;
	m_pRefUserMgr = pUserMgr;
	m_pRefRoomMgr = pLobbyMgr;
}

// 패킷을 처리하는 곳
// : 새로운 패킷 아이디 생겨도 여기에 처리하고 구현 계속 진행하는 방식!
void PacketProcess::Process(PacketInfo packetInfo)
{
	using netLibPacketId = NServerNetLib::PACKET_ID;
	using commonPacketId = PACKET_ID;

	auto packetId = packetInfo.PacketId;
		
	switch (packetId)
	{
	case (int)netLibPacketId::NTF_SYS_CONNECT_SESSION: // 연결 
		NtfSysConnctSession(packetInfo); // (실제 클라이언트가 패킷을 보낸 것은 아님)
		break;
	case (int)netLibPacketId::NTF_SYS_CLOSE_SESSION: // 연결 끊어짐 
		NtfSysCloseSession(packetInfo); // (실제 클라이언트가 패킷을 보낸 것은 아님)
		break;
	case (int)commonPacketId::LOGIN_IN_REQ: // 로그인
		Login(packetInfo); // 실제 클라이언트가 보낸 패킷을, 패킷 아이디를 알아서 처리하는 과정을 담고 있다~
		break;
	// ... 이어서 구현 ...
	}
	
}


ERROR_CODE PacketProcess::NtfSysConnctSession(PacketInfo packetInfo)
{
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysConnctSession. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::NtfSysCloseSession(PacketInfo packetInfo)
{
	auto pUser = std::get<1>(m_pRefUserMgr->GetUser(packetInfo.SessionIndex));

	if (pUser) {		
		m_pRefUserMgr->RemoveUser(packetInfo.SessionIndex);		
	}
			
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
	return ERROR_CODE::NONE;
}


ERROR_CODE PacketProcess::Login(PacketInfo packetInfo)
{
	// 패스워드는 무조건 pass 해준다.
	// ID 중복이라면 에러 처리한다.
	// 현재 DB 처리 X
	PktLogInRes resPkt;
	auto reqPkt = (PktLogInReq*)packetInfo.pRefData;

	auto addRet = m_pRefUserMgr->AddUser(packetInfo.SessionIndex, reqPkt->szID);

	if (addRet != ERROR_CODE::NONE) {
		resPkt.SetError(addRet);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_IN_RES, sizeof(PktLogInRes), (char*)&resPkt);
		return addRet;
	}
		
	resPkt.ErrorCode = (short)addRet;
	m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_IN_RES, sizeof(PktLogInRes), (char*)&resPkt);
	// TcpNetwork::SendData 함수를 통해 패킷을 보낸다. 
		// 첫 번째 인자는 세션 인덱스(어떤 클라이언트에게 보낼 지)
		// 두 번째 인자는 패킷 아이디
		// 세 번째 인자는 패킷 바디 사이즈
		// 네 번째 인자는 패킷 바디 데이터 (구조체 형변환해서 보내고 있다)

	return ERROR_CODE::NONE;
}


