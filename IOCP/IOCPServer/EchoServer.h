#pragma once

#include "IOCPServer.h"

class EchoServer : public IOCPServer // IOCPServer(��� ��Ʈ��ũ ���̺귯��)�� ��� �޾� ���(���ܱ���)    // �̶�, IOCPServer ��� �̸����� TCPNetwork�� IOCPNetwork������ �̸��� �ٲٴ� ���� ����.
{
	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		printf("[OnConnect] Ŭ���̾�Ʈ: Index(%d)\n", clientIndex_);
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		printf("[OnClose] Ŭ���̾�Ʈ: Index(%d)\n", clientIndex_);
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		printf("[OnReceive] Ŭ���̾�Ʈ: Index(%d), dataSize(%d)\n", clientIndex_, size_);
	}
};