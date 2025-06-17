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

ClientStatus clients[MAX_CLIENTS];
int client_count = 0;
redisContext *redis = NULL;

int is_client_connected(const char *client_id) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, client_id) == 0) {
            return clients[i].connected;
        }
    }
    return 0;
}

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

void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    mosquitto_subscribe(mosq, NULL, "sensor/#", 0); // sensor
    mosquitto_subscribe(mosq, NULL, "$SYS/broker/connection/#", 0); // broker 
    mosquitto_subscribe(mosq, NULL, "ack/#", 0);  // ack
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;

    if (strncmp(topic, "sensor/", 7) == 0) {
        // 예: 토픽 "sensor/+/light" client id = light
        const char *client_id = strrchr(topic, '/') + 1;

        if (is_client_connected(client_id)) {
            mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, 1, false);
            char ack_topic[128];
            snprintf(ack_topic, sizeof(ack_topic), "ack/%s", client_id);

            mosquitto_publish(mosq, NULL, ack_topic, strlen("OK"), "OK", 1, false);
        } else {
            redisCommand(redis, "RPUSH unsent:%s %s", client_id, payload);
        }
    }
    if (strncmp(topic, "ack/", 4) == 0) {
        const char *client_id = topic + 4;
        printf("[ACK] Client %s received message. Deleting unsent buffer.\n", client_id);
        redisCommand(redis, "DEL unsent:%s", client_id);
    }
    if (strstr(topic, "$SYS/broker/connection/") && strstr(topic, "/state")) {
        const char *start = topic + strlen("$SYS/broker/connection/");
        const char *end = strstr(start, "/state");
        if (end) {
            char client_id[64] = {0};
            strncpy(client_id, start, end - start);
            int state = atoi(payload);
            update_client_status(client_id, state);
            if (state == 1) {
                publish_unsent_messages(mosq, client_id);
            }
        }
    }
}

int main() {
    redis = redisConnect("0.0.0.0", 6379);
    if (redis == NULL || redis->err) {
        if (redis) {
            printf("Redis connection error: %s\n", redis->errstr);
            redisFree(redis);
        } else {
            printf("Redis connection allocation failed\n");
        }
        return 1;
    }

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("persis_session", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to connect to broker\n");
        return 1;
    }

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    redisFree(redis);
    return 0;
}