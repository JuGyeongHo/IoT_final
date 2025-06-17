/*
파일 수정
sudo vim ~/mosquitto/mosquitto.conf

topic notify/received/# in 0
persistence true
persistence_location /var/lib/mosquitto/
sys_interval 5  

실행
gcc clean_session.c -o clean_session -lmosquitto
*/
#include <stdio.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <string.h>


void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {

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

    // mosquitto_subscribe(mosq, NULL, "notify/received/#", 0);
    mosquitto_subscribe(mosq, NULL, "$SYS/broker/connection/#", 0);


    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}