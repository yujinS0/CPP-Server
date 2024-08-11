#pragma once



namespace NServerNetLib
{
	struct ServerConfig
	{
		unsigned short Port;
		int BackLogCount;

		int MaxClientCount;
		short MaxClientRecvBufferSize;
		short MaxClientSendBufferSize;
	};
		
	struct ClientSession
	{
		bool IsConnected() { return SocketFD != 0 ? true : false; }

		void Clear()
		{
			SocketFD = 0;	
		}

		int Index = 0;
		unsigned long long	SocketFD = 0;		
		char*   pRecvBuffer = nullptr; 		
	};

	struct RecvPacketInfo
	{
		int SessionIndex = 0;
		short PacketId = 0;
		short PacketBodySize = 0;
		char* pRefData = 0;
	};
	

	enum class SOCKET_CLOSE_CASE : short
	{
		SESSION_POOL_EMPTY = 1,
		SELECT_ERROR = 2,
		SOCKET_RECV_ERROR = 3,
		SOCKET_RECV_BUFFER_PROCESS_ERROR = 4,
		SOCKET_SEND_ERROR = 5,
		FORCING_CLOSE = 6,
	};
	

	enum class PACKET_ID : short
	{
		NTF_SYS_CONNECT_SESSION = 2,
		NTF_SYS_CLOSE_SESSION = 3,
				
	};

#pragma pack(push, 1)
	struct PacketHeader
	{
		short TotalSize;
		short Id;
		unsigned char Reserve;
	};

	const int PACKET_HEADER_SIZE = sizeof(PacketHeader);

#pragma pack(pop)



	// 에러 코드는 1 ~ 200까지 사용한다.
	enum class NET_ERROR_CODE : short
	{
		NONE = 0,

		SERVER_SOCKET_CREATE_FAIL = 11,
		SERVER_SOCKET_SO_REUSEADDR_FAIL = 12,
		SERVER_SOCKET_BIND_FAIL = 14,
		SERVER_SOCKET_LISTEN_FAIL = 15,
		SERVER_SOCKET_FIONBIO_FAIL = 16,

		SEND_CLOSE_SOCKET = 21,
		SEND_SIZE_ZERO = 22,
		CLIENT_SEND_BUFFER_FULL = 23,
		CLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE = 24,

		ACCEPT_API_ERROR = 26,
		ACCEPT_MAX_SESSION_COUNT = 27,
		ACCEPT_API_WSAEWOULDBLOCK = 28,

		RECV_API_ERROR = 32,
		RECV_BUFFER_OVERFLOW = 32,
		RECV_REMOTE_CLOSE = 33,
		RECV_PROCESS_NOT_CONNECTED = 34,
		RECV_CLIENT_MAX_PACKET = 35,
	};
}