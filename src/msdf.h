#pragma once

struct glyph_renderer_msdf : glyph_renderer
{
    font_manager_ft* manager;
    font_atlas* atlas;
    span_vector span;

    glyph_renderer_msdf(font_manager_ft* manager, font_atlas* atlas);
    virtual ~glyph_renderer_msdf() = default;

    atlas_entry* render(font_face_ft *face, int font_size, int glyph);
};

inline glyph_renderer_msdf::glyph_renderer_msdf(font_manager_ft* manager,
    font_atlas* atlas) : manager(manager), atlas(atlas) {}