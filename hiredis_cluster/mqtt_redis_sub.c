/*
실행 명령어:
LD_LIBRARY_PATH=/usr/local/lib ./mqtt_redis
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
    redisClusterSetOptionAddNode(redis_cluster, "192.168.102.1:7001");
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