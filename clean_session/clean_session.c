/*
파일 수정
sudo vim ~/mosquitto/mosquitto.conf

topic notify/received/# in 0
persistence true
persistence_location /var/lib/mosquitto/
sys_interval 5  
*/
#include <stdio.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <string.h>

// 특정 client의 세션 내 메시지를 조회하여 payload가 존재하는지 확인
int check_persist_message(const char *client_id, const char *expected_payload) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mosquitto_ctrl session show %s -p 1883", client_id); // client id의 session 조회

    FILE *fp = popen(cmd, "r"); // session file 오픈
    if (!fp) {
        perror("popen failed");
        return 0;
    }

    char line[512];
    //일치하는 메시지를 확인
    while (fgets(line, sizeof(line), fp)) { 
        if (strstr(line, expected_payload)) {
            pclose(fp);
            return 1; // 발견됨
        }
    }
    pclose(fp);
    return 0; 
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    const char *topic = msg->topic;
    const char *payload = (const char *)msg->payload;

    if (strstr(topic, "notify/received/") == topic) {
        const char *client_id = topic + strlen("notify/received/");
        printf("Notify from client [%s], payload: [%s]\n", client_id, payload);

        if (check_persist_message(client_id, payload)) {
            printf("Message found in session. Disconnecting client [%s]...\n", client_id);

            char cmd[256];
            snprintf(cmd, sizeof(cmd), "mosquitto_ctrl session disconnect %s -p 1883", client_id);//session 삭제
            int ret = system(cmd);//실행
            if (ret != 0) {
                fprintf(stderr, "Failed to execute: %s\n", cmd);//실행 실패
            }
        } else {
            printf("No matching message found in session for client [%s].\n", client_id);//다른 메시지 전송
        }
    }
    if (strstr(msg->topic, "$SYS/broker/connection/") &&
        strstr(msg->topic, "/state")) {

        // 예: "$SYS/broker/connection/MeshBroker2/state"
        const char *bridge_name_start = msg->topic + strlen("$SYS/broker/connection/");
        const char *bridge_name_end = strstr(bridge_name_start, "/state");

        char bridge_name[128] = {0};
        strncpy(bridge_name, bridge_name_start, bridge_name_end - bridge_name_start);

        int state = atoi((char *)msg->payload);
        if(state == 0){
            printf("%s Broker bridge down\n",bridge_name);
        }
    }
}

int main() {
    mosquitto_lib_init();

    struct mosquitto *mosq = mosquitto_new("session_cleaner", false, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create Mosquitto instance\n");
        return 1;
    }

    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to connect to broker.\n");
        return 1;
    }

    mosquitto_subscribe(mosq, NULL, "notify/received/#", 0);
    mosquitto_subscribe(mosq, NULL, "$SYS/broker/connection/#", 0);


    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}