# 소스 및 타겟
SRC = mqtt_redis_sub.c
TARGET = mqtt_redis_sub

# 컴파일러 및 플래그
CC = gcc
CFLAGS = -Wall -g -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lhiredis_cluster -lhiredis -lmosquitto

# 기본 빌드 타겟
$(TARGET): $(SRC)
        $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 청소 명령
clean:
        rm -f $(TARGET)

.PHONY: clean


