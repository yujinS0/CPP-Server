#include "IOCompletionPort.h"

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;		//�� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��

int main()
{
	IOCompletionPort ioCompletionPort;

	//������ �ʱ�ȭ
	ioCompletionPort.InitSocket();

	//���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	ioCompletionPort.BindandListen(SERVER_PORT);

	ioCompletionPort.StartServer(MAX_CLIENT);

	printf("�ƹ� Ű�� ���� ������ ����մϴ�\n");
	getchar();

	ioCompletionPort.DestroyThread();
	return 0;
}

