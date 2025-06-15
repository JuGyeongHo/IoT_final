/*
실행 
LD_LIBRARY_PATH=/usr/local/lib ./mqtt_redis 
*/

#include <mosquitto.h>
#include <hiredis_cluster/hircluster.h>
#include <stdio.h>
#include <string.h>

static redisClusterContext *redis_cluster = NULL; // Redis Cluster 연결 핸들

// MQTT 연결 콜백
void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    printf("Connected to MQTT broker with code %d\n", rc);

    mosquitto_subscribe(mosq, NULL, "test/topic", 0);
    mosquitto_subscribe(mosq, NULL, "test/topic2", 0);
    // Publish 하지 않아도 Redis Cluster에 메시지 정보 저장
    redisReply *reply = redisClusterCommand(redis_cluster,
        "XADD msg_stream * topic %s payload %s qos %d retain %d",
        "connect/test", "connected", 0, 0);

    if (reply == NULL) {
        fprintf(stderr, "Redis write in on_connect failed: %s\n", redis_cluster->errstr);
    } else {
        printf("Redis on_connect write OK: %lld fields set\n", reply->integer);
        freeReplyObject(reply);
    }
}

// MQTT 메시지 수신 콜백
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;
    int qos = msg->qos;
    int retain = msg->retain;

    printf("Received on topic %s: %s\n", topic, payload);

    // Redis Cluster에 메시지 정보 저장
    redisReply *reply = redisClusterCommand(redis_cluster,
        "XADD msg_stream * topic %s payload %s qos %d retain %d",
        topic, payload, qos, retain);

    if (reply == NULL) {
        fprintf(stderr, "Redis write failed: %s\n", redis_cluster->errstr);
    } else {
        printf("Redis write OK: %lld fields set\n", reply->integer);
        freeReplyObject(reply);
    }
}

int main() {
    mosquitto_lib_init();

    // Redis Cluster 연결
    redis_cluster = redisClusterContextInit();
    redisClusterSetOptionAddNode(redis_cluster, "192.168.102.1:7001");
    redisClusterSetOptionParseSlaves(redis_cluster);
    redisClusterConnect2(redis_cluster);

    if (redis_cluster == NULL || redis_cluster->err) {
        fprintf(stderr, "Redis Cluster connection error: %s\n",
                redis_cluster ? redis_cluster->errstr : "NULL");
        return 1;
    }

    // MQTT 클라이언트 생성
    struct mosquitto *mosq = mosquitto_new("mqtt_redis_client", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    // MQTT 브로커 연결
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