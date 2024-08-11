#pragma once
#include <memory>

#include "NetLib/TcpNetwork.h"

class Server
{
public:
	Server() = default;
	~Server() = default;

	bool Init();

	void Run();

	void Stop();


private:
	bool m_IsRun = false;

	// 네크워크 관련 클래스로 멤버로 가지고 있음
	std::unique_ptr<NServerNetLib::TcpNetwork> m_pNetwork; 
};

