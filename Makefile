
CPUS = $(shell cat /proc/cpuinfo | grep "^processor" | wc -l)
COMMON_CONFIG = -DRATE_BPS=100000000ULL -DPKT_LEN=64
THREADS_CONFIG = -DTB_PRODUCER=2 -DTB_CONSUMER=$(CPUS)
DEBUG_CONFIG = -DTB_PRODUCER_DEBUG=0 -DTB_CONSUMER_DEBUG=1

CC = gcc
CFLAGS += --std=gnu11 -Wall -Wextra -Wno-unused-parameter -Werror -O3
CFLAGS += -pthread $(COMMON_CONFIG) $(THREADS_CONFIG) $(DEBUG_CONFIG)
HEADERS = $(wildcard *.h)
SRCS = $(wildcard *.c)

.PHONY: all test clean

all: $(HEADERS)
	$(CC) $(CFLAGS) -o test $(SRCS)

test: all
	./test

clean:
	rm -rf test *.o
