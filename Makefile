
COMMON_CONFIG = -DRATE_BPS=1000000000ULL
THREADS_CONFIG = -DTB_PRODUCER=2 -DTB_CONSUMER=8
DEBUG_CONFIG = -DTB_PRODUCER_DEBUG=0 -DTB_CONSUMER_DEBUG=1

CC = gcc
CFLAGS += --std=gnu99 -Wall -Werror -Wno-unused -O3 -pthread $(COMMON_CONFIG) $(THREADS_CONFIG) $(DEBUG_CONFIG)
HEADERS = $(wildcard *.h)
SRCS = $(wildcard *.c)

.PHONY: all test clean

all: $(HEADERS)
	$(CC) $(CFLAGS) -o test $(SRCS)

test: all
	./test

clean:
	rm -rf test *.o
