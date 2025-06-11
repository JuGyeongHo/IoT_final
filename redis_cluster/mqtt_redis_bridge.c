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