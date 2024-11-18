#pragma once

#include "IOCPServer.h"
#include "Packet.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <print>

class EchoServer : public IOCPServer // IOCPServer(��� ��Ʈ��ũ ���̺귯��)�� ��� �޾� ���(���ܱ���)    // �̶�, IOCPServer ��� �̸����� TCPNetwork�� IOCPNetwork������ �̸��� �ٲٴ� ���� ����.
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
		packet.Set(clientIndex_, size_, pData_); // ���� �������� ����(pData_), ũ��(size_), ���� �޾Ҵ���(clientIndex_) ��..

		std::lock_guard<std::mutex> guard(mLock);
		mPacketDataQueue.push_back(packet);			// ���� ������ ť�� ����ؼ� ��� �ִ�. (���⼭�� ���� i/o�����忡�� ��Ŷ ó���� �ϰ� ���� �ʱ� ������..��Ŷó�� ������� �Ѱ��ֱ� ���� ť�� ��Ƶд�.)
		// EchoServer�� ProcessPacket()�Լ����� ť���� ��Ŷ�� ������ ó���Ѵ�.
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
	void ProcessPacket() // ��Ŷ ó�� ������
	{
		while (mIsRunProcessThread)
		{
			auto packetData = DequePacketData();
			if (packetData.DataSize != 0) // ��Ŷ�� ������
			{
				std::print("SendMsg");
				SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData); // ���� ��Ŷ�� �ٽ� ������. (EchoServer)
				// ���� ���ڼ����� �ƴ϶�� �� ������ ��ŶID�� �������� ���� ������ ���� ó���� �ϴ� ������ ����
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