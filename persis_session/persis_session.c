//실행 명령어:
// LD_LIBRARY_PATH=/usr/local/lib ./persis_session

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <hiredis/hiredis.h>

#define MAX_CLIENTS 100

typedef struct {
    char id[64];
    int connected;
} ClientStatus;

ClientStatus clients[MAX_CLIENTS]; // 연결된 클라이언트 목록
int client_count = 0; // 현재 연결된 클라이언트
redisContext *redis = NULL; // redis 연결
// client가 연결되어 있는지 확인
int is_client_connected(const char *client_id) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, client_id) == 0) {
            return clients[i].connected;
        }
    }
    return 0;
}
// client 상태 갱신
void update_client_status(const char *client_id, int status) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, client_id) == 0) {
            clients[i].connected = status;
            return;
        }
    }
    if (client_count < MAX_CLIENTS) {
        strncpy(clients[client_count].id, client_id, sizeof(clients[client_count].id));
        clients[client_count].connected = status;
        client_count++;
    }
}
// redis에서 unsent 메시지를 클라이언트에게 전송
void publish_unsent_messages(struct mosquitto *mosq, const char *client_id) {
    redisReply *reply = redisCommand(redis, "LRANGE unsent:%s 0 -1", client_id);
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            char topic[128];
            snprintf(topic, sizeof(topic), "sensor/ack/%s", client_id);
            mosquitto_publish(mosq, NULL, topic, strlen(reply->element[i]->str), reply->element[i]->str, 1, false);
        }
    }
    freeReplyObject(reply);
    redisCommand(redis, "DEL unsent:%s", client_id);
}
// 구독
void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    mosquitto_subscribe(mosq, NULL, "sensor/#", 0);       // publish 토픽
    mosquitto_subscribe(mosq, NULL, "ack/#", 0);          // ACK
    mosquitto_subscribe(mosq, NULL, "notify/online", 0);  // 연결 알림
    mosquitto_subscribe(mosq, NULL, "notify/offline", 0); // 끊김 알림
}
// 메시지 수신
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    if (!msg || !msg->topic || !msg->payload) return;

    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;
    if (strcmp(msg->topic, "sensor/ack/#") == 0) {
        // ack 토픽이면 무시
        return;
    }
    if (strncmp(topic, "sensor/", 7) == 0) { // sensor/+/client
        const char *client_id = strrchr(topic, '/');
        if (!client_id || *(client_id + 1) == '\0') return;
        client_id++;
        printf("find client ... ");
        if (is_client_connected(client_id)) { //client connected
            printf("client connected");
            redisReply *r = redisCommand(redis, "EXISTS unsent:%s", client_id);
            if (r && r->type == REDIS_REPLY_INTEGER && r->integer > 0) {
                printf(" - stale Redis message found, deleting.\n");
                redisCommand(redis, "DEL unsent:%s", client_id);
            }
            if (r) freeReplyObject(r);
        } else { //client disconnected
            printf("client disconnected");
            redisCommand(redis, "RPUSH unsent:%s %s", client_id, payload); // store redis
        }
    }
    else if (strncmp(topic, "ack/", 4) == 0) { // Delete redis
        const char *client_id = topic + 4;
        printf("[ACK] Client %s received message. Deleting unsent buffer.\n", client_id);
        redisCommand(redis, "DEL unsent:%s", client_id);
    }
    else if (strcmp(topic, "notify/online") == 0) { // client connection
        printf("[INFO] Client %s is online. Sending buffered messages...\n", payload);
        update_client_status(payload, 1);
        publish_unsent_messages(mosq, payload);
    }
    else if (strcmp(topic, "notify/offline") == 0) { // client disconnection
        printf("[INFO] Client %s went offline.\n", payload);
        update_client_status(payload, 0);
    }
}
int main() {
    redis = redisConnect("0.0.0.0", 6379); //redis 연결
    if (redis == NULL || redis->err) {
        if (redis) {
            printf("Redis connection error: %s\n", redis->errstr);
            redisFree(redis);
        } else {
            printf("Redis connection allocation failed\n");
        }
        return 1;
    } else {
        printf("[INFO] Successfully connected to Redis port 6379\n");
    }

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("persis_session", false, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);
    //broker 연결
    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to connect to Mosquitto broker at localhost:1883\n");
        return 1;
    } else {
        printf("[INFO] Successfully connected to Mosquitto broker at localhost:1883\n");
    }

    printf("[INFO] MQTT client persis_session started. Listening for messages...\n");

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    redisFree(redis);
    return 0;
}