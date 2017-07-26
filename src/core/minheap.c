//
// Created by zhsyourai on 7/26/17.
//
#include "zRPC/support/lock.h"
#include "timer_engine.h"
#include "timer_heap.h"

void *initialize();

int add(void *engine_context, zRPC_timer *timer);

int del(void *engine_context, zRPC_timer *timer);

int32_t dispatch(void *engine_context, zRPC_timer **results[], size_t *nresults);

void release(void *engine_context);

typedef struct zRPC_minheap_timer_context {
    zRPC_mutex mutex;
    zRPC_mutex run_mutex;
    zRPC_timer_heap heap;
    zRPC_timespec min_deadline;
} zRPC_minheap_timer_context;

const zRPC_timer_engine_vtable minheap_timer_engine_vtable = {
    "minheap",
    initialize,
    add,
    del,
    dispatch,
    release
};

void *initialize() {
  zRPC_minheap_timer_context *context = malloc(sizeof(zRPC_minheap_timer_context));
  zRPC_timer_heap_init(&context->heap);
  zRPC_mutex_init(&context->mutex);
  zRPC_mutex_init(&context->run_mutex);
  context->min_deadline = zRPC_time_inf_future(zRPC_CLOCK_MONOTONIC);
  return context;
}

int add(void *engine_context, zRPC_timer *timer) {
  zRPC_minheap_timer_context *context = engine_context;
  zRPC_mutex_lock(&context->mutex);
  int is_first_timer = zRPC_timer_heap_add(&context->heap, timer);
  if (is_first_timer) {
    if (zRPC_time_cmp(timer->deadline, context->min_deadline) < 0) {
      context->min_deadline = timer->deadline;
    }
  }
  zRPC_mutex_unlock(&context->mutex);
  return 0;
}

int del(void *engine_context, zRPC_timer *timer) {
  zRPC_minheap_timer_context *context = engine_context;
  zRPC_mutex_lock(&context->mutex);
  zRPC_timer_heap_remove(&context->heap, timer);
  zRPC_mutex_unlock(&context->mutex);
  return 0;
}

#define TIMER_BASE_TMP_NUM 512

int32_t dispatch(void *engine_context, zRPC_timer **results[], size_t *nresults) {
  zRPC_minheap_timer_context *context = engine_context;
  zRPC_timespec now = zRPC_now(zRPC_CLOCK_MONOTONIC);
  zRPC_timespec ts;
  zRPC_timer *timer = NULL;
  size_t tmp_num = TIMER_BASE_TMP_NUM;
  (*results) = calloc(tmp_num, sizeof(**results));
  size_t n = 0;
  if (zRPC_mutex_trylock(&context->run_mutex)) {
    do {
      zRPC_mutex_lock(&context->mutex);
      if (zRPC_timer_heap_is_empty(&context->heap))
        break;
      timer = zRPC_timer_heap_top(&context->heap);
      if (zRPC_time_cmp(timer->deadline, now) > 0)
        break;
      timer->triggered = 1;
      zRPC_timer_heap_pop(&context->heap);
      zRPC_mutex_unlock(&context->mutex);
      context->min_deadline = timer->deadline;
      if(n >= tmp_num) {
        *results = realloc(*results, tmp_num *= 2);
      }
      (*results)[n++] = timer;
    } while (1);
    *nresults = n;
    zRPC_mutex_unlock(&context->mutex);
    zRPC_mutex_unlock(&context->run_mutex);
  }

  if (timer == NULL) {
    ts = zRPC_time_inf_future(zRPC_TIMESPAN);
    return zRPC_time_to_millis(ts);
  }

  now = zRPC_now(zRPC_CLOCK_MONOTONIC);

  if (zRPC_time_cmp(timer->deadline, now) < 0) {
    ts = zRPC_time_0(zRPC_TIMESPAN);
    return zRPC_time_to_millis(ts);
  }

  return zRPC_time_to_millis(zRPC_time_sub(timer->deadline, now));
}


void release(void *engine_context) {
  zRPC_minheap_timer_context *context = engine_context;
  zRPC_timer_heap_destroy(&context->heap);
  zRPC_mutex_destroy(&context->mutex);
  zRPC_mutex_destroy(&context->run_mutex);
}