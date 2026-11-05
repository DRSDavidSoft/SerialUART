#ifndef PORTABLE_ASYNC_H
#define PORTABLE_ASYNC_H
/* Optional cooperative jobs and transport buffering. No allocator, threads or OS.
 * All calls belong to one owning loop. Callbacks must return promptly and must
 * not mutate/reenter the pool. Contexts remain borrowed through completion. */
#include <stddef.h>
#include <stdint.h>
#ifndef PS_ASYNC
#define PS_ASYNC 1
#endif
#ifndef PS_ASYNC_QUEUE
#define PS_ASYNC_QUEUE 1
#endif
#if PS_ASYNC
enum { PS_ASYNC_PENDING, PS_ASYNC_DONE, PS_ASYNC_FAILED,
       PS_ASYNC_CANCELLED, PS_ASYNC_TIMEOUT };
typedef unsigned (*ps_async_step_fn)(void *, uint32_t now, int *result);
typedef void (*ps_async_stop_fn)(void *, unsigned reason);
typedef void (*ps_async_done_fn)(void *, uint32_t id, unsigned state, int result);
typedef struct {
    uint32_t id, started, timeout;
    unsigned state;
    int result;
    ps_async_step_fn step;
    ps_async_stop_fn stop;
    ps_async_done_fn done;
    void *context;
} ps_async_job;
typedef struct {
    ps_async_job *jobs;
    unsigned capacity, cursor, running;
    uint32_t next_id;
} ps_async;
static inline int ps_async_init(ps_async *p, ps_async_job *jobs, unsigned capacity) {
    unsigned i;
    if(!p || !jobs || !capacity)return 0;
    p->jobs=jobs;p->capacity=capacity;p->cursor=p->running=0;p->next_id=1;
    for(i=0;i<capacity;++i)jobs[i].id=0;
    return 1;
}
/* NULL step means event-driven: owner calls complete when a host event arrives.
 * timeout=0 disables deadline. Other durations must be <= UINT32_MAX/2.
 * ID zero means rejected/full/exhausted. IDs never repeat within a pool lifetime. */
static inline uint32_t ps_async_submit(ps_async *p, uint32_t now, uint32_t timeout,
        ps_async_step_fn step, ps_async_stop_fn stop, ps_async_done_fn done, void *context) {
    unsigned i;
    if(!p || p->running || !p->next_id || timeout>UINT32_MAX/2)return 0;
    for(i=0;i<p->capacity;++i)if(!p->jobs[i].id){
        ps_async_job *j=&p->jobs[i];
        j->id=p->next_id++;j->started=now;j->timeout=timeout;
        j->state=PS_ASYNC_PENDING;j->result=0;j->step=step;
        j->stop=stop;j->done=done;j->context=context;
        return j->id;
    }
    return 0;
}
static inline unsigned ps_async_pending(const ps_async *p) {
    unsigned i,n=0;
    for(i=0;i<p->capacity;++i)if(p->jobs[i].id)++n;
    return n;
}
/* Completion is delivered on poll, never inline. First terminal event wins.
 * Host must marshal worker/ISR completions onto the owner loop. */
static inline int ps_async_complete(ps_async *p,uint32_t id,unsigned state,int result) {
    unsigned i;
    if(!p || p->running || !id ||
       (state!=PS_ASYNC_DONE && state!=PS_ASYNC_FAILED && state!=PS_ASYNC_CANCELLED))return 0;
    for(i=0;i<p->capacity;++i)if(p->jobs[i].id==id && p->jobs[i].state==PS_ASYNC_PENDING){
        p->jobs[i].state=state;p->jobs[i].result=result;return 1;
    }
    return 0;
}
static inline int ps_async_cancel(ps_async *p,uint32_t id) {
    return ps_async_complete(p,id,PS_ASYNC_CANCELLED,0);
}
static inline unsigned ps_async_cancel_all(ps_async *p) {
    unsigned i,n=0;
    if(!p || p->running)return 0;
    for(i=0;i<p->capacity;++i)if(p->jobs[i].id)
        n+=(unsigned)ps_async_cancel(p,p->jobs[i].id);
    return n;
}
/* At most budget active jobs visited, each at most once per poll. Round robin.
 * Poll at least once per 2^31 milliseconds for reliable wrapping-clock deadlines.
 * stop releases/cancels external work on cancellation/timeout before done.
 * It must not block; stopping hardware/threads is still the host's responsibility. */
static inline unsigned ps_async_poll(ps_async *p,uint32_t now,unsigned budget) {
    unsigned scanned=0,visited=0;
    if(!p || p->running)return 0;
    p->running=1;
    while(scanned<p->capacity && visited<budget){
        ps_async_job *j=&p->jobs[p->cursor];
        p->cursor=(p->cursor+1)%p->capacity;++scanned;
        if(!j->id)continue;
        ++visited;
        if(j->state==PS_ASYNC_PENDING){
            if(j->timeout && (uint32_t)(now-j->started)>=j->timeout)j->state=PS_ASYNC_TIMEOUT;
            else if(j->step){
                j->state=j->step(j->context,now,&j->result);
                if(j->state>PS_ASYNC_FAILED)j->state=PS_ASYNC_FAILED;
            }
        }
        if(j->state!=PS_ASYNC_PENDING){
            if((j->state==PS_ASYNC_CANCELLED || j->state==PS_ASYNC_TIMEOUT) && j->stop)
                j->stop(j->context,j->state);
            if(j->done)j->done(j->context,j->id,j->state,j->result);
            j->id=0;j->context=0;j->step=0;j->stop=0;j->done=0;
        }
    }
    p->running=0;
    return visited;
}
#endif
#if PS_ASYNC_QUEUE
/* All-or-nothing producer writes. Consumer peeks a contiguous span and consumes
 * only bytes actually transmitted. No atomics: use the same owning loop. */
typedef struct {
    unsigned char *bytes;
    size_t capacity, head, count;
} ps_byte_queue;
static inline int ps_byte_queue_init(ps_byte_queue *q,void *bytes,size_t capacity) {
    if(!q || !bytes || !capacity)return 0;
    q->bytes=(unsigned char *)bytes;q->capacity=capacity;q->head=q->count=0;return 1;
}
static inline size_t ps_byte_queue_space(const ps_byte_queue *q) {
    return q->capacity-q->count;
}
static inline int ps_byte_queue_write(ps_byte_queue *q,const void *bytes,size_t n) {
    size_t i,at;
    const unsigned char *src=(const unsigned char *)bytes;
    if(n>ps_byte_queue_space(q) || (!src && n))return 0;
    at=q->head;
    /* Avoid size_t overflow for very large caller buffers. */
    if(q->count>=q->capacity-at)at=q->count-(q->capacity-at);else at+=q->count;
    for(i=0;i<n;++i){q->bytes[at]=src[i];if(++at==q->capacity)at=0;}
    q->count+=n;return 1;
}
static inline size_t ps_byte_queue_peek(const ps_byte_queue *q,const unsigned char **bytes) {
    size_t n=q->capacity-q->head;
    *bytes=q->bytes+q->head;return q->count<n?q->count:n;
}
static inline int ps_byte_queue_consume(ps_byte_queue *q,size_t n) {
    if(n>q->count)return 0;
    if(n>=q->capacity-q->head)q->head=n-(q->capacity-q->head);else q->head+=n;
    q->count-=n;return 1;
}
#endif
#endif
