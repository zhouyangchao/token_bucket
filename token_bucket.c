#include <stdlib.h>
#include <unistd.h>
#include "token_bucket.h"

#ifndef TB_MALLOC
#define TB_MALLOC(s) malloc(s)
#endif

#ifndef TB_FREE
#define TB_FREE(s) free(s)
#endif

#ifndef TB_USLEEP
#define TB_USLEEP(us) usleep(us)
#endif

/* create a token bucket
 * param bps : the bytes/second of the token bucket
 * param interval : the interval(ms) to add tokens
 * param scale : the scale tokens to handle burst traffic
 * thread unsafety
 * return the actual number added*/
struct token_bucket *token_bucket_init(uint64_t bps, uint64_t interval, float scale)
{
	struct token_bucket *tb;
	if (unlikely(interval == 0 || scale < 0)) {
		return NULL;
	}
	tb = TB_MALLOC(sizeof(struct token_bucket));
	if (unlikely(tb == NULL)) {
		return NULL;
	}
	tb->max_tokens = bps * (scale + 1);
	tb->cur_tokens = bps;
	tb->rate_bps = bps;
	tb->interval = interval;
	tb->add_tokens = (bps * interval) / 1000;
	return tb;
}

/* destroy the token bucket
 * thread unsafety */
void token_bucket_destroy(struct token_bucket *tb)
{
	if (tb) {
		TB_FREE(tb);
	}
}

/* add token to bucket
 * thread safety
 * return the actual number added*/
uint64_t token_bucket_add_tokens(struct token_bucket *tb, uint64_t tokens)
{
	uint64_t old_tokens;
	uint64_t new_tokens;
	do {
		old_tokens = tb->cur_tokens;
		new_tokens = old_tokens + tokens;
		if (unlikely(new_tokens > tb->max_tokens)) {
			new_tokens = tb->max_tokens;
		}
	} while (!atomic_cas(&tb->cur_tokens, old_tokens, new_tokens));
	return new_tokens - old_tokens;
}

/* get tokens from token bucket, may block
 * thread safety
 * return 
 *     TB_INVAL : tokens is more than max tokens
 *     TB_SUCCESS  : get tokens success*/
enum token_bucket_action 
token_bucket_get_tokens(struct token_bucket *tb, uint64_t tokens)
{
	uint64_t old_tokens, new_tokens;
	if (unlikely(tokens > tb->max_tokens)) {
		return TB_INVAL;
	}
	do {
		old_tokens = tb->cur_tokens;
		while (unlikely(old_tokens < tokens)) {
			/*sleep half of interval to wait new tokens*/
			TB_USLEEP(tb->interval * 1000 / 2);
			old_tokens = tb->cur_tokens;
		}
		new_tokens = old_tokens - tokens;
	} while (!atomic_cas(&tb->cur_tokens, old_tokens, new_tokens));
	return TB_SUCCESS;
}

/* get tokens from token bucket, return immediately
 * thread safety
 * return 
 *     TB_INVAL : tokens is more than max tokens
 *     TB_SUCCESS  : get tokens success
 *     TB_FAILED  : get tokens failed*/
enum token_bucket_action 
token_bucket_try_get_tokens(struct token_bucket *tb, uint64_t tokens)
{
	uint64_t old_tokens, new_tokens;
	if (unlikely(tokens > tb->max_tokens)) {
		return TB_INVAL;
	}
	do {
		old_tokens = tb->cur_tokens;
		if (unlikely(old_tokens < tokens)) {
			return TB_FAILED;
		}
		new_tokens = old_tokens - tokens;
	} while (!atomic_cas(&tb->cur_tokens, old_tokens, new_tokens));
	return TB_SUCCESS;
}
