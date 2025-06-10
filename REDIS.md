Eclipse **Mosquitto**에서 MQTT 메시지 및 세션을 **Redis Cluster 기반 persistence**로 관리하기 위한 **구현 방법**은 다음과 같은 단계를 따릅니다.

---

## ✅ 목표 재확인

* 여러 Mosquitto 브로커가 **Bridge**로 메시지를 공유
* 각 브로커가 **Redis Cluster**에 연결하여 메시지, 세션, 구독 정보를 저장/조회
* Mosquitto의 기본 파일 기반 persistence 대신 **Redis를 외부 persistence layer로 사용**
* 메시지 **중복 방지**, **세션 복원**, **retain 메시지 유지**, **QoS 처리**가 가능해야 함

---

# 🛠 Eclipse Mosquitto에서 Redis 기반 Persistence 구현하기

## 1. 📂 Mosquitto Plugin 기능 사용

Mosquitto는 **v2.0 이상부터 플러그인 아키텍처**를 지원하며, 다음과 같은 이벤트를 처리할 수 있습니다:

* `MOSQ_EVT_MESSAGE` → 메시지 publish 시
* `MOSQ_EVT_SUBSCRIBE` / `MOSQ_EVT_UNSUBSCRIBE`
* `MOSQ_EVT_DISCONNECT`, `MOSQ_EVT_CONNECT`

### 🔧 플러그인 개발 환경 준비

```bash
sudo apt install libmosquitto-dev cmake build-essential
```

---

## 2. 🧠 Redis 클러스터 연동을 위한 Mosquitto 플러그인 (C 예시)

### 📁 디렉터리 구조 예시

```
mosquitto_redis_plugin/
├── CMakeLists.txt
├── redis_plugin.c
```

---

### 📜 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(mosquitto_redis_plugin C)

add_library(redis_plugin SHARED redis_plugin.c)
target_link_libraries(redis_plugin mosquitto hiredis)
```

---

### 📜 `redis_plugin.c` (핵심 구현 스켈레톤)

```c
#include <mosquitto_broker.h>
#include <hiredis/hiredis.h>
#include <stdio.h>

static redisContext *redis;

int mosquitto_plugin_version(int supported_version_count, const int *supported_versions) {
    return 5; // Plugin API version
}

int mosquitto_plugin_init(mosquitto_plugin_id_t *identifier, void **user_data, struct mosquitto_opt *opts, int opt_count) {
    mosquitto_callback_register(identifier, MOSQ_EVT_MESSAGE, on_message, NULL, NULL);
    redis = redisConnect("127.0.0.1", 6379);
    if(redis == NULL || redis->err) {
        fprintf(stderr, "Redis connection error\n");
        return MOSQ_ERR_UNKNOWN;
    }
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_plugin_cleanup(void *user_data, struct mosquitto_opt *opts, int opt_count) {
    if(redis) redisFree(redis);
    return MOSQ_ERR_SUCCESS;
}

int on_message(int event, void *event_data, void *userdata) {
    struct mosquitto_evt_message *msg = event_data;

    redisCommand(redis, "HSET msg:%s topic %s payload %b qos %d retain %d",
                 msg->client_id, msg->topic, msg->payload, msg->payloadlen,
                 msg->qos, msg->retain);

    return MOSQ_ERR_SUCCESS;
}
```

> ✅ 확장 시 QoS 재처리, retain 메시지 구분 저장, session 관리 등을 이 코드에 추가 가능

---

## 3. 🔌 Mosquitto 설정에 Plugin 등록

`/etc/mosquitto/mosquitto.conf`에 다음을 추가:

```conf
plugin /path/to/libredis_plugin.so
plugin_opt_config_file /etc/mosquitto/plugin.conf
```

그리고 `/etc/mosquitto/plugin.conf`:

```conf
redis_host localhost
redis_port 6379
```

---

## 4. 🧠 Redis Cluster 구성 방법

### Docker 기반 Redis Cluster (간단 버전 예시)

```bash
docker network create redis-cluster

for port in 7001 7002 7003 7004 7005 7006; do
  docker run -d --net redis-cluster --name redis-$port -p $port:$port redis \
    redis-server --port $port --cluster-enabled yes \
    --cluster-config-file nodes.conf --cluster-node-timeout 5000
done

docker exec -it redis-7001 redis-cli --cluster create \
  redis-7001:7001 redis-7002:7002 redis-7003:7003 \
  redis-7004:7004 redis-7005:7005 redis-7006:7006 \
  --cluster-replicas 1
```

---

## 5. 🧪 Redis에 저장된 메시지 확인 예시

```bash
# 메시지 확인
redis-cli HGETALL msg:clientA

# retain 메시지
redis-cli GET retained:sensors/temp
```

---

## 6. 🎯 QoS 메시지와 세션 처리 확장 포인트

| 처리 대상              | 구현 포인트 (플러그인 내)                                       |
| ------------------ | ----------------------------------------------------- |
| **QoS 1/2 메시지 저장** | `on_message` → `msg:{client_id}:{packet_id}` 저장       |
| **중복 메시지 방지**      | Redis에 msg\_id 기록 후 중복 체크                             |
| **세션 유지/복원**       | `on_connect`, `on_disconnect` 이벤트에 Redis HSET/HGET 사용 |
| **Retain 메시지 처리**  | 토픽별로 `retained:{topic}` 키 사용                          |

---

## ✅ 결과적으로…

Eclipse Mosquitto에서 Redis Cluster를 이용한 persistence 및 session 처리를 위해선:

### 🔧 구현요소 요약

| 항목        | 방식                                 |
| --------- | ---------------------------------- |
| 메시지 저장    | Mosquitto 플러그인에서 Redis `HSET`      |
| 세션 유지     | 클라이언트 연결/해제 시 Redis `HSET` / `DEL` |
| 메시지 중복 방지 | Redis에 msg ID 키 저장 및 확인            |
| 구독 정보     | `SADD subs:{client_id}` 저장         |
| 분산 처리     | Redis Cluster가 자동 샤딩 및 고가용성 제공     |

---

## ➕ 보너스: 대안

* **플러그인 대신 외부 메시지 처리기 사용**

  * Mosquitto에서 Bridge 또는 Webhook으로 메시지를 외부 Python 앱에 전달
  * 이 앱이 Redis에 저장/조회 처리

---

필요하시면:

* ✅ 전체 플러그인 코드 + 빌드 스크립트
* ✅ Docker Compose 전체 구성 예제
* ✅ Python 기반 외부 처리기 예시

를 바로 제공해드릴 수 있습니다. 원하시는 항목 있으신가요?