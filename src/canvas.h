// See LICENSE for license details.

#pragma once

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

/*
 * Edge Type
 */

enum EdgeType {
    EdgeNone = 0,

    EdgeLinear = 2,
    EdgeQuadratic = 3,
    EdgeCubic = 4,

    PrimitiveRectangle = 5,
    PrimitiveCircle = 6,
    PrimitiveEllipse = 7,
    PrimitiveRoundedRectangle = 8,
};

/*
 * Brush type
 */

enum BrushType {
    BrushNone = 0,

    BrushSolid = 1,
    BrushAxial = 2,
    BrushRadial = 3,
};


/*
 * Low-level Canvas Accelerator API for creating GPU resident shapes
 *
 * Some of this shape code is derived from msdfgen/ext/import-font.cpp
 * It has been simplified and adopts a data-oriented programming approach.
 * The context output is arrays formatted suitably to download to a GPU.
 *
 * The following 3 arrays are downloaded to the GPU:
 *
 * - shapes
 * - edges
 * - brushes
 *
 */

struct AEdge {
    float type;
    vec2 p[4];

    static int numPoints(int edge_type);
};

struct AContour {
    float edge_offset;
    float edge_count;
};

struct AShape {
    float contour_offset;
    float contour_count;
    float edge_offset;
    float edge_count;
    vec2 offset;
    vec2 size;
    float fill_brush;
    float stroke_brush;
    float stroke_width;
};

struct ABrush {
    float type;
    vec2 p[4];
    vec4 c[4];

    static int numPoints(int brush_type);
};

struct AContext {
    std::vector<AShape> shapes;
    std::vector<AContour> contours;
    std::vector<AEdge> edges;
    std::vector<ABrush> brushes;
    vec2 pos;

    void clear();
    int new_brush(ABrush b);
    int new_shape(vec2 offset, vec2 size);
    int new_contour();
    int new_edge(AEdge e);

    static bool brush_equals(ABrush *b0, ABrush *b1);
    static bool shape_equals(AShape *s0, AEdge *e0, AShape *s1, AEdge *e1);

    int find_brush(ABrush *b);
    int add_brush(ABrush *b, bool dedup = true);
    bool update_brush(int brush_num, ABrush *b);

    int find_shape(AShape *s, AEdge *e);
    int copy_shape(AShape *shape, AEdge *edges);
    int add_shape(AShape *s, AEdge *e, bool dedup = true);
    bool update_shape(int shape_num, AShape *s, AEdge *e);

    int add_glyph(FT_Face ftface, int sz, int dpi, int glyph);

    void print_shape(int shape_num);
    void print_edge(int edge_num);
    void print_brush(int brush_num);
};


/*
 * text renderer
 */

struct text_renderer_canvas : text_renderer
{
    AContext &ctx;
    std::map<int,int> &glyph_map;

    static const int glyph_size = 4096;

    text_renderer_canvas(AContext &ctx, std::map<int,int> &glyph_map);
    virtual ~text_renderer_canvas() = default;

    virtual void render(draw_list &batch,
        std::vector<glyph_shape> &shapes,
        text_segment *segment);
};

inline text_renderer_canvas::text_renderer_canvas(AContext &ctx,
    std::map<int,int> &glyph_map) : ctx(ctx), glyph_map(glyph_map) {}


/*
 * Canvas API
 *
 * This is the high-level canvas API which abstracts the low-level
 * accelerator API. It manages incremental changes to drawables.
 */

/* Low-level wrappers used to access the canvas accelerator arrays */

struct Canvas;

struct Brush
{
    BrushType brush_type;
    vec2 points[4];
    color colors[4];
};

struct Edge
{
    Canvas *canvas;
    int shape_num;
    int contour_num;
    int edge_num;

    EdgeType type();
    size_t num_points();
    vec2 get_point(size_t offset);
    void set_point(size_t offset, vec2 val);
};

struct Contour
{
    Canvas *canvas;
    int shape_num;
    int contour_num;

    size_t num_edges();
    Edge get_edge(size_t offset);
};

struct Shape
{
    Canvas *canvas;
    int shape_num;

    size_t num_contours();
    Contour get_contour(size_t offset);

    vec2 get_offset();
    vec2 get_size();
    Brush get_fill_brush();
    Brush get_stroke_brush();
    float get_stroke_width();

    void set_offset(vec2 offset);
    void set_size(vec2 size);
    void set_fill_brush(Brush fill_brush);
    void set_stroke_brush(Brush stroke_brush);
    void set_stroke_width(float stroke_width);
};

/* forward decls for high-level canvas objects */

struct Drawable;         /* Base class for high-level obejcts */
struct Patch;            /* Set of contours composed of Bézier curves */
struct Text;             /* Position, size, face and text string */
struct Primitve;         /* Base class for shapes composed of one edge */
struct Circle;           /* Circle: position, radius */
struct Ellipse;          /* Ellipse: position, halfsize */
struct Rectangle;        /* Rectangle: position, halfsize */
struct RoundedRectangle; /* Rounded Rectangle: position, halfsize, radius */

/*
 * Drawable is the base class for all canvas objects
 */

struct Drawable
{
    typedef std::unique_ptr<Drawable> Ptr;

    Canvas *canvas;
    int drawable_type;
    int drawable_num;
    int ll_shape_num;
    vec2 pos;
    float z;
    float scale;

    size_t num_edges();
    Edge get_edge(size_t edge_num);
    Shape get_shape();

    vec2 get_position();
    float get_z();
    float get_scale();

    void set_position(vec2 pos);
    void set_z(float z);
    void set_scale(float scale);
};

/*
 * Patch encompasses a set of contours containing Bézier curves
 */

struct Patch : Drawable
{
    size_t num_contours();
    Contour get_contour(size_t contour_num);

    void new_contour();
    void new_line(vec2 p1, vec2 p2);
    void new_quadratic_curve(vec2 p1, vec2 c1, vec2 p2);
    void new_cubic_curve(vec2 p1, vec2 c1, vec2 c2, vec2 p2);
};

/*
 * Text ecompasses a text string with position, size, and face
 */

enum text_halign {
    text_halign_left,
    text_halign_center,
    text_halign_right,
};

enum text_valign {
    text_valign_top,
    text_valign_center,
    text_valign_bottom,
};

struct Text : Drawable
{
    float size;
    font_face* face;
    text_halign halign;
    text_valign valign;
    std::string text;
    std::string lang;
    color col;

    text_segment segment;
    std::vector<glyph_shape> shapes;

    float get_size();
    font_face* get_face();
    text_halign get_halign();
    text_valign get_valign();
    std::string get_text();
    std::string get_lang();
    color get_color();

    void set_size(float size);
    void set_face(font_face* face);
    void set_halign(text_halign halign);
    void set_valign(text_valign valign);
    void set_text(std::string text);
    void set_lang(std::string lang);
    void set_color(color col);

    text_segment& get_text_segment();
    std::vector<glyph_shape>& get_glyph_shapes();
    vec2 get_text_size();
};

/*
 * Primitive Shape subclasses are single edge shapes with no contours
 */

struct Primitive : Drawable
{
    vec2 get_origin();
    void set_origin(vec2 origin);
    vec2 get_vec(size_t offset);
    void set_vec(size_t offset, vec2 val);
};

struct Circle : Primitive
{
    vec2 get_origin();
    void set_origin(vec2 origin);
    float get_radius();
    void set_radius(float radius);

    void update_circle(vec2 pos, float radius);
};

struct Ellipse : Primitive
{
    vec2 get_origin();
    void set_origin(vec2 origin);
    vec2 get_halfsize();
    void set_halfsize(vec2 radius);

    void update_ellipse(vec2 pos, vec2 half_size);
};

struct Rectangle : Primitive
{
    vec2 get_origin();
    void set_origin(vec2 origin);
    vec2 get_halfsize();
    void set_halfsize(vec2 radius);

    void update_rectangle(vec2 pos, vec2 half_size);
};

struct RoundedRectangle : Primitive
{
    vec2 get_origin();
    void set_origin(vec2 origin);
    vec2 get_halfsize();
    float get_radius();
    void set_halfsize(vec2 radius);
    void set_radius(float radius);

    void update_rounded_rectangle(vec2 pos, vec2 half_size, float radius);
};

/*
 * Canvas API proper
 */

struct Canvas
{
    std::vector<Drawable::Ptr> objects;
    std::map<int,int> glyph_map;
    std::unique_ptr<AContext> ctx;
    text_shaper_hb text_shaper;
    text_renderer_canvas text_renderer;
    font_manager *manager;
    bool dirty;
    Brush fill_brush;
    Brush stroke_brush;
    float stroke_width;

    Canvas(font_manager* manager);

    /* interface to low-level objects shared with the gpu */
    Brush get_brush(int brush_num);
    int get_brush_num(Brush p);
    size_t num_shapes();
    Shape get_shape(int shape_num);
    size_t num_contours();
    Contour get_contour(int shape_num, int contour_num);
    size_t num_edges();
    Edge get_edge(int shape_num, int contour_num, int edge_num);

    /* types used by canvas drawables */
    enum drawable_type {
        drawable_patch,
        drawable_text,
        drawable_circle,
        drawable_ellipse,
        drawable_rectangle,
        drawable_rounded_rectangle
    };

    /* drawables are the high-evel canvas objects */
    size_t num_drawables();
    Drawable* get_drawable(size_t offset);

    /* brushes are associated with new canvas objects on creation */
    Brush get_fill_brush();
    void set_fill_brush(Brush brush);
    Brush get_stroke_brush();
    void set_stroke_brush(Brush brush);
    float get_stroke_width();
    void set_stroke_width(float width);

    /* interface to create new canvas objects */
    Patch* new_patch(vec2 offset, vec2 size);
    Text* new_text();
    Circle* new_circle(vec2 pos, float radius);
    Ellipse* new_ellipse(vec2 pos, vec2 half_size);
    Rectangle* new_rectangle(vec2 pos, vec2 half_size);
    RoundedRectangle* new_rounded_rectangle(vec2 pos, vec2 half_size, float radius);

    /* emit canvas to draw list */
    void emit(draw_list &batch);
};