#include "IOCompletionPort.h"

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;		//�� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��

int main()
{
	IOCompletionPort ioCompletionPort; // 1. ������

	//������ �ʱ�ȭ
	ioCompletionPort.InitSocket(); // 2. InitSocket

	//���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	ioCompletionPort.BindandListen(SERVER_PORT); // 3. BindandListen

	ioCompletionPort.StartServer(MAX_CLIENT); // 4. StartServer

	printf("�ƹ� Ű�� ���� ������ ����մϴ�\n");
	getchar();

	ioCompletionPort.DestroyThread();
	return 0;
}

