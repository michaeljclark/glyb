// See LICENSE for license details.

#pragma once

typedef unsigned uint;

typedef struct {
    float pos[3];
    float uv[2];
    uint color;
    float material;
} draw_vertex;

enum { tbo_iid = -1 };

enum {
    st_clamp       = (1 << 1),
    st_wrap        = (1 << 2),
    filter_nearest = (1 << 3),
    filter_linear  = (1 << 4),
};

typedef struct {
    int iid;
    int size[3];
    int modrect[4];
    int flags;
    uint8_t *pixels;
} draw_image;

enum {
    image_none = 0
};

enum {
    mode_triangles = 1,
    mode_lines     = 2,
};

enum {
    shader_simple   = 1,
    shader_msdf     = 2,
    shader_canvas   = 3,
};

typedef struct {
    uint viewport[4];
    uint iid;
    uint mode;
    uint shader;
    uint offset;
    uint count;
} draw_cmd;

typedef struct {
    std::vector<draw_image> images;
    std::vector<draw_cmd> cmds;
    std::vector<draw_vertex> vertices;
    std::vector<uint> indices;
} draw_list;

inline void draw_list_clear(draw_list &batch)
{
    batch.cmds.clear();
    batch.vertices.clear();
    batch.indices.clear();
}

inline void draw_list_viewport(draw_list &batch, uint x, uint y, uint w, uint h)
{
    bool empty = batch.cmds.size() == 0;
    draw_cmd &last = batch.cmds.back();

    if (empty ||
        (last.viewport[0] != x &&
         last.viewport[1] != y &&
         last.viewport[2] != w &&
         last.viewport[3] != h))
    {
        batch.cmds.push_back({{ x, y, w, h },
            !empty ? last.iid : 0,
            !empty ? last.mode : 0,
            !empty ? last.shader : 0,
            !empty ? last.offset + last.count : 0,
            0
        });
    }
}

inline uint draw_list_vertex(draw_list &batch, draw_vertex v)
{
    auto i = batch.vertices.insert(batch.vertices.end(), v);
    return (uint)(i - batch.vertices.begin());
}

inline void draw_list_indices(draw_list &batch, uint iid, uint mode, uint shader,
    std::initializer_list<uint> l)
{
    bool empty = batch.cmds.size() == 0;
    draw_cmd &last = batch.cmds.back();

    uint start, end;

    start = (uint)batch.indices.size();
    batch.indices.insert(batch.indices.end(), l.begin(), l.end());
    end = (uint)batch.indices.size();

    if (empty ||
        last.iid != iid ||
        last.mode != mode ||
        last.shader != shader)
    {
        batch.cmds.push_back({{
            !empty ? last.viewport[0] : 0,
            !empty ? last.viewport[1] : 0,
            !empty ? last.viewport[2] : 0,
            !empty ? last.viewport[3] : 0
        }, iid, mode, shader, start, end - start });
    } else {
        last.count += (end - start);
    }
}

inline void draw_list_image_delta(draw_list &batch, image *img, bin_rect delta, int flags)
{
    int w = img->getWidth(), h = img->getHeight(), d = img->getBytesPerPixel();

    draw_image drim{img->iid, {w,h,d}, { delta.a.x, delta.a.y,
        (delta.b.x - delta.a.x), (delta.b.y - delta.a.y)
    }, flags, img->getData()};

    auto i = std::lower_bound(batch.images.begin(), batch.images.end(), drim,
        [](const draw_image &l, const draw_image &r) { return l.iid < r.iid; });

    if (i == batch.images.end() || i->iid != drim.iid) {
        i = batch.images.insert(i, drim);
    } else {
        if (delta.a.x == w && delta.a.y == h) {
            // do nothing
        }
        else if (i->modrect[0] == 0 && i->modrect[1] == 0 &&
            i->modrect[2] == w && i->modrect[3] == h) {
            // set delta
            i->modrect[0] = delta.a.x;
            i->modrect[1] = delta.a.y;
            i->modrect[2] = (delta.b.x - delta.a.x);
            i->modrect[3] = (delta.b.y - delta.a.y);
        }
        else {
            // widen delta
            int x1 = std::min(i->modrect[0],delta.a.x);
            int y1 = std::min(i->modrect[1],delta.a.y);
            int x2 = std::max(i->modrect[0]+i->modrect[2],delta.b.x);
            int y2 = std::max(i->modrect[1]+i->modrect[3],delta.b.y);
            i->modrect[0] = x1;
            i->modrect[1] = y1;
            i->modrect[2] = x2-x1;
            i->modrect[3] = y2-y1;
        }
    }
}

inline void draw_list_image(draw_list &batch, image *img, int flags)
{
    bin_rect delta(bin_point((int)img->getWidth(),(int)img->getHeight()),bin_point(0,0));

    return draw_list_image_delta(batch, img, delta, flags);
}