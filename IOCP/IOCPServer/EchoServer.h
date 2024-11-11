#pragma once

#include "IOCPServer.h"

class EchoServer : public IOCPServer // IOCPServer(라는 네트워크 라이브러리)를 상속 받아 사용(간단구현)    // 이때, IOCPServer 라는 이름보다 TCPNetwork나 IOCPNetwork등으로 이름을 바꾸는 것이 좋다.
{
	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		printf("[OnConnect] 클라이언트: Index(%d)\n", clientIndex_);
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		printf("[OnClose] 클라이언트: Index(%d)\n", clientIndex_);
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		printf("[OnReceive] 클라이언트: Index(%d), dataSize(%d)\n", clientIndex_, size_);
	}
};