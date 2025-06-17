# Redis Cluster with MQTT

## 1. 설치 의존성
```bash
sudo apt update
sudo apt install libmosquitto-dev libhiredis-dev libevent-dev build-essential git cmake
```

hiredis-cluster 설치(실시간 적용)
```bash
git clone https://github.com/Nordix/hiredis-cluster.git
cd hiredis-cluster
```
```bash
mkdir build
cd build
cmake ..
make
sudo make install
```
## 2.redis-server 실행
의존성 설치
```bash
sudo apt update && sudo apt install -y redis-server
```
- /etc/redis/redis.conf 파일 설정
```bash
port 7001 # 각 노드별 고유 포트 지정
cluster-enabled yes
cluster-config-file nodes.conf
cluster-node-timeout 5000
appendonly yes
protected-mode no
bind 0.0.0.0 # 외부 접속 허용
```
redis server 재시작
```bash
sudo systemctl restart redis-server
```
redis node 생성
```bash
redis-cli --cluster create \
192.168.100.1:7001 192.168.100.2:7002 192.168.100.3:7003 \
--cluster-replicas 1
```
## 3.redis server에 저장 C 코드
C 파일 예시(mqtt_redis_sub.c)
```c
/*
실행 명령어:
LD_LIBRARY_PATH=/usr/local/lib ./mqtt_redis_sub
*/

#include <mosquitto.h>
#include <hiredis_cluster/hircluster.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static redisClusterContext *redis_cluster = NULL; // Redis Cluster 연결 핸들
static const char *client_id = "mqtt_redis_client"; // 고정 client_id 사용

// 현재 시간 문자열 반환 함수
void get_time_str(char *buffer, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, len, "%Y-%m-%d %H:%M:%S", t);
}

// MQTT 연결 콜백
void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    printf("Connected to MQTT broker with code %d\n", rc);

    mosquitto_subscribe(mosq, NULL, "test/topic", 0);
    mosquitto_subscribe(mosq, NULL, "test/topic2", 0);

    char now[64];
    get_time_str(now, sizeof(now));

    // 세션 상태 저장 (Stream + Hash 동시)
    redisReply *reply = redisClusterCommand(redis_cluster,
        "XADD session_stream * client %s status connected time \"%s\"",
        client_id, now);
    if (reply == NULL) {
        fprintf(stderr, "Redis on_connect XADD failed: %s\n", redis_cluster->errstr);
    } else {
        freeReplyObject(reply);
    }

    reply = redisClusterCommand(redis_cluster,
        "HSET session:%s status connected last_seen \"%s\"", client_id, now);
    if (reply == NULL) {
        fprintf(stderr, "Redis on_connect HSET failed: %s\n", redis_cluster->errstr);
    } else {
        freeReplyObject(reply);
    }
}
// MQTT 메시지 수신 콜백
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;
    int qos = msg->qos;
    int retain = msg->retain;

    char now[64];
    get_time_str(now, sizeof(now));

    printf("Received on topic %s: %s\n", topic, payload);

    // Redis에 메시지 기록 (Stream 사용)
    redisReply *reply = redisClusterCommand(redis_cluster,
        "XADD msg_stream * client %s topic %s payload %s qos %d retain %d time \"%s\"",
        client_id, topic, payload, qos, retain, now);
    if (reply == NULL) {
        fprintf(stderr, "Redis message write failed: %s\n", redis_cluster->errstr);
    } else {
        freeReplyObject(reply);
    }
}

int main() {
    mosquitto_lib_init();

    redis_cluster = redisClusterContextInit();
    redisClusterSetOptionAddNode(redis_cluster, "192.168.100.1:7001");
    redisClusterSetOptionParseSlaves(redis_cluster);
    redisClusterConnect2(redis_cluster);

    if (redis_cluster == NULL || redis_cluster->err) {
        fprintf(stderr, "Redis Cluster connection error: %s\n",
                redis_cluster ? redis_cluster->errstr : "NULL");
        return 1;
    }

    struct mosquitto *mosq = mosquitto_new(client_id, true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60)) {
        fprintf(stderr, "Unable to connect to MQTT broker\n");
        return 1;
    }

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    redisClusterFree(redis_cluster);

    return 0;
}
```
```makefile
# 소스 및 타겟
SRC = mqtt_redis_sub.c
TARGET = mqtt_redis_sub

# 컴파일러 및 플래그
CC = gcc
CFLAGS = -Wall -g -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lhiredis_cluster -lhiredis -lmosquitto

# 기본 빌드 타겟
$(TARGET): $(SRC)
        $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 청소 명령
clean:
        rm -f $(TARGET)

.PHONY: clean
```
## 4. 빌드 및 실행
빌드 및 실행
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
```bash
make
```
```bash
LD_LIBRARY_PATH=/usr/local/lib ./mqtt_redis_sub
```
실행결과 확인
## 4. 다른 Broker에서 확인

**Redis Cluster Reader**
저장된 DB형태로 메시지를 확인 /home/raspberry/redis_reader
```c
#include <stdio.h>
#include <stdlib.h>
#include <hiredis_cluster/hircluster.h>

int main() {
    redisClusterContext *cc = redisClusterContextInit();
    redisClusterSetOptionAddNode(cc, "192.168.100.1:7001");
    redisClusterSetOptionParseSlaves(cc);
    redisClusterConnect2(cc);

    if (cc == NULL || cc->err) {
        fprintf(stderr, "Redis Cluster connection error: %s\n",
                cc ? cc->errstr : "NULL");
        return 1;
    }

    // Redis Stream 읽기 (최근 메시지 가져오기)
    redisReply *reply = redisClusterCommand(cc,
        "XREVRANGE msg_stream + - COUNT 5");

    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY) {
        fprintf(stderr, "Stream read failed\n");
        redisClusterFree(cc);
        return 1;
    }

    printf("Latest messages:\n");
    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply *entry = reply->element[i];
        if (entry->type == REDIS_REPLY_ARRAY && entry->elements == 2) {
            printf("ID: %s\n", entry->element[0]->str);
            redisReply *fields = entry->element[1];
            for (size_t j = 0; j < fields->elements; j += 2) {
                printf("  %s: %s\n",
                    fields->element[j]->str,
                    fields->element[j + 1]->str);
            }
        }
    }

    freeReplyObject(reply);
    redisClusterFree(cc);
    return 0;
}
```
```makefile
# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -g -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lhiredis_cluster -lhiredis

# 파일 이름
TARGET = stream_reader
SRC = stream_reader.c

# 기본 빌드 대상
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 정리
clean:
	rm -f $(TARGET)

.PHONY: all clean
```
빌드
```bash
make
```
```bash
LD_LIBRARY_PATH=/usr/local/lib ./redis_reader
```

mosquitto_sub으로 메시지 구독
```bash
mosquitto_sub -h 192.168.100.1 -t test/topic -v
```

- mosquitto_pub으로 메시지 발행
```bash
mosquitto_pub -h 192.168.100.1 -t test/topic -m "hello world"
```

```bash
redis-cli -c -h 192.168.100.1 -p 7001
192.168.100.1:7001>XRANGE session_stream - +
172.18.0.4:7003> HGETALL session:mqtt_redis_client
```
결과 출력
```m
1) 1) "1749959528503-0"
   2) 1) "client"
      2) "mqtt_redis_client"
      3) "status"
      4) "connected"
      5) "time"
      6) "\"2025-06-15 12:52:08\""
```
```m
1) "status"
2) "connected"
3) "last_seen"
4) "\"2025-06-15 12:52:08\""
```

# Redis Cluster MQTT의 효용성
현재까지 설계 방향은 Redis Cluster는 구독자로 브로커에 연결하고 수신한 토픽, 메시지 등을 저장한다. 
이 방식은 브로커 간의 직접적인 연결 없이, Redis Cluster를 저장소로 활용하는 구조이다. 각 브로커가 Redis Cluster와 연결하는 방식은 데이터가 분산 처리되는 구조이며, 클라이언트가 어떤 브로커에 연결되어 있든, Redis에서 데이터를 가져올 수 있다. 
만약, 연결된 브로커와의 연결이 좋지 않아 장애가 발생할 경우 Redis Cluster의 다른 노드를 통하여 데이터를 가져와 연결을 유지할 수 있다. 
hiredis cluster 사용으로 실시간 적으로 DB에 저장이 가능하다.
## ✅ hiredis-cluster 사용의 장점 요약
1. 	분산 Redis 저장소와 연결
>•	단일 Redis가 아닌 클러스터 전체 노드에 분산 저장되므로, 대량 메시지도 병목 없이 처리 가능.
2.	자동 슬롯 관리
>•	hiredis-cluster는 키 해시 슬롯 계산 및 노드 재접속을 자동 처리 → 개발자가 Redis 분산 구조를 직접 고려하지 않아도 됨.
3.	Stream 기반 메시지 저장
>•	MQTT 메시지를 Redis XADD 명령으로 시계열 로그처럼 저장 가능.
>•	이후 XRANGE, XREAD로 다중 소비자가 시간순으로 처리 가능.
4.	단순한 코드 구조
>•	redisClusterCommand() 한 줄로 클러스터에 안전하게 커맨드 전달 가능.
>•	연결이 끊겨도 재시도/리다이렉션 자동 처리.
5.	확장성 및 유연성
>•	메시지 로그뿐 아니라, 인증 토큰, 세션 관리, 디바이스 상태 등 다양한 MQTT 응용에 Redis 자료구조 활용 가능 (Hash, List, Pub/Sub 등).
## 시도 및 겪은 이슈
- 시도: 브로커마다 redis cluster를 사용하여 실시간으로 client id와 메시지를 redis server에 분산하여 저장한다. 연결된 브로커가  redis server를 통해 데이터를 공유하고, 연결이 끊겼을 경우 저장된 내용을 통해 세션을 유지하며 client로 전송한다.
- 이슈: 브로커의 network가 down 되면 다른 브로커에서 저장된 redis cluster의 DB에 접근 할 수가 없음 
- 해결책: mosquitto persistence 원리를 이용하여 qos1/2로 메시지를 전송하면 브로커에서 자동으로 세션과 메시지를 저장하여 별도의 공유를 할 필요가 없음