CC=gcc
CFLAGS=-Wall -I/usr/include/hiredis
LIBS=-lmosquitto -lhiredis

all: mqtt_redis_bridge

mqtt_redis_bridge: mqtt_redis_bridge.c
	$(CC) $(CFLAGS) -o mqtt_redis_bridge mqtt_redis_bridge.c $(LIBS)

clean:
	rm -f mqtt_redis_bridge