# IoT_final

***


Handong University IoT Final Project for the 1st semester of 2025


**Mesh Broker Network**

Task:
>1. Mosquitto broker 사이의 publisher and subscriber 메시지 공유 
>2. WiFi는 되는데 broker가 안되는 경우 확인
  
# 서로 다른 Mosquitto broker 사이의 publisher and subscriber 메시지 공유 확인
***

# Redis Cluster를 사용하여 메시지 공유 확인

>1. Broker A와 Broker B가 같은 네트워크
>2. Client가 Broker A에서 메시지 pub/sub
>3. Broker A는 Redis Cluster(port 7001)에 메시지 저장
>4. Broker B가 Broker A의 Redis Cluster에 접속
>5. Client ID,Topic,QoS Level, Session 등을 확인 
***
# WiFi는 되는데 broker가 안되는 경우 탐지

**Case 1: Broker A 연결 도중 Broker A Down**


>1. 2개의 라즈베리파이(A,B)가 AP, Broker로 동작 및 연결 중
>2. Publisher는 Broker A와 연결해서 Broker B의 Subscriber로 메시지 전송
>3. 연결 도중 Broker A가 Down
>4. Publisher and Subscriber 는 A의 상태가 Down 되었음을 확인

**Case 2: Broker A 연결 도중 Broker B Down**


>1. 2개의 라즈베리파이(A,B)가 AP, Broker로 동작 및 연결 중
>2. Publisher는 Broker A와 연결해서 Broker B의 Subscriber로 메시지 전송
>3. 연결 도중 Broker B가 Down
>4. Publisher and Subscriber 는 B의 상태가 Down 되었음을 확인

**Solution 1: MQTT bridge + heartbeat topic 사용**

>1. A 브로커가 B브로커와 bridge로 연결되어 있으며, B브로커에서 일정 주기로 b/status/heratbeat 토픽을 publish
>2. A 브로커에 연결된 클라이언트가 이 토픽을 구독하여 생존을 확인

**Solution 2: A브로커에서 B브로커의 상태를 ping하는 백엔드 서비스**
>1. A 브로커에 연결된 클라이언트가 직접 B 브로커의 IP/Port로 TCP 연결을 시도하거나 A 브로커에 붙은 backend가 B 브로커의 상태를 Redis 등에 업데이트
>2. 클라이언트는 Redis나 MQTT상태 토픽을 통해 확인
>3. 이 결과를 Redis에 업데이트하거나, A 브로커에서 b/status 토픽으로 broadcast.

**Solution 3: Redis Cluster 상태 활용 (MQTT-Redis)**
>1. A 브로커와 B 브로커가 각각 Redis cluster 노드에 연동
>2. 클라이언트는 Redis cluster에서 B 브로커 관련 메시지 키 생성 여부를 확인