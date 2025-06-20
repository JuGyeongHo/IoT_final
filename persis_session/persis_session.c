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
} ClientStatus; // client 상태

ClientStatus clients[MAX_CLIENTS]; // 연결된 client
int client_count = 0; // 현재 클라이언트 
redisContext *redis = NULL; /// redis 핸들
//client 연결 체크
int is_client_connected(const char *client_id) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, client_id) == 0) {
            return clients[i].connected;
        }
    }
    return 0;
}
//client 상태 업데이트
void update_client_status(const char *client_id, int status) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, client_id) == 0) { //client 연결 조회
            clients[i].connected = status;
            return;
        }
    }
    if (client_count < MAX_CLIENTS) { // 상태 업데이트
        strncpy(clients[client_count].id, client_id, sizeof(clients[client_count].id));
        clients[client_count].connected = status;
        client_count++;
    }
}
//redis에서 메시지 가져오기
int get_redis(const char *client_id, char *buffer, size_t buffer_size) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "redis-cli HGET unsent %s", client_id); // redis 값 가져오기

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen failed");
        return 0;
    }
    if (fgets(buffer, buffer_size, fp) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';  // 개행 제거
        }
        // Redis에서 값이 없으면 0
        if (strcmp(buffer, "") == 0) {
            pclose(fp);
            return 0;
        }
        // printf("get value: %s\n",buffer);
        pclose(fp);
        return 1;
    }
    pclose(fp);
    return 0;
}
// 미전송 메시지 전송
void publish_unsent_messages(struct mosquitto *mosq, const char *client_id) { 
    char message[256];
    if (get_redis(client_id, message, sizeof(message))) {
        char topic[128];
        snprintf(topic, sizeof(topic), "sensor/ack/%s", client_id);
        mosquitto_publish(mosq, NULL, topic, strlen(message), message, 1, false); // 메시지 전송
        printf("[INFO] Publishing topic: %s client %s msg %s\n", topic, client_id , message);

        // ACK 전송
        char ack_topic[128];
        snprintf(ack_topic, sizeof(ack_topic), "ack/%s", client_id);
        const char *ack_msg = "OK";
        mosquitto_publish(mosq, NULL, ack_topic, strlen(ack_msg), ack_msg, 1, false);
    } else {
        printf("[INFO] No buffered message found for client %s (via redis-cli)\n", client_id);
    }
    redisCommand(redis, "HDEL unsent %s", client_id);
}

void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    mosquitto_subscribe(mosq, NULL, "sensor/#", 0);       // publish 토픽
    mosquitto_subscribe(mosq, NULL, "ack/#", 0);          // ACK
    mosquitto_subscribe(mosq, NULL, "notify/online", 0);  // 연결 알림
    mosquitto_subscribe(mosq, NULL, "notify/offline", 0); // 끊김 알림
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    if (!msg || !msg->topic || !msg->payload) return;

    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;
    if (strncmp(topic, "sensor/ack/", strlen("sensor/ack/")) == 0){ // 재전송은 무시
        return; 
    }
    if (strncmp(topic, "sensor/", 7) == 0) { // 메시지 전송
        const char *client_id = strrchr(topic, '/');
        if (!client_id || *(client_id + 1) == '\0') return;
        client_id++;
        printf("senssor topic sending ... \n");
        if (!is_client_connected(client_id)) {
            printf("client disconnected\n");
            redisCommand(redis, "HSET unsent %s %s", client_id, payload);
        }
    }
    else if (strncmp(topic, "ack/", 4) == 0) { // redis 삭제
        const char *client_id = topic + 4;
        printf("[ACK] Client %s received message. Deleting unsent buffer.\n", client_id);
        redisCommand(redis, "HDEL unsent %s", client_id);
    }
    else if (strcmp(topic, "notify/online") == 0) { // client 연결됨
        printf("[INFO] Client %s is online. Sending buffered messages...\n", payload);
        update_client_status(payload, 1);
        publish_unsent_messages(mosq, payload);
    }
    else if (strcmp(topic, "notify/offline") == 0) { // client 연결 끊김
        printf("[INFO] Client %s went offline.\n", payload);
        update_client_status(payload, 0);
    }
}
int main() {
    redis = redisConnect("0.0.0.0", 6379); // Redis 연결
    if (redis == NULL || redis->err) {
        if (redis) {
            printf("Redis connection error: %s\n", redis->errstr);
            redisFree(redis);
        } else { 
            printf("Redis connection allocation failed\n");
        }
        return 1;
    } else {
        printf("[INFO] Successfully connected to Redis at 6379\n");
    }

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("persis_session", false, NULL); 
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS) { // broker 연결
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