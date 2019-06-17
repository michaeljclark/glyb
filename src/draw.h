// See LICENSE for license details.

#pragma once

typedef unsigned uint;

typedef struct {
    float pos[3];
    float uv[2];
    uint color;
} draw_vertex;

typedef struct {
    uint iid;     /* image id */
    uint size[3];     /* image dimensions: width,height,depth */
    uint modrect[4];  /* modified rectangle */
    uint flags;
    enum {
        st_clamp       = (1 << 1),
        st_wrap        = (1 << 2),
        filter_nearest = (1 << 3),
        filter_linear  = (1 << 4),
    };
    uint8_t *pixels;
} draw_image;

typedef enum {
    image_none = 0
} _image;

typedef enum {
    mode_triangles = 1,
    mode_lines     = 2,
} _mode;

typedef enum {
    shader_simple   = 1,
    shader_msdf     = 2,
} _shader;

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

inline void draw_list_add(draw_list &batch, uint iid, uint mode, uint shader, std::initializer_list<uint> l)
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
