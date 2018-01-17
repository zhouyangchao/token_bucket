#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "token_bucket.h"

#define CACHELINE 64

struct counter {
	pthread_t thread;
	uint64_t success_times;
	uint64_t failed_times;
	uint64_t send_bytes;
	uint64_t drop_bytes;
} __attribute__ ((aligned (CACHELINE)));

static struct token_bucket *s_tb;

static struct counter s_producer_counters[TB_PRODUCER];
static struct counter s_consumer_counters[TB_CONSUMER];

static void *producer(void *args)
{
#if TB_PRODUCER_DEBUG
	struct counter *counter = (struct counter *)args;
	uint64_t tokens;
#endif
	uint64_t add_tokens = s_tb->add_tokens / TB_PRODUCER;
	for (;;) {
#if TB_PRODUCER_DEBUG
		uint64_t tokens = token_bucket_add_tokens(s_tb, add_tokens);
		if (tokens == add_tokens) {
			counter->success_times++;
		} else {
			counter->failed_times++;
		}
		counter->send_bytes += add_tokens;
#else
		(void)token_bucket_add_tokens(s_tb, add_tokens);
#endif
		usleep(s_tb->interval * 1000);
	}
	return NULL;
}

static void *consumer(void *args)
{
	struct counter *counter = (struct counter *)args;
	enum token_bucket_action ret;
	for (;;) {
		ret = token_bucket_try_get_tokens(s_tb, PKT_LEN);
		if (ret == TB_SUCCESS) {
#if TB_CONSUMER_DEBUG
			counter->success_times++;
#endif
			counter->send_bytes += PKT_LEN;
		} else {
			/*sleep half of interval to wait new tokens*/
			usleep(s_tb->interval * 1000 / 2);
#if TB_CONSUMER_DEBUG
			counter->failed_times++;
#endif
			counter->drop_bytes += PKT_LEN;
		}
	}
	return NULL;
}

static void counter_dump_rate(const char *name, 
	const struct counter *new, const struct counter *old, uint64_t interval)
{
	printf("%s: %lu success, %lu failed, send %lu bps, drop %lu bps\n", name, 
		(new->success_times - old->success_times)/interval, 
		(new->failed_times - old->failed_times)/interval, 
		(new->send_bytes - old->send_bytes)/interval, 
		(new->drop_bytes - old->drop_bytes)/interval);
}

static void supervisor()
{
#define SUPERVISOR_INTERVAL (1ULL) //s
	int i, j = 0;
#if TB_PRODUCER_DEBUG
	struct counter producer_counters[2];
	struct counter *last_producer_sum, *producer_sum;
	memset(producer_counters, 0x0, sizeof(producer_counters));
#endif
#if TB_CONSUMER_DEBUG
	struct counter consumer_counters[2];
	struct counter *last_consumer_sum, *consumer_sum;
	memset(consumer_counters, 0x0, sizeof(consumer_counters));
#endif
	for (;;) {
#if TB_PRODUCER_DEBUG
		producer_sum = &producer_counters[(j)%2];
		last_producer_sum = &producer_counters[(j+1)%2];
		memset(producer_sum, 0x0, sizeof(struct counter));
		for (i = 0; i < TB_PRODUCER; ++i) {
			producer_sum->success_times += s_producer_counters[i].success_times;
			producer_sum->failed_times += s_producer_counters[i].failed_times;
			producer_sum->send_bytes += s_producer_counters[i].send_bytes;
			producer_sum->drop_bytes += s_producer_counters[i].drop_bytes;
		}
		counter_dump_rate("producer_rate", producer_sum, last_producer_sum, SUPERVISOR_INTERVAL);
#endif
#if TB_CONSUMER_DEBUG
		consumer_sum = &consumer_counters[(j)%2];
		last_consumer_sum = &consumer_counters[(j+1)%2];
		memset(consumer_sum, 0x0, sizeof(struct counter));
		for (i = 0; i < TB_CONSUMER; ++i) {
			consumer_sum->success_times += s_consumer_counters[i].success_times;
			consumer_sum->failed_times += s_consumer_counters[i].failed_times;
			consumer_sum->send_bytes += s_consumer_counters[i].send_bytes;
			consumer_sum->drop_bytes += s_consumer_counters[i].drop_bytes;
		}
		counter_dump_rate("consumer_rate", consumer_sum, last_consumer_sum, SUPERVISOR_INTERVAL);
#endif
//		printf("token bucket: %016lu, %016lu, %016lu, %016lu, %016lu.\n", 
//			s_tb->max_tokens, s_tb->cur_tokens, 
//			s_tb->rate_bps, s_tb->interval, s_tb->add_tokens);
		fflush(stdout);
		j++;
		usleep(SUPERVISOR_INTERVAL * 1000000);
	}
}

int main()
{
	int i, ret;
	s_tb = token_bucket_init(RATE_BPS, 100, 0.5f);
	if (s_tb == NULL) {
		printf("token bucket init failed.\n");
		return 1;
	}
	printf("token bucket init success.\n");
	for (i = 0; i < TB_PRODUCER; ++i) {
		ret = pthread_create(&s_producer_counters[i].thread, NULL, 
			producer, &s_producer_counters[i]);
		if (ret < 0) {
			printf("errno %d: %s.\n", errno, strerror(errno));
		}
	}
	fflush(NULL);
	for (i = 0; i < TB_CONSUMER; ++i) {
		ret = pthread_create(&s_consumer_counters[i].thread, NULL, 
			consumer, &s_consumer_counters[i]);
		if (ret < 0) {
			printf("errno %d: %s.\n", errno, strerror(errno));
		}
	}
	printf("producer %d, consumer %d.\n", TB_PRODUCER, TB_CONSUMER);
	fflush(NULL);
	supervisor();
	token_bucket_destroy(s_tb);
	return 0;
}