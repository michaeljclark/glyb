#pragma once

struct glyph_renderer_msdf : glyph_renderer
{
    span_vector span;

    glyph_renderer_msdf() = default;
    virtual ~glyph_renderer_msdf() = default;

    atlas_entry render(font_atlas* atlas, font_face_ft *face,
    	int font_size, int glyph);
};
