# CPP-Server
C++ 서버 및 IOCP 학습을 위한 레포지토리

학습 내용에 대한 문서 정리는 아직 진행중입니다.

## IOCP

IOCP 관련 전체 내용은 [IOCP](./IOCP/)에 위치

그 중 [IOCPServer](./IOCP/IOCPServer/)에 네트워크 관련 서버 소스코드가 위치하고 있습니다.

## 진행상황

아래의 단계 중 현재 1-send 구현(5단계 방식을 활용한 구현)까지 완료한 상황입니다.

- 1 단계. 가장 간단한 IOCP 서버. Echo 서버 코드 만들기  
    - IOCP API 동작에 대해서 이해할 수 있다.
- 2 단계. OverlappedEx 구조체에 있는 `char m_szBuf[ MAX_SOCKBUF ]`를 stClientInfo 구조체로 이동 및 코드 분리하기
    - 앞 단계에는 OverlappedEx 구조체에 `m_szBuf`가 있어서 불 필요한 메모리 낭비가 발생함
- 3 단계. 애플리케이션과 네트워크 코드 분리하기  
    - IOCP를 네트워크 라이브러리화 하기 위해 네트워크와 서버 로직 코드를 분리한다.
    - 연결, 끊어짐, 데이터 받음을 애플리케이션에 전달하기  
- 4 단계. 네트워크와 로직(패킷 or 요청) 처리 각각의 스레드로 분리하기  
    - Send를 Recv와 다른 스레드에서 하기  
    - send를 연속으로 보낼 수 있는 구조가 되어야 한다.  
- 5 단계. 1-Send 구현하기  
    - 버퍼에 쌓아 놓고, send 스레드에서 보내기.   
- 6 단계. 1-Send 구현하기  
    - queue에 담아 놓고 순차적으로 보내기.    
- 7 단계. 비동기 Accept 사용하기
    - 6단계까지는 Accept 처리를 동기 I/O로 했다. 이것을 비동기I/O로 바꾼다. 이로써 네트워크 동작이 모두 비동기 I/O가 된다
- 8 단계. 채팅 서버 만들기 (ChatServer_01)
    - 패킷 구조 사용하기, 로그인
- 9 단계. 로그인 때 Redis 사용하기 (ChatServer_02)       
    - C++에서 Redis를 사용하는 방법은 [examples_cpp_redis](https://github.com/jacking75/examples_cpp_redis )를 참고한다.
- 10 단계. 방 입장, 방 나가기, 방 채팅 구현하기 (ChatServer_03)         

## 학습자료

- [edu_cpp_IOCP](https://github.com/jacking75/edu_cpp_IOCP/tree/master)

---

## IOCP 과정 정리
IOCP 기반 에코 서버에서 클라이언트가 연결되고 메시지를 주고받을 때, 

`Accept`, `Send`, `Receive` 작업이 비동기로 처리됩니다. 

이를 IOCP API의 흐름과 함께 설명하겠습니다.


### IOCP 서버의 주요 흐름

1. **IOCP 객체 생성 및 소켓 바인딩**
   - 서버는 `CreateIoCompletionPort()`를 호출하여 IOCP 객체를 생성합니다.
   - 클라이언트 소켓이 생성되면, 해당 소켓을 IOCP에 바인딩하여 IO 작업 완료 시 IOCP 큐에 작업 완료 알림이 등록되도록 설정합니다.

2. **`Accept` 작업 요청**
   - 서버는 클라이언트 연결을 기다리기 위해 `AcceptEx()`를 호출합니다. 
   - 연결이 완료되면, IOCP는 완료 알림을 큐에 등록하고 워커 스레드에서 이를 처리합니다.

3. **`Receive` 작업 요청**
   - 클라이언트로부터 데이터를 비동기로 수신하기 위해 `WSARecv()`를 호출합니다.
   - 이 작업 또한 IOCP에 의해 관리되며, 수신이 완료되면 IOCP 큐에 완료 알림이 등록됩니다.

4. **`Send` 작업 요청**
   - 서버는 클라이언트로부터 받은 데이터를 비동기로 송신하기 위해 `WSASend()`를 호출합니다.
   - 송신이 완료되면 IOCP 큐에 완료 알림이 등록됩니다.

5. **작업 완료 대기 및 처리**
   - 워커 스레드는 `GetQueuedCompletionStatus()`를 호출하여 IOCP 큐에서 완료된 작업을 기다립니다.
   - 완료된 작업의 유형(Accept, Receive, Send)에 따라 적절히 처리합니다.




