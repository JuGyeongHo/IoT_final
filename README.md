# IoT_final

***


Handong University IoT Final Project for the 1st semester of 2025


**Mesh Broker Network**

Task:
>1. Mosquitto broker 사이의 publisher and subscriber 메시지 공유 
>2. WiFi는 되는데 broker가 안되는 경우 확인
  
## 1,persistence session
**요약**
- 요약: redis cluster를 DB로 사용하여 publishser가 전송한 메시지의 구독자가 없을 경우 redis cluster에 client id와 payload를 저장하여 동일한 구독자(client id 일치)가 접속 할 경우 저장된 메시지를 구독자에게 전송하도록 하였다.
- 시도: bridge로 연결된 브로커의 경우 client가 접속하지 않았다면 redis에 값을 저장하여 구독자에게 전달하도록 하였다.
- 문제점: redis cluster가 단순 DB로 동작하며, 로컬에서 동작하기에 동기화 문제가 발생한다. 
- 해결책: redis cluster를 브로커 각각의 server에 돌리며 Master Slave 구조를 사용하여 서버를 동기화 시킨다. 

## 2.WiFi는 되는데 broker가 안되는 경우 탐지

**Solution: MQTT bridge사용**
>- 브로커의 '$SYS/broker/connection/#'으로 구독하여 broker의 bridge 연결 상태 확인 가능
