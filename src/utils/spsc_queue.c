#include "spsc_queue.h"

#include <stdlib.h>
#include <errno.h>

int spsc_bq_init(spsc_bq_t *q, size_t capacity)
{
    if (!q || capacity == 0) {
        return EINVAL;
    }

    void **buf = (void **)malloc(sizeof(void *) * capacity);
    if (!buf) {
        return ENOMEM;
    }

    q->buf = buf;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->closed = 0;

    int ret;
    if ((ret = pthread_mutex_init(&q->mtx, NULL)) != 0) {
        free(buf);
        return ret;
    }
    if ((ret = pthread_cond_init(&q->not_empty, NULL)) != 0) {
        pthread_mutex_destroy(&q->mtx);
        free(buf);
        return ret;
    }
    if ((ret = pthread_cond_init(&q->not_full, NULL)) != 0) {
        pthread_cond_destroy(&q->not_empty);
        pthread_mutex_destroy(&q->mtx);
        free(buf);
        return ret;
    }

    return 0;
}

void spsc_bq_destroy(spsc_bq_t *q)
{
    if (!q) return;

    pthread_mutex_lock(&q->mtx);
    // 标记关闭，唤醒所有等待线程
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mtx);

    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->mtx);

    free(q->buf);
    q->buf = NULL;
    q->capacity = 0;
    q->head = q->tail = q->count = 0;
}

int spsc_bq_push(spsc_bq_t *q, void *item)
{
    if (!q) return EINVAL;

    int ret = 0;

    pthread_mutex_lock(&q->mtx);

    // 队列满则等待
    while (q->count == q->capacity && !q->closed) {
        pthread_cond_wait(&q->not_full, &q->mtx);
    }

    if (q->closed) {
        // 队列关闭后禁止再 push
        ret = EPIPE;
        goto out;
    }

    // 写入元素
    q->buf[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    // 唤醒可能阻塞在 pop 上的消费者
    pthread_cond_signal(&q->not_empty);

out:
    pthread_mutex_unlock(&q->mtx);
    return ret;
}

int spsc_bq_pop(spsc_bq_t *q, void **out)
{
    if (!q || !out) return EINVAL;

    int ret = 0;

    pthread_mutex_lock(&q->mtx);

    // 队列空则等待
    while (q->count == 0 && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->mtx);
    }

    if (q->count == 0 && q->closed) {
        // 队列关闭且已无元素
        ret = EPIPE;
        goto out;
    }

    // 取出元素
    *out = q->buf[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    // 唤醒可能阻塞在 push 上的生产者
    pthread_cond_signal(&q->not_full);

out:
    pthread_mutex_unlock(&q->mtx);
    return ret;
}

int spsc_bq_try_push(spsc_bq_t *q, void *item)
{
    if (!q) return EINVAL;

    int ret = 0;

    pthread_mutex_lock(&q->mtx);

    if (q->closed) {
        ret = EPIPE;                // 队列已关闭
    } else if (q->count == q->capacity) {
        ret = EAGAIN;               // 队列已满，非阻塞失败
    } else {
        // 有空间，可以直接入队
        q->buf[q->tail] = item;
        q->tail = (q->tail + 1) % q->capacity;
        q->count++;
        // 提醒可能阻塞在 pop 的消费者
        pthread_cond_signal(&q->not_empty);
    }

    pthread_mutex_unlock(&q->mtx);
    return ret;
}

int spsc_bq_try_pop(spsc_bq_t *q, void **out)
{
    if (!q || !out) return EINVAL;

    int ret = 0;

    pthread_mutex_lock(&q->mtx);

    if (q->count == 0) {
        if (q->closed) {
            ret = EPIPE;            // 关闭且空
        } else {
            ret = EAGAIN;           // 还没数据，非阻塞失败
        }
    } else {
        // 有数据，直接出队
        *out = q->buf[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;

        // 提醒可能阻塞在 push 的生产者
        pthread_cond_signal(&q->not_full);
    }

    pthread_mutex_unlock(&q->mtx);
    return ret;
}

void spsc_bq_close(spsc_bq_t *q)
{
    if (!q) return;

    pthread_mutex_lock(&q->mtx);
    q->closed = 1;
    // 唤醒所有等待线程，让它们看到 closed 状态
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mtx);
}