#include <iostream>
#include <thread>

#include "Server.h"

int main()
{
	Server server; // Server 객체 생성
	server.Init(); // Server 객체 초기화 = 즉 Listen 상태로 들어감

	std::thread runThread([&]() { 	// Server 객체의 Run 함수를 쓰레드로 돌림
		server.Run(); } // Run 내부 보면 Select API 사용
	);


	
	std::cout << "press any key to exit..." << std::endl;
	getchar();

	server.Stop();
	runThread.join();

	return 0;
}