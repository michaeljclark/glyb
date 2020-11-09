// See LICENSE for license details.

#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>

/*
 * == Overview ==
 *
 * C++ threaded worker pool that executes work-items from a queue, featuring:
 *
 * - customizable work item
 * - customizable worker
 * - controllable concurrency
 *
 * designed to dispatch 'm' irregular sized items to a pool of 'n' threads.
 * dispatcher asynchronously passes work-items to worker threads who execute
 * work-items with persistent worker instances, then signal completion back
 * to the dispatcher which then processes any remaining work-items.
 *
 * application state for workers is created by a pool worker factory lambda.
 * worker threads have one worker object implementing the pool_worker protocol.
 *
 * pool dispatcher contains the work queue and creates workers and threads
 * with mainloops that process work-items one-by-one calling exec on a worker.
 *
 * == Usage ==
 *
 * user defines an ITEM type and a WORKER type that implements pool_worker:
 *
 *   template <typename ITEM> struct pool_worker
 *   {
 *       virtual ~pool_worker() = default;
 *       virtual void operator()(ITEM &wi) = 0;
 *   };
 *
 *   template <typename ITEM, typename WORKER> struct pool_executor;
 *
 * == Definitions ==
 *
 * pool_worker         - protocol implementing exec(const pool_workitem&).
 * pool_executor       - dispatches work items to worker thread's workers.
 *                       manages worker threads and a queue of work items.
 * pool_worker_thread  - composes std:thread and contains worker mainloop.
 */

template <typename ITEM> struct pool_worker;
template <typename ITEM, typename WORKER> struct pool_executor;
template <typename ITEM, typename WORKER> struct pool_worker_thread;

/*
 * pool_worker protocol
 */

template <typename ITEM> struct pool_worker
{
    virtual ~pool_worker() = default;
    virtual void operator()(ITEM &wi) = 0;
};

/*
 * pool_executor
 */

template <typename ITEM, typename WORKER>
struct pool_executor
{
    typedef std::unique_ptr<pool_worker<ITEM>> worker_ptr;
    typedef std::function<worker_ptr()> worker_factory_fn;

    /*
     * multithreaded work queue specific structure members
     *
     * workers    - worker threads to process work queue
     * running    - boolean variable that is cleared to shutdown workers
     * queue      - storage for work items
     * total      - upper bound of items to process, write to start work
     * processing - upper bound of items processing, written to dequeue work
     * processed  - lower bound of items processing, written to finish work
     * mutex      - lock for condition variable
     * request    - condition variable waited on by workers
     * response   - condition variable waited on by executor
     */
    std::vector<std::unique_ptr<pool_worker_thread<ITEM,WORKER>>> workers;
    std::atomic<bool>                 running;
    std::vector<ITEM>                 queue;
    std::atomic<size_t>               total;
    std::atomic<size_t>               processing;
    std::atomic<size_t>               processed;
    std::mutex                        mutex;
    std::condition_variable           request;
    std::condition_variable           response;

    pool_executor(size_t num_threads, size_t queue_size,
        const worker_factory_fn &worker_factory = [](){
        return std::unique_ptr<pool_worker<ITEM>>(new WORKER());
    });
    virtual ~pool_executor();

    bool enqueue(ITEM &&item);
    bool enqueue(const ITEM &item);

    void run();
    void shutdown();
};

/*
 * pool_worker_thread
 */

template <typename ITEM, typename WORKER>
struct pool_worker_thread
{
    typedef std::unique_ptr<pool_worker<ITEM>> worker_ptr;
    typedef std::function<worker_ptr()> worker_factory_fn;

    pool_executor<ITEM,WORKER>& dispatcher;
    const worker_factory_fn worker_factory;
    const size_t worker_num;
    std::thread thread;

    pool_worker_thread(pool_executor<ITEM,WORKER>& dispatcher,
        const worker_factory_fn &worker_factory, size_t worker_num);

    void mainloop();
};

template <typename ITEM, typename WORKER>
pool_worker_thread<ITEM,WORKER>::pool_worker_thread(
    pool_executor<ITEM,WORKER>& dispatcher,
    const std::function<std::unique_ptr<pool_worker<ITEM>>()> &worker_factory,
    size_t worker_num
) :
    dispatcher(dispatcher),
    worker_factory(worker_factory),
    worker_num(worker_num),
    thread(&pool_worker_thread::mainloop, this)
{}

template <typename ITEM, typename WORKER>
void pool_worker_thread<ITEM,WORKER>::mainloop()
{
    std::unique_ptr<pool_worker<ITEM>> worker = worker_factory();

    while (dispatcher.running) {
        size_t total, processing, workitem, processed;

        /* find out how many items still need processing */
        total = dispatcher.total.load(std::memory_order_acquire);
        processing = dispatcher.processing.load(std::memory_order_acquire);

        /* sleep on dispatcher condition if there is no work */
        if (processing >= total) {
            std::unique_lock<std::mutex> lock(dispatcher.mutex);
            dispatcher.request.wait(lock);
            continue;
        }

        /* dequeue work-item and process it */
        workitem = dispatcher.processing.fetch_add(1, std::memory_order_seq_cst);
        (*worker)(dispatcher.queue[workitem]);
        processed = dispatcher.processed.fetch_add(1, std::memory_order_seq_cst);

        /* notify dispatcher when last item has been processed */
        total = dispatcher.total.load(std::memory_order_acquire);
        if (processed == total - 1) {
            dispatcher.response.notify_one();
        }
    }
}

/*
 * pool_executor implementation
 */

template <typename ITEM, typename WORKER>
pool_executor<ITEM,WORKER>::pool_executor(size_t num_threads, size_t queue_size,
    const worker_factory_fn &worker_factory
) :
    workers(), running(true), queue(), total(0), processing(0), processed(0),
    mutex(), request(), response()
{
    for (size_t i = 0; i < num_threads; i++) {
        workers.push_back(std::make_unique<pool_worker_thread<ITEM,WORKER>>
            (*this, worker_factory, i));
    }
    queue.resize(queue_size);
}

template <typename ITEM, typename WORKER>
pool_executor<ITEM,WORKER>::~pool_executor()
{
    shutdown();
}

template <typename ITEM, typename WORKER>
bool pool_executor<ITEM,WORKER>::enqueue(ITEM &&item)
{
    return enqueue(item);
}

template <typename ITEM, typename WORKER>
bool pool_executor<ITEM,WORKER>::enqueue(const ITEM &item)
{
    size_t workitem;
    do {
        workitem = total.load(std::memory_order_relaxed);
        if (workitem == queue.size()) return false;
        queue[workitem] = item;
    } while (!total.compare_exchange_strong(workitem, workitem + 1,
        std::memory_order_seq_cst));

    request.notify_one();
    return true;
}

template <typename ITEM, typename WORKER>
void pool_executor<ITEM,WORKER>::run()
{
    /* if no workers or queue empty, do nothing */
    if (workers.size() == 0 || processed == total) {
        return;
    }

    /* wake all workers */
    request.notify_all();

    /* while processed less than queue size, wait for response */
    while (processed < total) {
        std::unique_lock<std::mutex> lock(mutex);
        response.wait(lock);
    }

    /* work queue is processed, take lock and clear queue */
    mutex.lock();
    total.store(0, std::memory_order_release);
    processing.store(0, std::memory_order_release);
    processed.store(0, std::memory_order_release);
    mutex.unlock();
}

template <typename ITEM, typename WORKER>
void pool_executor<ITEM,WORKER>::shutdown()
{
    running = false;
    request.notify_all();
    for (size_t i = 0; i < workers.size(); i++) {
        workers[i]->thread.join();
    }
    workers.clear();
}