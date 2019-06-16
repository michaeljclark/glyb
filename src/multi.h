#pragma once

/*
 * glyph_render_request
 */

struct glyph_render_request
{
    font_atlas* atlas;
    font_face_ft *face;
    int font_size;
    int glyph;

    const bool operator!=(const glyph_render_request &o) const {
        return std::tie(atlas, face, font_size, glyph) !=
            std::tie(o.atlas, o.face, o.font_size, o.glyph);
    }
    const bool operator<(const glyph_render_request &o) const {
        return std::tie(atlas, face, font_size, glyph) <
            std::tie(o.atlas, o.face, o.font_size, o.glyph);
    }
};

/*
 * glyph_renderer_worker
 */

struct glyph_renderer_factory
{
    virtual ~glyph_renderer_factory() = default;
    virtual std::unique_ptr<glyph_renderer> create() = 0;
};

template <typename T>
struct glyph_renderer_factory_impl : glyph_renderer_factory
{
    std::unique_ptr<glyph_renderer> create()
    {
        return std::unique_ptr<glyph_renderer>(new T());
    }
};

/*
 * glyph_renderer_worker
 */

struct glyph_renderer_multi;

struct glyph_renderer_worker
{
    static const bool debug = false;

    glyph_renderer_multi* master;
    size_t worker_num;
    glyph_renderer_factory& renderer_factory;
    std::thread thread;
    std::map<font_face_ft*,std::unique_ptr<font_face_ft>> face_map;

    glyph_renderer_worker(glyph_renderer_multi* master, size_t worker_num,
        glyph_renderer_factory& renderer_factory);

    font_face_ft* get_face(font_face_ft *face);

    void mainloop();
};

inline glyph_renderer_worker::glyph_renderer_worker(
    glyph_renderer_multi* master, size_t worker_num,
    glyph_renderer_factory& renderer_factory) :
    master(master), worker_num(worker_num),
    renderer_factory(renderer_factory),
    thread(&glyph_renderer_worker::mainloop, this) {}

/*
 * glyph_renderer_multi
 */

struct glyph_renderer_multi
{
    static const bool debug = false;

    /*
     * renderer specific structure members
     *
     * variable_size - indicates use of variable size glyph renderer
     * manager       - font manager used to acquire font atlases
     */
    bool                              variable_size;
    font_manager*                     manager;

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
     * request    - condition variable waited on by workers to start work
     * response   - condition variable waited on by master on completed work
     */
    std::vector<std::unique_ptr<glyph_renderer_worker>> workers;
    std::atomic<bool>                 running;
    std::vector<glyph_render_request> queue;
    std::atomic<size_t>               total;
    std::atomic<size_t>               processing;
    std::atomic<size_t>               processed;
    std::mutex                        mutex;
    std::condition_variable           request;
    std::condition_variable           response;

    glyph_renderer_multi(font_manager* manager,
        glyph_renderer_factory& renderer_factory, size_t num_threads);
    virtual ~glyph_renderer_multi();

    void add(std::vector<glyph_shape> &shapes, text_segment *segment);
    void add(font_atlas* atlas, font_face_ft *face, int font_size, int glyph);
    void run();
    void shutdown();
};
