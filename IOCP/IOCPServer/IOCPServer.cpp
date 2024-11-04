#include "IOCompletionPort.h"

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;		//총 접속할수 있는 클라이언트 수

int main()
{
	IOCompletionPort ioCompletionPort; // 1. 생성자

	//소켓을 초기화
	ioCompletionPort.InitSocket(); // 2. InitSocket

	//소켓과 서버 주소를 연결하고 등록 시킨다.
	ioCompletionPort.BindandListen(SERVER_PORT); // 3. BindandListen

	ioCompletionPort.StartServer(MAX_CLIENT); // 4. StartServer

	printf("아무 키나 누를 때까지 대기합니다\n");
	getchar();

	ioCompletionPort.DestroyThread();
	return 0;
}

