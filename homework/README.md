# 서로 다른 Mosquitto broker 사이의 publisher and subscriber 메시지 공유 확인
***
**Case: Bridge 설정을 확인**


>1. Broker A와 Broker B가 연결 됨
>2. Publisher가 Broker A로 publish
>3. Subscriber가 Broker B로 subscribe
>4. Broker A->B에서 publisher/subscriber의 메시지가 전달
>5. Broker Log를 통해 Client ID,Topic,QoS Level, Session 등을 확인 

***
**Case: Broker B에서 메시지를 확인**

>1. Broker A와 Broker B가 연결 됨
>2. Publisher가 Broker A로 publish
>3. Subscriber가 Broker A로 subscribe
>4. Broker A와 연결된 Broker B에서 publisher/subscriber 메시지를 확인
>5. Broker Log를 통해 Client ID,Topic,QoS Level, Session 등을 확인 
***
# WiFi는 되는데 broker가 안되는 경우 탐지
***
**Case 1: Broker A 연결 도중 Broker A Down**


>1. 2개의 라즈베리파이(A,B)가 AP, Broker로 동작 및 연결 중
>2. Publisher는 Broker A와 연결해서 Broker B의 Subscriber로 메시지 전송
>3. 연결 도중 Broker A가 Down
>4. Publisher and Subscriber 는 A의 상태가 Down 되었음을 확인
***
**Case 2: Broker A 연결 도중 Broker B Down**


>1. 2개의 라즈베리파이(A,B)가 AP, Broker로 동작 및 연결 중
>2. Publisher는 Broker A와 연결해서 Broker B의 Subscriber로 메시지 전송
>3. 연결 도중 Broker B가 Down
>4. Publisher and Subscriber 는 B의 상태가 Down 되었음을 확인

