// See LICENSE for license details.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cassert>
#include <cctype>
#include <cmath>

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include "binpack.h"
#include "utf8.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "multi.h"
#include "logger.h"

/*
 * glyph_renderer_worker
 */

static const char log_name[] = "glyph_renderer_worker";

void glyph_renderer_worker::mainloop()
{
    std::unique_ptr<glyph_renderer> renderer = renderer_factory.create();

    while (master->running) {
        size_t total, processing, workitem, processed;

        total = master->total.load(std::memory_order_acquire);
        processing = master->processing.load(std::memory_order_acquire);

        if (processing >= total) {
            std::unique_lock<std::mutex> lock(master->mutex);
            master->request.wait(lock);
            continue;
        }

        workitem = master->processing.fetch_add(1, std::memory_order_seq_cst);
        glyph_render_request r = master->queue[workitem];
        r.atlas->multithreading.store(true, std::memory_order_release);
        renderer->render(r.atlas, get_face(r.face), 0, r.glyph);
        processed = master->processed.fetch_add(1, std::memory_order_seq_cst);

        if (debug) {
            Debug("%s-%02zu workitem=%zu\t[font=%s, glyph=%d]\n",
                log_name, worker_num, workitem, r.face->name.c_str(),
                r.glyph);
        }

        total = master->total.load(std::memory_order_acquire);
        if (processed == total - 1) {
            if (debug) {
                Debug("%s-%02zu notify-master\n", log_name, worker_num);
            }
            master->response.notify_one();
        }
    }
}

font_face_ft* glyph_renderer_worker::get_face(font_face_ft *face)
{
    auto fi = face_map.find(face);
    if (fi != face_map.end()) {
        return fi->second.get();
    } else {
        fi = face_map.insert(face_map.end(), std::make_pair(face,
            std::unique_ptr<font_face_ft>(face->dup_thread())));
        return fi->second.get();
    }
}

/*
 * glyph_renderer_multi
 */

glyph_renderer_multi::glyph_renderer_multi(font_manager* manager,
    glyph_renderer_factory& renderer_factory, size_t num_threads) :
    variable_size(true), manager(manager),
    workers(), running(true), dedup(), queue(),
    capacity(1024), total(0), processing(0), processed(0),
    mutex(), request(), response()
{
    for (size_t i = 0; i < num_threads; i++) {
        workers.push_back(std::make_unique<glyph_renderer_worker>
            (this, i, renderer_factory));
    }
    queue.resize(1024);
}

glyph_renderer_multi::~glyph_renderer_multi()
{
    shutdown();
}

void glyph_renderer_multi::add(std::vector<glyph_shape> &shapes,
    text_segment *segment)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment->face);
    int font_size = segment->font_size;
    font_atlas *atlas = manager->getCurrentAtlas(face);

    for (auto shape : shapes) {
        auto gi = atlas->glyph_map.find({face->font_id, 0, shape.glyph});
        if (gi != atlas->glyph_map.end()) continue;

        glyph_render_request r{atlas, face, shape.glyph};
        auto i = std::lower_bound(dedup.begin(), dedup.end(), r,
            [](const glyph_render_request &l,
               const glyph_render_request &r) { return l < r; });

        if (i == dedup.end() || *i != r) {
            dedup.insert(i, r);
            enqueue(r);
        }
    }
}

void glyph_renderer_multi::enqueue(glyph_render_request &r)
{
    size_t workitem;
    do {
        workitem = total.load(std::memory_order_relaxed);
        if (workitem == capacity) return;
        queue[workitem] = r;
    } while (!total.compare_exchange_strong(workitem, workitem + 1,
        std::memory_order_seq_cst));

    request.notify_one();
}

void glyph_renderer_multi::run()
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
    dedup.clear();
    queue.clear();
    mutex.unlock();
}

void glyph_renderer_multi::shutdown()
{
    running = false;
    request.notify_all();
    for (size_t i = 0; i < workers.size(); i++) {
        workers[i]->thread.join();
    }
    workers.clear();
}