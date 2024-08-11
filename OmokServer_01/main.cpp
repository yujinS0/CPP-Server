#include <iostream>
#include <thread>

#include "Server.h"

int main()
{
	Server server; // 서버의 인스턴스화
	server.Init();	// Init 함수 호출 (내부적으로 네트워크의 init 함수)

	std::thread runThread([&]() { // 워크 스레드 만들어서 진행 (이유? Run 함수에 써있음)
		server.Run(); } // 서버의 Run 함수 호출 
	);


	
	std::cout << "press any key to exit...";
	getchar(); // 키 누르면 스레드 종료되도록

	server.Stop();
	runThread.join();

	return 0;
}