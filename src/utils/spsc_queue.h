#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <pthread.h>
#include <stddef.h> // size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void **buf;              // 元素指针数组
    size_t capacity;         // 容量
    size_t head;             // 出队位置
    size_t tail;             // 入队位置
    size_t count;            // 当前元素个数

    pthread_mutex_t mtx;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;

    int closed;              // 标记队列已关闭（用于优雅退出）
} spsc_bq_t;

/**
 * @brief 初始化单生产者单消费者阻塞队列
 * 
 * @param q         队列对象指针
 * @param capacity  队列最大容量（至少为 1）
 * @return 0 表示成功，非 0 为错误号（errno 风格）
 */
int spsc_bq_init(spsc_bq_t *q, size_t capacity);

/**
 * @brief 销毁队列
 * 
 * 调用后不再允许使用该队列。
 * 注意：不会自动释放元素里存放的指针所指向的内存。
 */
void spsc_bq_destroy(spsc_bq_t *q);

/**
 * @brief 入队（阻塞），单生产者专用
 *
 * 如果队列已满，则阻塞等待直到有空间或队列被关闭。
 * 
 * @param q     队列指针
 * @param item  要入队的元素指针
 * @return 0 成功；
 *         非 0：如果队列已被关闭，返回 EPIPE。
 */
int spsc_bq_push(spsc_bq_t *q, void *item);

/**
 * @brief 出队（阻塞），单消费者专用
 * 
 * 如果队列为空，则阻塞等待直到有元素或队列被关闭。
 * 
 * @param q 队列指针
 * @param out 接收出队元素的指针地址
 * @return 0 成功；
 *         非 0：如果队列已被关闭且队列中无元素，返回 EPIPE。
 */
int spsc_bq_pop(spsc_bq_t *q, void **out);

/**
 * @brief 非阻塞入队，单生产者专用
 * 
 * - 如果队列已满，立即返回 EAGAIN，不阻塞；
 * - 如果队列已关闭，返回 EPIPE；
 * - 否则入队并返回 0。
 */
int spsc_bq_try_push(spsc_bq_t *q, void *item);

/**
 * @brief 非阻塞出队，单消费者专用
 * 
 * - 如果队列为空，立即返回 EAGAIN，不阻塞；
 * - 如果队列已关闭且空，返回 EPIPE；
 * - 否则出队并返回 0。
 */
int spsc_bq_try_pop(spsc_bq_t *q, void **out);

/**
 * @brief 关闭队列
 * 
 * - 之后的 push / try_push 将返回 EPIPE；
 * - 如果有线程阻塞在 push/pop 上，将被唤醒，返回 EPIPE。
 * 
 * 通常在退出时调用，配合 pop 的返回值退出消费者线程。
 */
void spsc_bq_close(spsc_bq_t *q);

#ifdef __cplusplus
}
#endif

#endif // SPSC_QUEUE_H