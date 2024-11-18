#pragma once

#include "IOCPServer.h"
#include "Packet.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <print>

class EchoServer : public IOCPServer // IOCPServer(라는 네트워크 라이브러리)를 상속 받아 사용(간단구현)    // 이때, IOCPServer 라는 이름보다 TCPNetwork나 IOCPNetwork등으로 이름을 바꾸는 것이 좋다.
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;


	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		printf("[OnConnect] Client: Index(%d)\n", clientIndex_);
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		printf("[OnClose] Client: Index(%d)\n", clientIndex_);
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		printf("[OnReceive] Client: Index(%d), dataSize(%d)\n", clientIndex_, size_);

		PacketData packet;
		packet.Set(clientIndex_, size_, pData_); // 받은 데이터의 정보(pData_), 크기(size_), 누가 받았는지(clientIndex_) 등..

		std::lock_guard<std::mutex> guard(mLock);
		mPacketDataQueue.push_back(packet);			// 받은 내용을 큐를 사용해서 담고 있다. (여기서는 지금 i/o쓰레드에서 패킷 처리를 하고 있지 않기 때문에..패킷처리 스레드로 넘겨주기 위해 큐에 담아둔다.)
		// EchoServer의 ProcessPacket()함수에서 큐에서 패킷을 꺼내서 처리한다.
	}

	void Run(const UINT32 maxClient)
	{
		mIsRunProcessThread = true;
		mProcessThread = std::thread([this]() { ProcessPacket(); });

		StartServer(maxClient);
	}

	void End()
	{
		mIsRunProcessThread = false;

		if (mProcessThread.joinable())
		{
			mProcessThread.join();
		}

		DestroyThread();
	}

private:
	void ProcessPacket() // 패킷 처리 스레드
	{
		while (mIsRunProcessThread)
		{
			auto packetData = DequePacketData();
			if (packetData.DataSize != 0) // 패킷이 있으면
			{
				std::print("SendMsg");
				SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData); // 받은 패킷을 다시 보낸다. (EchoServer)
				// 만약 에코서버가 아니라면 이 곳에서 패킷ID가 무엇인지 보고 종류에 따라 처리를 하는 식으로 진행
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	PacketData DequePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty())
		{
			return PacketData();
		}

		packetData.Set(mPacketDataQueue.front());

		mPacketDataQueue.front().Release();
		mPacketDataQueue.pop_front();

		return packetData;	
	}

	bool mIsRunProcessThread = false;

	std::thread mProcessThread;

	std::mutex mLock;
	std::deque<PacketData> mPacketDataQueue;
};