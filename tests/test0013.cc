#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "worker.h"

/*
 * C++ threaded worker pool that executes work-items from a queue, featuring:
 *
 * - customizable work item   (std::async has heavyweight items)
 * - customizable worker      (std::async has not control over lifecycle)
 * - controllable concurrency (std::async concurrency is not controllable)
 */

static size_t next_mule_id;

struct mule_item { size_t id; };

struct mule_worker : pool_worker<mule_item>
{
    size_t mule_id;

    mule_worker() : mule_id(next_mule_id++) {
        printf("mule-%zu: began\n", mule_id);
    }

    virtual ~mule_worker() {
        printf("mule-%zu: finished\n", mule_id);
    }

    virtual void operator()(mule_item &item) {
        sleep(1); printf("mule-%zu item %zu\n", mule_id, item.id);
    }
};

int main(int argc, const char **argv)
{
    const size_t num_threads = std::thread::hardware_concurrency();
    const size_t queue_size = num_threads * 2;

    pool_executor<mule_item,mule_worker> pool(num_threads, queue_size, [](){
        return std::unique_ptr<pool_worker<mule_item>>(new mule_worker());
    });

    /* enqueue work items to the pool.
     * executor creates 'mule_worker' instances to process work items. */
    for (size_t i = 0; i < num_threads * 2; i++ ) {
        pool.enqueue(mule_item{i});
    }

    /* work completed after run(), which is an implicit control flow join,
     * not a full thread join; just a lightweight condition variable wake. */
    pool.run();

    return 0;
}