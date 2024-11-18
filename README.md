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




### 에코 서버의 상세 과정 (IOCP API와 함께)

#### 1. IOCP 객체 생성
서버는 IOCP 객체를 생성합니다:
```cpp
HANDLE iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
```
이 객체는 모든 I/O 작업의 완료 상태를 관리합니다.



#### 2. 클라이언트 연결 (`AcceptEx`)
클라이언트 연결 요청을 수락하기 위해 비동기 `AcceptEx()`를 호출합니다:
```cpp
AcceptEx(listenSocket, clientSocket, buffer, 0, addrSize, addrSize, &bytesReceived, &overlapped);
```
- 이 작업은 완료될 때까지 기다리지 않고 바로 반환됩니다.
- 클라이언트 연결이 완료되면 IOCP 큐에 알림이 등록됩니다.

#### 3. 연결 완료 처리
`GetQueuedCompletionStatus()`가 `Accept` 작업 완료를 감지합니다:
```cpp
BOOL result = GetQueuedCompletionStatus(
    iocpHandle,
    &bytesTransferred,
    (PULONG_PTR)&completionKey,
    &overlapped,
    INFINITE);
```
- `completionKey`를 통해 연결된 클라이언트의 정보를 가져옵니다.
- 완료된 작업 유형(`OVERLAPPED` 구조체의 내용)을 확인하여 다음 작업을 요청합니다.



#### 4. 데이터 수신 (`WSARecv`)
클라이언트가 데이터를 보낼 수 있도록 비동기 `WSARecv()`를 호출합니다:
```cpp
WSARecv(clientSocket, &wsaBuf, 1, NULL, &flags, &overlapped, NULL);
```
- `wsaBuf`는 데이터를 저장할 버퍼를 가리킵니다.
- 클라이언트가 데이터를 보낼 때까지 기다리지 않고 즉시 반환됩니다.

#### 5. 수신 완료 처리
클라이언트로부터 데이터 수신이 완료되면 IOCP 큐에 알림이 등록되고, 워커 스레드에서 이를 처리합니다:
```cpp
if (overlappedEx->operationType == IOOperation::RECV) {
    std::cout << "Received: " << overlappedEx->buffer << std::endl;
    // 에코를 위해 송신 요청
    WSASend(clientSocket, &wsaBuf, 1, NULL, 0, &overlapped, NULL);
}
```
- 받은 데이터를 그대로 송신 작업에 사용하여 에코 기능을 구현합니다.

 

#### 6. 데이터 송신 (`WSASend`)
비동기로 데이터를 송신합니다:
```cpp
WSASend(clientSocket, &wsaBuf, 1, NULL, 0, &overlapped, NULL);
```
- `wsaBuf`는 송신할 데이터를 가리킵니다.
- 송신이 완료되면 IOCP 큐에 알림이 등록됩니다.


#### 7. 송신 완료 처리
송신 작업 완료 시에도 IOCP 큐에 알림이 등록되며, 워커 스레드가 이를 처리합니다:
```cpp
if (overlappedEx->operationType == IOOperation::SEND) {
    std::cout << "Sent: " << overlappedEx->buffer << std::endl;
    // 다시 수신 요청
    WSARecv(clientSocket, &wsaBuf, 1, NULL, &flags, &overlapped, NULL);
}
```
- 송신이 완료되면 다시 `WSARecv()`를 호출하여 수신 작업을 재개합니다.

 

### 정리

위의 과정은 클라이언트와 연결이 유지되는 동안 반복됩니다:
1. 클라이언트가 데이터를 보내면, 수신 작업이 완료되어 서버에서 데이터를 처리하고 에코.
2. 에코를 위해 송신 작업을 요청.
3. 송신 작업 완료 후 다시 수신 작업 대기.

 
장점 
- **고성능**: 다수의 클라이언트를 비동기로 처리하여 CPU 사용률을 최소화합니다.
- **확장성**: IOCP는 수천 개의 동시 연결도 효율적으로 처리할 수 있습니다.
- **비동기 처리**: 데이터 송수신과 같은 I/O 작업이 비동기로 수행되어 스레드가 대기하지 않습니다.

