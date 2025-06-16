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