#pragma once

namespace ui9 {

using dvec3 = glm::dvec3;

bool operator<(const dvec3 &a, const dvec3 &b)
{
    return (a[0] < b[0]) ||
           (a[0] == b[0] && a[1] < b[1]) ||
           (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
}

bool operator>(const dvec3 &a, const dvec3 &b) { return b < a; }
bool operator<=(const dvec3 &a, const dvec3 &b) { return a < b || a == b; }
bool operator>=(const dvec3 &a, const dvec3 &b) { return b < a || a == b; }

typedef enum {
    trim_leading = 0x1,
    trim_trailing = 0x2,
    trim_both = 0x3
} trim_options;

static std::vector<std::string> split(std::string str, std::string separator,
        bool includeEmptyElements = true, bool includeSeparators = false)
{
    size_t last_index = 0, index;
    std::vector<std::string> components;
    while ((index = str.find_first_of(separator, last_index)) != std::string::npos) {
        if (includeEmptyElements || index - last_index > 0) {
            components.push_back(str.substr(last_index, index - last_index));
        }
        if (includeSeparators) {
            components.push_back(separator);
        }
        last_index = index + separator.length();
    }
    if (includeEmptyElements || str.size() - last_index > 0) {
        components.push_back(str.substr(last_index, str.size() - last_index));
    }
    return components;
}

static std::string trim(std::string str, trim_options opts = trim_both)
{
    int first_nonws = -1;
    int last_nonws = 0;
    for (int i = 0; i < (int)str.length(); i++) {
        char c = str[i];
        if (c != ' ' && c != '\t') {
            if (first_nonws == -1) {
                first_nonws = i;
            }
            last_nonws = i + 1;
        }
    }
    if (first_nonws == -1) {
        first_nonws = 0;
    }
    int start = (opts & trim_leading) ? first_nonws : 0;
    int len = (opts & trim_trailing) ? last_nonws - start : (int)str.length() - start;
    return str.substr(start, len);
}

enum {
    all = -1,
    none = 0,
};

enum event_type {
    keyboard = 1,
    mouse = 2,
};

enum event_qualifier {
    pressed = 1,
    released = 2,
    motion = 3,
};

enum button {
    left = 1,
    center = 2,
    right = 3,
    wheel = 4,
};

struct Event
{
    char type;
    char qualifier;
};

struct KeyEvent
{
    Event header;
    int keycode;
    int scancode;
};

struct MouseEvent
{
    Event header;
    int button;
    dvec3 pos;
};

enum class Orientation
{
    horizontal,
    vertical
};

struct Rect
{
    vec2 pos;
    vec2 size;
};

struct Properties
{
    static const bool debug = false;

    std::map<std::string,std::string> map;

    Properties() = default;

    Properties(file_ptr rsrc)
    {
        load_properties(rsrc);
    }

    virtual ~Properties() = default;

    void load_properties(file_ptr rsrc);

    size_t size()
    {
        return map.size();
    }

    bool property_exists(std::string key)
    {
        return map.find(key) != map.end();
    }

    std::string get_property(std::string key, std::string default_val = "")
    {
        auto mi = map.find(key);
        return (mi != map.end()) ? (*mi).second : default_val;
    }
};

inline void Properties::load_properties(file_ptr rsrc)
{
    char buf[1024];

    while (rsrc->readLine(buf, sizeof(buf))) {
        std::string line(buf);
        if (line.size() == 0 || line[0] == '#') {
            continue;
        }
        size_t eqoffset = line.find_first_of("=");
        if (eqoffset != std::string::npos) {
            std::string key = trim(line.substr(0, eqoffset));
            std::string val = trim(line.substr(eqoffset+1));
            if (debug) {
                Debug("%s \"%s\"=\"%s\"\n", __func__, key.c_str(), val.c_str());
            }
            map.insert(std::make_pair(key, val));
        } else {
            Error("%s parse error: %s\n", __func__, buf);
        }
    }
    rsrc->close();

    Debug("%s loaded %d properties from %s\n",
        __func__, size(), rsrc->getBasename().c_str());
}

struct Defaults
{
    std::shared_ptr<Properties> props;

    Defaults();
    Defaults(file_ptr rsrc);

    std::string get_string(std::string qualifier, std::string key, std::string default_val = "");
    bool get_boolean(std::string qualifier, std::string key, bool default_val = false);
    int get_integer(std::string qualifier, std::string key, int default_val = 0);
    float get_float(std::string qualifier, std::string key, float default_val = 0.0f);
    vec2 get_vec2(std::string qualifier, std::string key, vec2 default_val = vec2(0));
    vec3 get_vec3(std::string qualifier, std::string key, vec3 default_val = vec3(0));
    vec4 get_vec4(std::string qualifier, std::string key, vec4 default_val = vec4(0));
    Rect get_rect(std::string qualifier, std::string key, Rect default_val = Rect{vec2(0),vec2(0)});
    color get_color(std::string qualifier, std::string key, color default_val = color(1,1,1,1));
};

inline Defaults::Defaults() : props() {}

inline Defaults::Defaults(file_ptr rsrc) : props(std::make_unique<Properties>(rsrc)) {}

inline std::string Defaults::get_string(std::string qualifier, std::string key, std::string default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        return props->get_property(prop);
    } else if (qualifier != "*") {
        return get_string("*", key, default_val);
    }
    return default_val;
}

inline bool Defaults::get_boolean(std::string qualifier, std::string key, bool default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        std::string strval = trim(props->get_property(prop));
        return (strval == "true" || strval == "1" || strval == "TRUE");
    } else if (qualifier != "*") {
        return get_boolean("*", key, default_val);
    }
    return default_val;
}

inline int Defaults::get_integer(std::string qualifier, std::string key, int default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        std::string strval = props->get_property(prop);
        return atoi(strval.c_str());
    } else if (qualifier != "*") {
        return get_integer("*", key, default_val);
    }
    return default_val;
}

inline float Defaults::get_float(std::string qualifier, std::string key, float default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        std::string strval = props->get_property(prop);
        return (float)atof(strval.c_str());
    } else if (qualifier != "*") {
        return get_float("*", key, default_val);
    }
    return default_val;
}

inline vec2 Defaults::get_vec2(std::string qualifier, std::string key, vec2 default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        vec4 v = get_vec4(qualifier, key, vec4(default_val.x, default_val.y,
            0, 0));
        return vec2(v.x,v.y);
    } else if (qualifier != "*") {
        return get_vec2("*", key, default_val);
    }
    return default_val;
}

inline vec3 Defaults::get_vec3(std::string qualifier, std::string key, vec3 default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        vec4 v = get_vec4(qualifier, key, vec4(default_val.x, default_val.y,
            default_val.z, 0));
        return vec3(v.x,v.y,v.z);
    } else if (qualifier != "*") {
        return get_vec3("*", key, default_val);
    }
    return default_val;
}

inline vec4 Defaults::get_vec4(std::string qualifier, std::string key, vec4 default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        std::string strval = props->get_property(prop);
        std::vector<std::string> strvec = split(strval.c_str(), ",");
        switch(strvec.size()) {
        case 1:
            return vec4(atof(strvec[0].c_str()));
        case 2:
            return vec4(atof(strvec[0].c_str()), atof(strvec[1].c_str()), 0, 1);
        case 3:
            return vec4(atof(strvec[0].c_str()), atof(strvec[1].c_str()),
                        atof(strvec[2].c_str()), 1);
        case 4:
            return vec4(atof(strvec[0].c_str()), atof(strvec[1].c_str()),
                        atof(strvec[2].c_str()), atof(strvec[3].c_str()));
        }
        return vec4(0);
    } else if (qualifier != "*") {
        return get_vec4("*", key, default_val);
    }
    return default_val;
}

inline Rect Defaults::get_rect(std::string qualifier, std::string key, Rect default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        std::string strval = props->get_property(prop);
        std::vector<std::string> strvec = split(strval.c_str(), ",");
        if (strvec.size() == 4) {
            float v1 = atof(strvec[0].c_str());
            float v2 = atof(strvec[1].c_str());
            float v3 = atof(strvec[2].c_str());
            float v4 = atof(strvec[3].c_str());
            return Rect{vec2(v1, v2),vec2(v3, v4)};
        }
    } else if (qualifier != "*") {
        return get_rect("*", key, default_val);
    }
    return default_val;
}

inline color Defaults::get_color(std::string qualifier, std::string key, color default_val)
{
    std::string prop = qualifier + std::string(".") + key;
    if (props->property_exists(prop)) {
        std::string strval = props->get_property(prop);
        std::vector<std::string> strvec = split(strval.c_str(), ",");
        if (strvec.size() == 1) {
            return color(strvec[0]);
        } else if (strvec.size() == 3) {
            float r = atof(strvec[0].c_str());
            float g = atof(strvec[1].c_str());
            float b = atof(strvec[2].c_str());
            return color(r, g, b, 1);
        } else  if (strvec.size() == 4) {
            float r = atof(strvec[0].c_str());
            float g = atof(strvec[1].c_str());
            float b = atof(strvec[2].c_str());
            float a = atof(strvec[3].c_str());
            return color(r, g, b, a);
        }
    } else if (qualifier != "*") {
        return get_color("*", key, default_val);
    }
    return default_val;
}

struct Context
{
    static const char* getDefaultsPath() { return "shaders/default.properties"; }

    Defaults defaults;
    font_manager *manager;

    Context() : defaults(file::getFile(getDefaultsPath())), manager(nullptr) {}
};

struct Sizing
{
    vec3 minimum;
    vec3 preferred;
};

struct Visible
{
    static const bool debug = true;

    const char *class_name;
    Context *context;
    Visible *parent;
    bool valid;
    bool visible;
    bool enabled;
    bool can_focus;
    bool focused;
    vec3 position;
    vec3 assigned_size;
    vec3 minimum_size;
    vec3 maximum_size;
    vec3 preferred_size;
    vec4 padding;
    vec4 border;
    vec4 margin;
    float border_radius;
    color fill_color;
    color stroke_color;
    color text_color;
    float font_size;
    float text_leading;
    std::string font_family;
    std::string font_style;

    Visible() = delete;
    Visible(const char* class_name) :
        class_name(class_name),
        context(nullptr),
        parent(nullptr),
        valid(false),
        visible(false),
        enabled(false),
        can_focus(false),
        focused(false),
        border_radius(0),
        font_size(0),
        text_leading(0)
    {}
    virtual ~Visible() = default;

    virtual Visible* get_parent() { return parent; }
    virtual void set_parent(Visible *v) { parent = v; invalidate(); }

    virtual void invalidate() { valid = false; }
    virtual void set_visible(bool v) { visible = v; invalidate(); }
    virtual void set_enabled(bool v) { enabled = v; invalidate(); }
    virtual void set_can_focus(bool v) { can_focus = v; invalidate(); }
    virtual void request_focus() {}
    virtual void release_focus() {}
    virtual void next_focus() {}
    virtual void prev_focus() {}

    virtual bool is_valid() { return valid; }
    virtual bool is_visible() { return visible; }
    virtual bool is_enabled() { return enabled; }
    virtual bool is_focusable() { return can_focus; }
    virtual bool is_focused() { return focused; }
    virtual bool get_can_focus() { return can_focus; }

    virtual void set_position(vec3 p) { position = p; invalidate(); }
    virtual void set_padding(vec4 v) { padding = v; invalidate(); }
    virtual void set_border(vec4 v) { border = v; invalidate(); }
    virtual void set_margin(vec4 v) { margin = v; invalidate(); }
    virtual void set_border_radius(float r) { border_radius = r; invalidate(); }
    virtual void set_fill_color(color c) { fill_color = c; invalidate(); }
    virtual void set_stroke_color(color c) { stroke_color = c; invalidate(); }
    virtual void set_text_color(color c) { text_color = c; invalidate(); }
    virtual void set_text_leading(float f) { text_leading = f; invalidate(); }
    virtual void set_font_size(float s) { font_size = s; invalidate(); }
    virtual void set_font_family(std::string s) { font_family = s; invalidate(); }
    virtual void set_font_style(std::string s) { font_style = s; invalidate(); }

    virtual vec3 get_position() { return position; }
    virtual vec4 get_padding() { return padding; }
    virtual vec4 get_border() { return border; }
    virtual vec4 get_margin() { return margin; }
    virtual float get_border_radius() { return border_radius; }
    virtual color get_fill_color() { return fill_color; }
    virtual color get_stroke_color() { return stroke_color; }
    virtual color get_text_color() { return text_color; }
    virtual float get_font_size() { return font_size; }
    virtual float get_text_leading() { return text_leading; }
    virtual std::string get_font_family() { return font_family; }
    virtual std::string get_font_style() { return font_style; }

    virtual font_face* get_font_face()
    {
        font_data d{
            .familyName = font_family,
            .styleName = font_style,
            .fontWeight = font_weight_any,
            .fontSlope = font_slope_any,
            .fontStretch = font_stretch_any,
            .fontSpacing = font_spacing_any
        };
        assert(get_context()->manager);
        return get_context()->manager->findFontByData(d);
    }

    virtual Context* get_context()
    {
        /* todo - find another way of injecting context */
        static Context ctx;
        return &ctx;
    }

    virtual Defaults* get_defaults()
    {
        return &get_context()->defaults;
    }

    virtual void load_properties()
    {
        Defaults *d = get_defaults();
        set_visible(d->get_boolean(class_name, "visible", true));
        set_enabled(d->get_boolean(class_name, "enabled", true));
        set_can_focus(d->get_boolean(class_name, "can-focus", true));
        set_position(d->get_vec3(class_name, "position", vec3(0)));
        set_minimum_size(d->get_vec3(class_name, "minimum-size", vec3(0)));
        set_maximum_size(d->get_vec3(class_name, "maximum-size", vec3(0)));
        set_preferred_size(d->get_vec3(class_name, "preferred-size", vec3(0)));
        set_padding(d->get_vec4(class_name, "padding", vec4(0)));
        set_border(d->get_vec4(class_name, "border", vec4(0)));
        set_margin(d->get_vec4(class_name, "margin", vec4(0)));
        set_border_radius(d->get_float(class_name, "border-radius", 0));
        set_fill_color(d->get_color(class_name, "fill-color", color(0,0,0,1)));
        set_stroke_color(d->get_color(class_name, "stroke-color", color(1,1,1,1)));
        set_text_color(d->get_color(class_name, "text-color", color(1,1,1,1)));
        set_text_leading(d->get_float(class_name, "text-leading", 1.5f));
        set_font_size(d->get_float(class_name, "font-size", 14.0f));
        set_font_family(d->get_string(class_name, "font-family", "*"));
        set_font_style(d->get_string(class_name, "font-style", "*"));
    }

    /*
     * accessors for border, margin and padding size totals
     *
     * - vec3(left + right, top + bottom, 0)
     */

    virtual vec3 p()
    {
        return vec3(border[0] + border[2], border[1] + border[3], 0);
    }

    virtual vec3 b()
    {
        return vec3(margin[0] + margin[2], margin[1] + margin[3], 0);
    }

    virtual vec3 m()
    {
        return vec3(padding[0] + padding[2], padding[1] + padding[3], 0);
    }

    /*
     * size properties are used as hints for the size calculation, and are
     * returned unmodified so that they can be stored persistently.
     *
     * toplevel sizes includes margin, border, padding and content.
     */

    virtual vec3 get_minimum_size() { return minimum_size; }
    virtual vec3 get_maximum_size() { return maximum_size; }
    virtual vec3 get_preferred_size() { return preferred_size; }

    virtual void set_minimum_size(vec3 v) { minimum_size = v; invalidate(); }
    virtual void set_maximum_size(vec3 v) { maximum_size = v; invalidate(); }
    virtual void set_preferred_size(vec3 v) { preferred_size = v; invalidate(); }

    /*
     * get_default_size returns preferred size bounded by minimum and maximum
     */
    virtual vec3 get_default_size()
    {
        return vec3(
            std::max(std::min(preferred_size.x,maximum_size.x),minimum_size.x),
            std::max(std::min(preferred_size.y,maximum_size.y),minimum_size.y),
            std::max(std::min(preferred_size.z,maximum_size.z),minimum_size.z)
        );
    };

    /*
     * calc_size computes dynamic size using minimum and preferred size.
     * override to recursively calculate space based on content.
     */
    virtual Sizing calc_size()
    {
        return Sizing{minimum_size, get_default_size()};
    }

    /*
     * grant_size assigned space, usually preferred size returned by calc_size.
     * containers override to recursively assign space to children.
     */
    virtual void grant_size(vec3 size)
    {
        assigned_size = size;
        invalidate();
    }

    /* get_assigned_size returns size assigned during layout */
    virtual vec3 get_assigned_size() { return assigned_size; }

    /* override to initialize */
    virtual void init(Canvas *c) {};

    /* override to implement custom layout */
    virtual void layout(Canvas *c) {};

    /* override to intercept events */
    virtual bool dispatch(Event *e) { return false; };
};

struct Container : Visible
{
    std::vector<std::unique_ptr<Visible>> children;

    Container(const char* class_name) : Visible(class_name) {}

    virtual bool has_children() { return children.size() > 0; }

    virtual void init(Canvas *c)
    {
        for (auto &o : children) {
            o->init(c);
        }
    }

    virtual void layout(Canvas *c)
    {
        for (auto &o : children) {
            o->layout(c);
        }
    }

    virtual bool dispatch(Event *e)
    {
        bool ret = false;
        for (auto &o : children) {
            if ((ret = o->dispatch(e))) break;
        }
        return ret;
    }

    virtual Sizing calc_size()
    {
        /* default container overlaps children so find maximum */
        Sizing maxsz{vec3(0),vec3(0)};
        for (auto &o : children) {
            Sizing sz = o->calc_size();
            maxsz = Sizing{
                .minimum =
                    vec3(std::max(maxsz.minimum.x, sz.minimum.x),
                         std::max(maxsz.minimum.y, sz.minimum.y),
                         std::max(maxsz.minimum.y, sz.minimum.y)),
                .preferred =
                    vec3(std::max(maxsz.preferred.x, sz.preferred.x),
                         std::max(maxsz.preferred.y, sz.preferred.y),
                         std::max(maxsz.preferred.y, sz.preferred.y))
            };
        }
        return maxsz;
    }

    virtual void grant_size(vec3 size)
    {
        /* default container overlaps children so assign same space */
        for (auto &o : children) {
            o->grant_size(size);
        }
    }

    virtual void add_child(Visible *c)
    {
        c->set_parent(this);
        children.push_back(std::unique_ptr<Visible>(c));
    }
};

struct Root : Container
{
    Root(font_manager *manager) : Container("Root")
    {
        get_context()->manager = manager;
        load_properties();
    }

    virtual void layout(Canvas *c)
    {
        Container::init(c);
        Container::grant_size(Container::calc_size().preferred);
        Container::layout(c);
    }

};

struct Frame : Container
{
    std::string text;

    Rectangle *rc;
    Text *tc;
    vec2 ts;

    Frame() :
        Container("Frame"),
        rc(nullptr),
        tc(nullptr)
    {
        load_properties();
    }

    virtual std::string get_text() { return text; }
    virtual void set_text(std::string str) { text = str; invalidate(); }

    virtual Sizing calc_size()
    {
        assert(tc);

        tc->set_text(text);
        tc->set_size(font_size);
        tc->set_face(get_font_face());

        ts = tc->get_text_size() * vec2(1.0f,text_leading);
        Sizing cs = has_children() ? children[0]->calc_size() : Sizing();

        vec3 min = p() + b() + m() +
            vec3(std::max(ts.x, cs.minimum.x), ts.y + cs.minimum.y, 0);
        vec3 pref = p() + b() + m() +
            vec3(std::max(ts.x, cs.preferred.x), ts.y + cs.preferred.y, 0);

        Sizing s = {
            vec3(std::max(min.x, get_minimum_size().x),
                 std::max(min.y, get_minimum_size().y),
                 std::max(min.z, get_minimum_size().z)),
            vec3(std::max(pref.x, get_default_size().x),
                 std::max(pref.y, get_default_size().y),
                 std::max(pref.z, get_default_size().z))
        };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return s;
    }

    virtual void grant_size(vec3 size)
    {
        Visible::grant_size(size);
        if (has_children()) {
            vec3 cs = size - (p() + b() + m()) - vec3(0, ts.y, 0);
            children[0]->grant_size(cs);
        }
    }

    virtual void init(Canvas *c)
    {
        if (!rc) {
            rc = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!tc) {
            tc = c->new_text();
        }
        Container::init(c);
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};
        Brush text_brush{BrushSolid, {}, { text_color }};

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;

        rc->pos = position;
        rc->set_origin(vec2(half_size));
        rc->set_halfsize(vec2(half_size));
        rc->set_radius(border_radius);
        rc->set_visible(visible);
        rc->set_fill_brush(fill_brush);
        rc->set_stroke_brush(stroke_brush);
        rc->set_stroke_width(border[0]);

        tc->pos = position + vec3(0.0f,-half_size.y + ts.y/2.0f,0);
        tc->set_text(text);
        tc->set_size(font_size);
        tc->set_face(get_font_face());
        tc->set_fill_brush(text_brush);
        tc->set_stroke_brush(Brush{BrushNone, {}, {}});
        tc->set_stroke_width(0);
        tc->set_halign(text_halign_center);
        tc->set_valign(text_valign_center);
        tc->set_visible(visible);

        if (has_children()) {
            children[0]->set_position(vec3(0, ts.y/2.0f, 0));
        }

        Container::layout(c);

        valid = true;
    }
};

struct GridData
{
    size_t left;
    size_t top;
    size_t width;
    size_t height;
    
    Sizing sz;
    Rect rect;
};

struct Grid : Container
{
    size_t rows_count;
    size_t cols_count;
    bool rows_homogeneous;
    bool cols_homogeneous;
    bool rows_expand;
    bool cols_expand;

    std::map<Visible*,GridData> datamap;
    std::vector<float> col_widths;
    std::vector<float> row_heights;
    float max_col_width;
    float max_row_height;
    std::vector<Rect> sizes;
    Sizing s;

    Grid() :
        Container("Grid"),
        col_widths(),
        row_heights(),
        max_col_width(0),
        max_row_height(0),
        sizes(),
        s{.minimum = vec3(0), .preferred = vec3(0)}
    {
        load_properties();
    }

    virtual size_t get_rows_count() { return rows_count; }
    virtual size_t get_cols_count() { return cols_count; }
    virtual bool is_rows_expand() { return rows_expand; }
    virtual bool is_cols_expand() { return cols_expand; }
    virtual bool is_rows_homogenous() { return rows_homogeneous; }
    virtual bool is_cols_homogeneous() { return cols_homogeneous; }
    virtual void set_rows_count(size_t n) { rows_count = n; invalidate(); }
    virtual void set_cols_count(size_t n) { cols_count = n; invalidate(); }
    virtual void set_rows_expand(bool n) { rows_expand = n; invalidate(); }
    virtual void set_cols_expand(bool n) { cols_expand = n; invalidate(); }
    virtual void set_rows_homogeneous(bool n) { rows_homogeneous = n; invalidate(); }
    virtual void set_cols_homogeneous(bool n) { cols_homogeneous = n; invalidate(); }

    virtual void load_properties()
    {
        Container::load_properties();
        Defaults *d = get_defaults();
        set_rows_expand(d->get_boolean(class_name, "rows-expand", true));
        set_cols_expand(d->get_boolean(class_name, "cols-expand", true));
        set_rows_homogeneous(d->get_boolean(class_name, "rows-homogeneous", true));
        set_cols_homogeneous(d->get_boolean(class_name, "cols-homogeneous", true));
    }

    void add_child(Visible *c, size_t left, size_t top, size_t width = 1, size_t height = 1)
    {
        Container::add_child(c);
        datamap[c] = GridData{ left, top, width, height };
    }

    virtual Sizing calc_size()
    {
        /* call calc_size on children, count rows and columns */
        rows_count = 0, cols_count = 0;
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di == datamap.end()) continue;
            GridData &grid_data = di->second;
            grid_data.sz = o->calc_size();
            cols_count = std::max(cols_count, grid_data.left + grid_data.width);
            rows_count = std::max(rows_count, grid_data.top + grid_data.height);
        }

        /* clear arrays */
        sizes.clear();
        sizes.resize(rows_count * cols_count);
        col_widths.clear();
        col_widths.resize(cols_count);
        row_heights.clear();
        row_heights.resize(rows_count);

        /* populate sizes array */
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di == datamap.end()) continue;
            GridData &grid_data = di->second;
            for (size_t i = 0; i < grid_data.width; i++) {
                for (size_t j = 0; j < grid_data.height; j++) {
                    size_t idx = (grid_data.top + j) * cols_count + grid_data.left + i;
                    sizes[idx].size = vec2(
                        grid_data.sz.preferred.x / grid_data.width,
                        grid_data.sz.preferred.y / grid_data.height
                    );
                }
            }
        }

        /* find max_col_width and max width for each column */
        max_col_width = 0;
        for (size_t i = 0; i < cols_count; i++) {
            for (size_t j = 0; j < rows_count; j++) {
                col_widths[i] = std::max(col_widths[i], sizes[j*cols_count+i].size.x);
            }
            max_col_width = std::max(col_widths[i], max_col_width);
        }

        /* find max_row_height and max height for each row */
        max_row_height = 0;
        for (size_t j = 0; j < rows_count; j++) {
            for (size_t i = 0; i < cols_count; i++) {
                row_heights[j] = std::max(row_heights[j], sizes[j*cols_count+i].size.y);
            }
            max_row_height = std::max(row_heights[j], max_row_height);
        }

        /* calculate overall size */
        vec3 min(
            is_cols_homogeneous() ? max_col_width * cols_count :
                std::accumulate(col_widths.begin(), col_widths.end(), 0),
            is_rows_homogenous() ? max_row_height * rows_count :
                std::accumulate(row_heights.begin(), row_heights.end(), 0),
            0);

        /* return results */
        vec3 pref = min;
        s = {
            vec3(std::max(min.x, get_minimum_size().x),
                 std::max(min.y, get_minimum_size().y),
                 std::max(min.z, get_minimum_size().z)),
            vec3(std::max(pref.x, get_default_size().x),
                 std::max(pref.y, get_default_size().y),
                 std::max(pref.z, get_default_size().z))
        };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return s;
    }

    virtual void grant_size(vec3 size)
    {
        Visible::grant_size(size);

        float hratio = is_cols_expand() && s.minimum.x > 0 ?
            (float)s.preferred.x / (float)s.minimum.x : 1.0f;
        float vratio = is_rows_expand() && s.minimum.y > 0 ?
            (float)s.preferred.y / (float)s.minimum.y : 1.0f;

        /* expand cell sizes horizontally */
        for (size_t j = 0; j < rows_count; j++) {
            for (size_t i = 0; i < cols_count; i++) {
                sizes[j*cols_count+i].size.x = (int)((is_cols_homogeneous() ?
                    max_col_width : col_widths[i]) * hratio);
            }
        }
        if (is_cols_expand()) {
            for (size_t j = 0; j < rows_count; j++) {
                float total_width = 0;
                for (int i = 0; i < cols_count; i++) {
                    total_width += sizes[j*cols_count+i].size.x;
                }
                while (total_width < assigned_size.x) {
                    for (size_t i = 0; i < cols_count && total_width < assigned_size.x; i++) {
                        sizes[j*cols_count+i].size.x++;
                        total_width++;
                    }
                }
            }
        }

        /* expand cell sizes vertically */
        for (size_t j = 0; j < rows_count; j++) {
            for (size_t i = 0; i < cols_count; i++) {
                sizes[j*cols_count+i].size.y = (int)((is_rows_homogenous() ?
                    max_row_height : row_heights[j]) * vratio);
            }
        }
        if (is_rows_expand()) {
            for (size_t i = 0; i < cols_count; i++) {
                float total_height = 0;
                for (int j = 0; j < rows_count; j++) {
                    total_height += sizes[j*cols_count+i].size.y;
                }
                while (total_height < assigned_size.y) {
                    for (size_t j = 0; j < rows_count && total_height < assigned_size.y; j++) {
                        sizes[j*cols_count+i].size.y++;
                        total_height++;
                    }
                }
            }
        }
        
        /* set x and y coordinates */
        float y = 0;
        for (size_t j = 0; j < rows_count; j++) {
            float x = 0;
            for (size_t i = 0; i < cols_count; i++) {
                size_t idx = j*cols_count+i;
                sizes[idx].pos = vec2(x, y);
                x += sizes[idx].size.x;
            }
            y += sizes[j * cols_count].size.y;
        }

        /* set grant space to children */
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di == datamap.end()) continue;
            GridData &grid_data = di->second;
            vec3 child_size(0);
            for (size_t i = 0; i < grid_data.width; i++) {
                for (size_t j = 0; j < grid_data.height; j++) {
                    size_t idx = (grid_data.top + j) * cols_count + grid_data.left + i;
                    child_size += vec3(sizes[idx].size, 0);
                }
            }
            size_t idx = grid_data.top * cols_count + grid_data.left;
            o->grant_size(child_size);
        }
    }

    virtual void init(Canvas *c)
    {
        Container::init(c);
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        /* set positions for children */
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di != datamap.end()) {
                GridData &grid_data = di->second;
                size_t idx = grid_data.top * cols_count + grid_data.left;
                o->set_position(position + vec3(sizes[idx].pos, 0) -
                    assigned_size/2.0f +
                    o->get_assigned_size()/2.0f);
            }
        }

        Container::layout(c);

        valid = true;
    }
};

struct Label : Visible
{
    std::string text;

    Rectangle *rc;
    Text *tc;
    vec2 ts;

    Label() :
        Visible("Label"),
        rc(nullptr),
        tc(nullptr)
    {
        load_properties();
    }

    virtual std::string get_text() { return text; }
    virtual void set_text(std::string str) { text = str; invalidate(); }

    virtual Sizing calc_size()
    {
        assert(tc);

        tc->set_text(text);
        tc->set_size(font_size);
        tc->set_face(get_font_face());

        ts = tc->get_text_size() * vec2(1.0f,text_leading);
        vec3 sz = p() + b() + m() + vec3(ts, 0);

        Sizing s = {
            vec3(std::max(sz.x, get_minimum_size().x),
                 std::max(sz.y, get_minimum_size().y),
                 std::max(sz.z, get_minimum_size().z)),
            vec3(std::max(sz.x, get_default_size().x),
                 std::max(sz.y, get_default_size().y),
                 std::max(sz.z, get_default_size().z))
        };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return s;
    }

    virtual void init(Canvas *c)
    {
        if (!rc) {
            rc = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!tc) {
            tc = c->new_text();
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};
        Brush text_brush{BrushSolid, {}, { text_color }};

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;

        rc->pos = position;
        rc->set_origin(vec2(half_size));
        rc->set_halfsize(vec2(half_size));
        rc->set_radius(border_radius);
        rc->set_visible(visible);
        rc->set_fill_brush(fill_brush);
        rc->set_stroke_brush(stroke_brush);
        rc->set_stroke_width(border[0]);

        tc->pos = position;
        tc->set_text(text);
        tc->set_size(font_size);
        tc->set_face(get_font_face());
        tc->set_fill_brush(text_brush);
        tc->set_stroke_brush(Brush{BrushNone, {}, {}});
        tc->set_stroke_width(0);
        tc->set_halign(text_halign_center);
        tc->set_valign(text_valign_center);
        tc->set_visible(visible);

        valid = true;
    }
};

struct Button : Visible
{
    std::string text;

    Rectangle *rc;
    Text *tc;
    vec2 ts;

    Button() :
        Visible("Button"),
        rc(nullptr),
        tc(nullptr)
    {
        load_properties();
    }

    virtual std::string get_text() { return text; }
    virtual void set_text(std::string str) { text = str; invalidate(); }

    virtual Sizing calc_size()
    {
        assert(tc);

        tc->set_text(text);
        tc->set_size(font_size);
        tc->set_face(get_font_face());

        ts = tc->get_text_size() * vec2(1.0f,text_leading);
        vec3 sz = p() + b() + m() + vec3(ts, 0);

        Sizing s = {
            vec3(std::max(sz.x, get_minimum_size().x),
                 std::max(sz.y, get_minimum_size().y),
                 std::max(sz.z, get_minimum_size().z)),
            vec3(std::max(sz.x, get_default_size().x),
                 std::max(sz.y, get_default_size().y),
                 std::max(sz.z, get_default_size().z))
        };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return s;
    }

    virtual void init(Canvas *c)
    {
        if (!rc) {
            rc = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!tc) {
            tc = c->new_text();
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};
        Brush text_brush{BrushSolid, {}, { text_color }};

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;

        rc->pos = position;
        rc->set_origin(vec2(half_size));
        rc->set_halfsize(vec2(half_size));
        rc->set_radius(border_radius);
        rc->set_visible(visible);
        rc->set_fill_brush(fill_brush);
        rc->set_stroke_brush(stroke_brush);
        rc->set_stroke_width(border[0]);

        tc->pos = position;
        tc->set_text(text);
        tc->set_size(font_size);
        tc->set_face(get_font_face());
        tc->set_fill_brush(text_brush);
        tc->set_stroke_brush(Brush{BrushNone, {}, {}});
        tc->set_stroke_width(0);
        tc->set_halign(text_halign_center);
        tc->set_valign(text_valign_center);
        tc->set_visible(visible);

        valid = true;
    }

    virtual bool dispatch(Event *e)
    {
        return false;
    }
};

struct Slider : Visible
{
    Orientation o;
    float control_size;
    float bar_thickness;
    float value;

    Rectangle *rc;
    Circle *cc;

    Slider() : Visible("Slider")
    {
        load_properties();

        rc = nullptr;
        cc = nullptr;
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
        set_control_size(d->get_float(class_name, "control-size", 10.0f));
        set_bar_thickness(d->get_float(class_name, "bar-thickness", 5.0f));
        set_value(d->get_float(class_name, "value", 0.0f));
    }

    virtual float get_control_size() { return control_size; }
    virtual void set_control_size(float v) { control_size = v; invalidate(); }
    virtual float get_bar_thickness() { return bar_thickness; }
    virtual void set_bar_thickness(float v) { bar_thickness = v; invalidate(); }
    virtual float get_value() { return value; }
    virtual void set_value(float v) { value = v; invalidate(); }

    virtual Sizing calc_size()
    {
        vec3 sz(0);

        Sizing s = {
            vec3(std::max(sz.x, get_minimum_size().x),
                 std::max(sz.y, get_minimum_size().y),
                 std::max(sz.z, get_minimum_size().z)),
            vec3(std::max(sz.x, get_default_size().x),
                 std::max(sz.y, get_default_size().y),
                 std::max(sz.z, get_default_size().z))
        };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return s;
    }

    virtual void init(Canvas *c)
    {
        if (!rc) {
            rc = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!cc) {
            cc = c->new_circle(vec2(0),0);
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};
        Brush text_brush{BrushSolid, {}, { text_color }};

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;

        rc->pos = position;
        rc->set_origin(vec2(half_size.x,bar_thickness/2.0f));
        rc->set_halfsize(vec2(half_size.x,bar_thickness/2.0f));
        rc->set_radius(border_radius);
        rc->set_visible(visible);
        rc->set_fill_brush(fill_brush);
        rc->set_stroke_brush(stroke_brush);
        rc->set_stroke_width(border[0]);

        /* todo - bounds and interval for value */
        cc->pos = position + vec3(-half_size.x + size_remaining.x*value, 0, 0);
        cc->set_origin(vec2(control_size));
        cc->set_radius(control_size);
        cc->set_visible(visible);
        cc->set_fill_brush(fill_brush);
        cc->set_stroke_brush(stroke_brush);
        cc->set_stroke_width(border[0]);

        valid = true;
    }

    virtual bool dispatch(Event *e)
    {
        return false;
    }
};

}