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
#include "draw.h"
#include "font.h"
#include "image.h"
#include "glyph.h"
#include "multi.h"

/*
 * glyph_renderer_worker
 */

static const char log_name[] = "glyph_renderer_worker";

void glyph_renderer_worker::mainloop()
{
    std::unique_ptr<glyph_renderer> renderer = renderer_factory.create();
    std::unique_lock<std::mutex> lock(master->mutex);

    while (master->running) {
        size_t workitem, completed;

        if (master->processing.load() != master->total) {

            workitem = master->processing.fetch_add(1);
            glyph_render_request r = master->queue[workitem];

            lock.unlock();
            r.atlas->multithreading.store(true, std::memory_order_release);
            renderer->render(r.atlas, get_face(r.face), r.font_size, r.glyph);
            lock.lock();

            if (debug) {
                printf("%s-%02zu workitem=%zu\t[font=%s, size=%d, glyph=%d]\n",
                    log_name, worker_num, workitem, r.face->name.c_str(),
                    r.font_size, r.glyph);
            }

            /* notify master if complete */
            completed = master->processed.fetch_add(1);
            if (completed == master->total - 1) {
                if (debug) {
                    printf("%s-%02zu notify-master\n", log_name, worker_num);
                }
                master->response.notify_one();
            }
        } else {
            master->request.wait(lock);
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
    workers(), running(true), queue(),
    total(0), processing(0), processed(0),
    mutex(), request(), response()
{
    for (size_t i = 0; i < num_threads; i++) {
        workers.push_back(std::make_unique<glyph_renderer_worker>
            (this, i, renderer_factory));
    }
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
    font_atlas *atlas = manager->getFontAtlas(face);

    for (auto shape : shapes) {
        add(atlas, face, font_size, shape.glyph);
    }
}

void glyph_renderer_multi::add(font_atlas *atlas, font_face_ft *face,
    int font_size, int glyph)
{
    int requested_size = variable_size ? 0 : font_size;

    auto gi = atlas->glyph_map.find({face->font_id, requested_size, glyph});
    if (gi != atlas->glyph_map.end()) return;

    glyph_render_request r{atlas, face, requested_size, glyph};
    auto i = std::lower_bound(queue.begin(), queue.end(), r,
        [](const glyph_render_request &l, const glyph_render_request &r) {
            return l < r; });

    if (i == queue.end() || *i != r) {
        i = queue.insert(i, r);
        total.store(queue.size(), std::memory_order_release);
    }
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