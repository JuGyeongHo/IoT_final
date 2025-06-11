# Redis Cluster with MQTT

## 1. 설치 의존성
```bash
sudo apt update
sudo apt install libmosquitto-dev libhiredis-dev build-essential git cmake
```
---
## 2. C 코드 예시(mqtt_redis_bridge.c)
- mqtt_redis_bridge.c 생성
```c
#include <mosquitto.h> // MQTT library
#include <hiredis/hiredis.h> // Redis library
#include <stdio.h>
#include <string.h>

static redisContext *redis; // redis connect handle
//MQTT 연결
void on_connect(struct mosquitto *mosq, void *userdata, int rc) { 
    printf("Connected to MQTT broker with code %d\n", rc);
    mosquitto_subscribe(mosq, NULL, "test/topic", 0);
}
// MQTT 시지 수신
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;
    int qos = msg->qos;
    int retain = msg->retain;

    printf("Received on topic %s: %s\n", topic, payload);
    //Redis에 메시지 정보 저장
    redisReply *reply = redisCommand(redis,
        "HSET msg:clientA topic %s payload %s qos %d retain %d",
        topic, payload, qos, retain);

    if (reply == NULL) {
        fprintf(stderr, "Redis write failed: %s\n", redis->errstr);
    } else {
        printf("Redis write OK: %lld fields set\n", reply->integer);
        freeReplyObject(reply);
    }
}

int main() {
    mosquitto_lib_init();

    redis = redisConnect("127.0.0.1", 7001); // Redis cluster 7001포트  노드
    if (redis == NULL || redis->err) {
        fprintf(stderr, "Redis connection error: %s\n", redis ? redis->errstr : "NULL");
        return 1;
    }
    // MQTT client
    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return 1;
    }
    
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);
    //Broker 연결
    if (mosquitto_connect(mosq, "localhost", 1883, 60)) {
        fprintf(stderr, "Unable to connect to MQTT broker\n");
        return 1;
    }
    
    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    redisFree(redis);
    return 0;
}
```
---
## 3. 빌드 방법(Makefile)
- Makefile 생성
```makefile
CC=gcc
CFLAGS=-Wall -I/usr/include/hiredis
LIBS=-lmosquitto -lhiredis

all: mqtt_redis_bridge

mqtt_redis_bridge: mqtt_redis_bridge.c
	$(CC) $(CFLAGS) -o mqtt_redis_bridge mqtt_redis_bridge.c $(LIBS)

clean:
	rm -f mqtt_redis_bridge
```

- 빌드 실행
```bash
make
```
---
## 4. 실행 방법
- mqtt_redis_bridge.c 컴파일
```bash
./mqtt_redis_bridge
```

- 출력 예시
```m
Received on topic test/topic: hello
```
---
## 5. Redis server 실행 
- 서버 실행 확인
```bash
docker ps
```

- 컨테이너 생성 
```bash
docker network create redis-cluster

for port in 7001 7002 7003 7004 7005 7006; do
  docker run -d --name redis-$port --net redis-cluster -p $port:$port redis \
    redis-server --port $port --cluster-enabled yes \
    --cluster-config-file nodes.conf --cluster-node-timeout 5000 --appendonly yes
done
```

- cluster 구성 명령어
```bash
docker exec -it redis-7001 redis-cli --cluster create \
  redis-7001:7001 redis-7002:7002 redis-7003:7003 \
  redis-7004:7004 redis-7005:7005 redis-7006:7006 \
  --cluster-replicas 1
```

- 서버 실행(도커-7001 port)
```bash
docker start redis-7001
```

- 직접 실행
```bash
redis-server
```
---
## 6. Test
- mosquitto.conf 파일 수정 및 빌드
```bash
port 1883
protocol mqtt

allow_anonymous true
```

mosquitto broker 실행
```bash
mosquitto -c ~/mosquitto/mosquitto.conf
```


- mosquitto_sub으로 메시지 구독
```bash
mosquitto_sub -h localhost -t test/topic -v
```

- mosquitto_pub으로 메시지 발행
```bash
mosquitto_pub -h localhost -t test/topic -m "hello world"
```

- redis cluster 확인
```bash
redis-cli -c -p 7001 HGETALL msg:clientA
```

출력 확인
```m
1 "topic"
2 "test/topic"
3 "payload"
4 "hello"
5 "qos"
6 "0"
7 "retain"
8 "0"
```
