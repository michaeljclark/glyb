#pragma once

#include <sstream>
#include <iomanip>
#include <cmath>

namespace ui9 {

using vec3 = glm::vec3;
using ivec2 = glm::ivec2;

bool operator<(const vec3 &a, const vec3 &b)
{
    return (a[0] < b[0]) ||
           (a[0] == b[0] && a[1] < b[1]) ||
           (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
}

bool operator>(const vec3 &a, const vec3 &b) { return b < a; }
bool operator<=(const vec3 &a, const vec3 &b) { return a < b || a == b; }
bool operator>=(const vec3 &a, const vec3 &b) { return b < a || a == b; }

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

enum event_type
{
    keyboard = 1,
    mouse = 2,
};

enum event_qualifier
{
    pressed = 1,
    released = 2,
    motion = 3,
};

enum button
{
    left_button = 1,
    center_button = 2,
    right_button = 3,
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
    vec3 pos;
};

enum axis_2D
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

    /* int get_enum(std::string qualifier, std::string key, const char* names[],
                    int values[], const char* default_val = "default") */
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
    static const bool debug = false;

    const char *class_name;
    Context *context;
    Visible *parent;
    bool valid;
    bool visible;
    bool enabled;
    bool can_focus;
    bool can_move;
    bool focused;
    Sizing last_sizing;
    vec3 position;
    vec3 assigned_size;
    vec3 minimum_size;
    vec3 maximum_size;
    vec3 preferred_size;
    vec3 current_size;
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
    mat4 the_matrix;

    Visible() = delete;
    Visible(const char* class_name) :
        class_name(class_name),
        context(nullptr),
        parent(nullptr),
        valid(false),
        visible(false),
        enabled(false),
        can_focus(false),
        can_move(false),
        focused(false),
        last_sizing(),
        position(0),
        assigned_size(0),
        minimum_size(0),
        maximum_size(0),
        preferred_size(0),
        current_size(0),
        padding(0),
        border(0),
        margin(0),
        border_radius(0),
        fill_color(1,1,1,1),
        stroke_color(1,1,1,1),
        text_color(1,1,1,1),
        font_size(0),
        text_leading(0)
    {}
    virtual ~Visible() = default;

    virtual Visible* get_parent() { return parent; }
    virtual void set_parent(Visible *v) { parent = v; invalidate(); }

    virtual void invalidate()
    {
        valid = false;
        if (parent) {
            parent->invalidate();
        }
    }

    virtual void set_visible(bool v) { visible = v; invalidate(); }
    virtual void set_enabled(bool v) { enabled = v; invalidate(); }
    virtual void set_can_focus(bool v) { can_focus = v; invalidate(); }
    virtual void set_can_move(bool v) { can_move = v; invalidate(); }
    virtual void request_focus() {}
    virtual void release_focus() {}
    virtual void next_focus() {}
    virtual void prev_focus() {}

    virtual bool is_valid() { return valid; }
    virtual bool is_visible() { return visible; }
    virtual bool is_enabled() { return enabled; }
    virtual bool is_focusable() { return can_focus; }
    virtual bool is_moveable() { return can_move; }
    virtual bool is_focused() { return focused; }

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
    virtual mat4 get_the_matrix() { return the_matrix; }

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
        set_can_move(d->get_boolean(class_name, "can-move", true));
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

    virtual vec3 m()
    {
        return vec3(margin[0] + margin[2], margin[1] + margin[3], 0);
    }

    virtual vec3 b()
    {
        return vec3(border[0] + border[2], border[1] + border[3], 0);
    }

    virtual vec3 p()
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
    virtual vec3 get_current_size() { return current_size; }

    virtual void set_minimum_size(vec3 v) { minimum_size = v; invalidate(); }
    virtual void set_maximum_size(vec3 v) { maximum_size = v; invalidate(); }
    virtual void set_preferred_size(vec3 v) { preferred_size = v; invalidate(); }

    virtual void set_current_size(vec3 v)
    {
        current_size = vec3(
            std::max(std::max(std::min(v.x,maximum_size.x),minimum_size.x),last_sizing.minimum.x),
            std::max(std::max(std::min(v.y,maximum_size.y),minimum_size.y),last_sizing.minimum.y),
            std::max(std::max(std::min(v.z,maximum_size.z),minimum_size.z),last_sizing.minimum.z)
        );
        invalidate();
    }

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
        return (last_sizing = Sizing{minimum_size, get_default_size()});
    }

    virtual Sizing get_last_sizing() { return last_sizing; }

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
    virtual void layout(Canvas *c) { valid = true; };

    /* override to intercept events */
    virtual bool dispatch(Event *e) { return false; };
};

struct Container : Visible
{
    std::vector<std::unique_ptr<Visible>> children;
    std::vector<Sizing> sizes;

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
        for (auto ci = children.rbegin(); ci != children.rend(); ci++) {
            if ((ret = (*ci)->dispatch(e))) break;
        }
        return ret;
    }

    virtual Sizing calc_size()
    {
        /* find bounds containing all children */
        sizes.resize(children.size());
        for (size_t i = 0; i < children.size(); i++) {
            sizes[i] = children[i]->calc_size();
        }
        vec3 lb(std::numeric_limits<float>::max());
        vec3 hm(std::numeric_limits<float>::min());
        vec3 hp(std::numeric_limits<float>::min());
        for (size_t i = 0; i < children.size(); i++) {
            vec3 clb = children[i]->get_position();
            vec3 chm = clb + sizes[i].minimum.x;
            vec3 chp = clb + sizes[i].preferred.x;
            lb = vec3(std::min(clb.x, lb.x),
                       std::min(clb.y, lb.y),
                       std::min(clb.z, lb.z));
            hm = vec3(std::max(chm.x, hm.x),
                       std::max(chm.y, hm.y),
                       std::max(chm.z, hm.z));
            hp = vec3(std::max(chp.x, hp.x),
                       std::max(chp.y, hp.y),
                       std::max(chp.z, hp.z));
        }
        return (last_sizing = Sizing{ hm - lb, hp - lb });
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

    virtual void grant_size(vec3 size)
    {
        for (size_t i = 0; i < children.size(); i++) {
            vec3 current_size = children[i]->get_current_size();
            if (current_size.x == 0 && current_size.y == 0) {
                current_size = sizes[i].preferred;
                children[i]->set_current_size(current_size);
            }
            children[i]->grant_size(current_size);
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) {
            return;
        }
        Container::init(c);
        grant_size(Container::calc_size().preferred);
        Container::layout(c);
        valid = true;
    }

};

struct Frame : Container
{
    std::string text;
    float click_radius;

    Rectangle *rect;
    Text *label;
    vec2 scale;
    vec3 delta;
    vec3 orig_size;
    bool _in_title;
    bool _in_corner;

    Frame() :
        Container("Frame"),
        rect(nullptr),
        label(nullptr),
        delta(0),
        orig_size(0),
        _in_title(false),
        _in_corner(false)
    {
        load_properties();
    }

    virtual float get_click_radius() { return click_radius; }
    virtual void set_click_radius(float v) { click_radius = v; invalidate(); }

    virtual void load_properties()
    {
        Container::load_properties();
        Defaults *d = get_defaults();
        set_click_radius(d->get_float(class_name, "click-radius", 5.0f));
    }

    virtual std::string get_text() { return text; }
    virtual void set_text(std::string str) { text = str; invalidate(); }

    virtual Sizing calc_size()
    {
        assert(label);

        label->set_text(text);
        label->set_size(font_size);
        label->set_face(get_font_face());

        scale = label->get_text_size() * vec2(1.0f, text_leading);
        Sizing cs = has_children() ? children[0]->calc_size() : Sizing();

        vec3 min = p() + b() + m() +
            vec3(std::max(scale.x, cs.minimum.x), scale.y + cs.minimum.y, 0);
        vec3 pref = p() + b() + m() +
            vec3(std::max(scale.x, cs.preferred.x), scale.y + cs.preferred.y, 0);

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
        return (last_sizing = s);
    }

    virtual void grant_size(vec3 size)
    {
        Visible::grant_size(size);
        if (has_children()) {
            vec3 cs = size - (p() + b() + m()) - vec3(0, scale.y, 0);
            children[0]->grant_size(cs);
        }
    }

    virtual void init(Canvas *c)
    {
        if (!rect) {
            rect = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!label) {
            label = c->new_text();
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

        rect->pos = position;
        rect->set_origin(vec2(half_size));
        rect->set_halfsize(vec2(half_size));
        rect->set_radius(border_radius);
        rect->set_visible(visible);
        rect->set_fill_brush(fill_brush);
        rect->set_stroke_brush(stroke_brush);
        rect->set_stroke_width(border[0]);

        label->pos = position + vec3(0.0f,-half_size.y + scale.y/2.0f,0);
        label->set_text(text);
        label->set_size(font_size);
        label->set_face(get_font_face());
        label->set_fill_brush(text_brush);
        label->set_stroke_brush(Brush{BrushNone, {}, {}});
        label->set_stroke_width(0);
        label->set_halign(text_halign_center);
        label->set_valign(text_valign_center);
        label->set_visible(visible);

        if (has_children()) {
            children[0]->set_position(position + vec3(0, scale.y/2.0f, 0));
        }

        Container::layout(c);

        valid = true;
    }

    virtual bool dispatch(Event *e)
    {
        MouseEvent *me = reinterpret_cast<MouseEvent*>(e);

        if (Container::dispatch(e)) {
            return true;
        }

        if (!is_moveable() || e->type != mouse || me->button != left_button) {
            return false;
        }

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;
        vec3 frame_dist = me->pos - vec3(rect->pos,0);

        bool in_title = frame_dist.x >= -half_size.x && frame_dist.x <= half_size.x &&
                        frame_dist.y >= -half_size.y && frame_dist.y <= half_size.y;
        bool in_corner = fabsf(frame_dist.x) < half_size.x + click_radius &&
                         fabsf(frame_dist.y) < half_size.y + click_radius &&
                         fabsf(frame_dist.x) > half_size.x - click_radius &&
                         fabsf(frame_dist.y) > half_size.y - click_radius;

        if (e->qualifier == pressed) {
            if (!_in_title && !_in_corner && in_corner) {
                _in_corner = in_corner;
                delta = frame_dist;
                orig_size = assigned_size;
            } else if (!_in_title && !_in_corner && in_title) {
                _in_title = in_title;
                delta = frame_dist;
            }
        }

        if (_in_title) {
            set_position(me->pos - delta);
        }
        if (_in_corner) {
            /* todo - edge resize */
            /* todo - scale relative to opposite corner versus center */
            vec3 d = me->pos - delta - vec3(rect->pos,0);
            d *= vec3(1.0f - std::signbit(frame_dist.x) * 2.0f,
                      1.0f - std::signbit(frame_dist.y) * 2.0f, 1);
            if (std::signbit(frame_dist.x) == std::signbit(delta.x) &&
                std::signbit(frame_dist.y) == std::signbit(delta.y))
            {
                set_current_size(orig_size + d*2.0f);
            }
        }

        if (e->qualifier == released) {
            if (_in_title) _in_title = false;
            if (_in_corner) _in_corner = false;
        }

        return _in_title || _in_corner;
    }
};

enum size_hint_type { dynamic, minimum, preferred, fixed, ratio };

struct grid_data
{
    size_t left;
    size_t top;
    size_t width;
    size_t height;

    struct { ivec2 type; vec2 value; } hint;
    
    Sizing sz;
    Rect rect;

    Sizing sizing()
    {
        float min_x = 0.0f, min_y = 0.0f, pref_x = 0.0f, pref_y = 0.0f;

        switch (hint.type.x) {
        case dynamic:   min_x = sz.minimum.x;    break;
        case fixed:     min_x = hint.value.x;    break;
        case ratio:     min_x = sz.minimum.x;    break;
        default: break;
        }
        switch (hint.type.y) {
        case dynamic:   min_y = sz.minimum.y;    break;
        case fixed:     min_y = hint.value.y;    break;
        case ratio:     min_y = sz.minimum.y;    break;
        default: break;
        }

        switch (hint.type.x) {
        case dynamic:   pref_x = sz.preferred.x; break;
        case fixed:     pref_x = hint.value.x;   break;
        case ratio:     pref_x = sz.preferred.x; break;
        default: break;
        }
        switch (hint.type.y) {
        case dynamic:   pref_y = sz.preferred.y; break;
        case fixed:     pref_y = hint.value.y;   break;
        case ratio:     pref_y = sz.preferred.y; break;
        default: break;
        }

        return Sizing{vec3(min_x,min_y,0),vec3(pref_x,pref_y,0)};
    }
};

struct size_pair
{
    float minimum;
    float preferred;
};

struct Grid : Container
{
    static const bool debug = false;

    size_t rows_count;
    size_t cols_count;
    bool rows_homogeneous;
    bool cols_homogeneous;

    std::map<Visible*,grid_data> datamap;
    std::vector<size_pair> col_widths;
    std::vector<size_pair> row_heights;
    size_pair max_col_width;
    size_pair max_row_height;
    size_pair sum_col_width;
    size_pair sum_row_height;
    std::vector<Sizing> sizes;
    std::vector<grid_data*> datas;
    Sizing s;

    Grid() :
        Container("Grid"),
        col_widths(),
        row_heights(),
        max_col_width{0},
        max_row_height{0},
        sum_col_width{0},
        sum_row_height{0},
        sizes(),
        s{vec3(0), vec3(0)}
    {
        load_properties();
    }

    virtual size_t get_rows_count() { return rows_count; }
    virtual size_t get_cols_count() { return cols_count; }
    virtual bool is_rows_homogenous() { return rows_homogeneous; }
    virtual bool is_cols_homogeneous() { return cols_homogeneous; }
    virtual void set_rows_count(size_t n) { rows_count = n; invalidate(); }
    virtual void set_cols_count(size_t n) { cols_count = n; invalidate(); }
    virtual void set_rows_homogeneous(bool n) { rows_homogeneous = n; invalidate(); }
    virtual void set_cols_homogeneous(bool n) { cols_homogeneous = n; invalidate(); }

    virtual void load_properties()
    {
        Container::load_properties();
        Defaults *d = get_defaults();
        set_rows_homogeneous(d->get_boolean(class_name, "rows-homogeneous", true));
        set_cols_homogeneous(d->get_boolean(class_name, "cols-homogeneous", true));
    }

    void add_child(Visible *c, size_t left, size_t top,
        size_t width = 1, size_t height = 1,
        ivec2 hint = ivec2(dynamic), vec2 value = vec2(0.0f))
    {
        Container::add_child(c);
        datamap[c] = { left, top, width, height, { hint, value } };
    }

    Sizing& sizing(const size_t row, const size_t col)
    {
        return sizes[row * cols_count + col];
    }

    grid_data*& data(const size_t row, const size_t col)
    {
        return datas[row * cols_count + col];
    }

    virtual Sizing calc_size()
    {
        /* call calc_size on children, count rows and columns */
        rows_count = 0, cols_count = 0;
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di == datamap.end()) continue;
            auto &d = di->second;
            d.sz = o->calc_size();
            cols_count = std::max(cols_count, d.left + d.width);
            rows_count = std::max(rows_count, d.top + d.height);
        }

        /* clear arrays */
        sizes.clear();
        sizes.resize(rows_count * cols_count);
        datas.clear();
        datas.resize(rows_count * cols_count);
        col_widths.clear();
        col_widths.resize(cols_count);
        row_heights.clear();
        row_heights.resize(rows_count);

        /* populate sizes array */
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di == datamap.end()) continue;
            auto &d = di->second;
            vec3 min = d.sizing().minimum;
            vec3 pref = d.sizing().preferred;
            for (size_t i = 0; i < d.width; i++) {
                for (size_t j = 0; j < d.height; j++) {
                    sizing(d.top + j, d.left + i).minimum = vec3( min.x / d.width, min.y / d.height, 0 );
                    sizing(d.top + j, d.left + i).preferred = vec3( pref.x / d.width, pref.y / d.height, 0 );
                    data(d.top + j, d.left + i) = &d;
                }
            }
        }

        /* find max_col_width and max width for each column */
        max_col_width = { 0, 0 };
        sum_col_width = { 0, 0 };
        for (size_t i = 0; i < cols_count; i++) {
            for (size_t j = 0; j < rows_count; j++) {
                col_widths[i].minimum = std::max(col_widths[i].minimum, sizing(j,i).minimum.x);
                col_widths[i].preferred = std::max(col_widths[i].preferred, sizing(j,i).preferred.x);
            }
            sum_col_width.minimum += col_widths[i].minimum;
            sum_col_width.preferred += col_widths[i].preferred;
            max_col_width.minimum = std::max(col_widths[i].minimum, max_col_width.minimum);
            max_col_width.preferred = std::max(col_widths[i].preferred, max_col_width.preferred);
        }

        /* find max_row_height and max height for each row */
        max_row_height = { 0, 0 };
        sum_row_height = { 0, 0 };
        for (size_t j = 0; j < rows_count; j++) {
            for (size_t i = 0; i < cols_count; i++) {
                row_heights[j].minimum = std::max(row_heights[j].minimum, sizing(j,i).minimum.y);
                row_heights[j].preferred = std::max(row_heights[j].preferred, sizing(j,i).preferred.y);
            }
            sum_row_height.minimum += row_heights[j].minimum;
            sum_row_height.preferred += row_heights[j].preferred;
            max_row_height.minimum = std::max(row_heights[j].minimum, max_row_height.minimum);
            max_row_height.preferred = std::max(row_heights[j].preferred, max_row_height.preferred);
        }

        /* calculate overall size */
        vec3 min(
            is_cols_homogeneous() ? max_col_width.minimum * cols_count : sum_col_width.minimum,
            is_rows_homogenous() ? max_row_height.minimum * rows_count : sum_row_height.minimum,
            0);
        vec3 pref(
            is_cols_homogeneous() ? max_col_width.preferred * cols_count : sum_col_width.preferred,
            is_rows_homogenous() ? max_row_height.preferred * rows_count : sum_row_height.preferred,
            0);

        /* output statistics */
        if (debug) {
            printf("grid_properties {\n");
            printf("\tcols_homogeneous=%d\n", cols_homogeneous);
            printf("\trows_homogeneous=%d\n", rows_homogeneous);
            printf("}\n" "col_stats {\n");
            printf("\tmax { minimum=%f, preferred=%f }\n",
                max_col_width.minimum, max_col_width.preferred);
            printf("\tsum { minimum=%f, preferred=%f }\n",
                sum_row_height.minimum, sum_row_height.preferred);
            printf("}\n" "row_stats {\n");
            printf("\tmax { minimum=%f, preferred=%f }\n",
                max_row_height.minimum, max_row_height.preferred);
            printf("\tsum { minimum=%f, preferred=%f }\n",
                sum_col_width.minimum, sum_col_width.preferred);
            printf("}\n" "col_widths {\n");
            for (size_t i = 0; i < cols_count; i++) {
                printf("\t[%zu] = { minimum=%f, preferred=%f }\n",
                    i, col_widths[i].minimum, col_widths[i].preferred);
            }
            printf("}\n" "row_heights {\n");
            for (size_t j = 0; j < rows_count; j++) {
                printf("\t[%zu] = { minimum=%f, preferred=%f }\n",
                    j, row_heights[j].minimum, row_heights[j].preferred);
            }
            printf("}\n");
            printf("sizes {\n");
            for (size_t i = 0; i < cols_count; i++) {
                for (size_t j = 0; j < rows_count; j++) {
                    size_t idx = j * cols_count + i;
                    vec2 min = sizes[idx].minimum, pref = sizes[idx].preferred;
                    printf("\t[%zu][%zu] { minimum=(%f,%f) preferred=(%f,%f) }\n",
                        i, j, min.x, min.y, pref.x, pref.y);
                }
            }
            printf("}\n");
        }

        /* return results */
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
        return (last_sizing = s);
    }

    virtual void grant_size(vec3 size)
    {
        Visible::grant_size(size);

        /*
         * if rows or columns are homogeneous, then all rows or columns are
         * assigned the same size. if rows or columns are not homogeneous,
         * then size is assigned as follows:
         *
         * 1). for all items, assign minimum
         * 2). for all items, if space remaining, assign preferred/ratio
         */

        /* set column sizes horizontally */
        for (size_t j = 0; j < rows_count; j++)
        {
            float total_assigned = 0, total_ratios = 0;
            for (size_t i = 0; i < cols_count; i++)
            {
                float m = (int)((is_cols_homogeneous() ?
                    max_col_width.minimum : col_widths[i].minimum));
                float p = (int)((is_cols_homogeneous() ?
                    max_col_width.preferred : col_widths[i].preferred));

                if (data(j,i) && data(j,i)->hint.type.x == ratio) {
                    total_ratios += data(j,i)->hint.value.x;
                } else {
                    total_ratios += std::max(p,m) / m;
                }

                total_assigned += m;
                sizing(j,i).preferred.x = m;
            }
            float remaining = size.x - total_assigned;
            for (size_t i = 0; i < cols_count; i++)
            {
                float m = (int)((is_cols_homogeneous() ?
                    max_col_width.minimum : col_widths[i].minimum));
                float p = (int)((is_cols_homogeneous() ?
                    max_col_width.preferred : col_widths[i].preferred));

                float r = 0.0f;
                if (data(j,i) && data(j,i)->hint.type.x == ratio) {
                    r = data(j,i)->hint.value.x;
                } else {
                    r = std::max(p,m) / m;
                }

                sizing(j,i).preferred.x += remaining * (r / total_ratios);
            }
        }

        /* set row sizes vertically */
        for (size_t i = 0; i < cols_count; i++)
        {
            float total_assigned = 0, total_ratios = 0;
            for (size_t j = 0; j < rows_count; j++)
            {
                float m = (int)((is_rows_homogenous() ?
                    max_row_height.minimum : row_heights[j].minimum));
                float preferred = (int)((is_rows_homogenous() ?
                    max_row_height.preferred : row_heights[j].preferred));

                if (data(j,i) && data(j,i)->hint.type.y == ratio) {
                    total_ratios += data(j,i)->hint.value.y;
                } else {
                    total_ratios += std::max(preferred,m) / m;
                }

                total_assigned += m;
                sizing(j,i).preferred.y = m;
            }
            float remaining = size.y - total_assigned;
            for (size_t j = 0; j < rows_count; j++)
            {
                float m = (int)((is_rows_homogenous() ?
                    max_row_height.minimum : row_heights[j].minimum));
                float p = (int)((is_rows_homogenous() ?
                    max_row_height.preferred : row_heights[j].preferred));

                float r = 0.0f;
                if (data(j,i) && data(j,i)->hint.type.y == ratio) {
                    r = data(j,i)->hint.value.y;
                } else {
                    r = std::max(p,m) / m;
                }

                sizing(j,i).preferred.y += remaining * (r / total_ratios);
            }
        }

        /* set grant space to children */
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di == datamap.end()) continue;
            auto &d = di->second;
            vec3 child_size(0);
            for (size_t i = 0; i < d.width; i++) {
                for (size_t j = 0; j < d.height; j++) {
                    child_size += sizing(d.top + j, d.left + i).preferred;
                }
            }
            if (d.width > 1) child_size.y /= d.width;
            if (d.height > 1) child_size.x /= d.height;
            size_t idx = d.top * cols_count + d.left;
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

        /* gather column positions */
        float x = 0;
        std::vector<float> col_pos(cols_count);
        for (size_t i = 0; i < cols_count; i++) {
            float col_width = 0;
            for (size_t j = 0; j < rows_count; j++) {
                col_width = std::max(col_width, sizing(j,i).preferred.x);
            }
            col_pos[i] = x;
            x += col_width;
        }

        /* gather row positions */
        float y = 0;
        std::vector<float> row_pos(rows_count);
        for (size_t j = 0; j < rows_count; j++) {
            float row_height = 0;
            for (size_t i = 0; i < cols_count; i++) {
                row_height = std::max(row_height, sizing(j,i).preferred.y);
            }
            row_pos[j] = y;
            y += row_height;
        }

        /* set positions for children */
        for (auto &o : children) {
            auto di = datamap.find(o.get());
            if (di != datamap.end()) {
                auto &d = di->second;
                vec3 grid_pos = vec3(col_pos[d.left], row_pos[d.top], 0);
                vec3 origin = -assigned_size/2.0f + o->get_assigned_size()/2.0f;
                o->set_position(position + grid_pos + origin);
            }
        }

        Container::layout(c);

        valid = true;
    }
};

struct Label : Visible
{
    std::string text;

    Rectangle *rect;
    Text *label;
    vec2 scale;

    Label() :
        Visible("Label"),
        rect(nullptr),
        label(nullptr)
    {
        load_properties();
    }

    virtual std::string get_text() { return text; }
    virtual void set_text(std::string str) { text = str; invalidate(); }

    virtual Sizing calc_size()
    {
        assert(label);

        label->set_text(text);
        label->set_size(font_size);
        label->set_face(get_font_face());

        scale = label->get_text_size() * vec2(1.0f,text_leading);
        vec3 sz = p() + b() + m() + vec3(scale, 0);

        Sizing s = {
            vec3(std::max(sz.x, get_default_size().x),
                 std::max(sz.y, get_default_size().y),
                 std::max(sz.z, get_default_size().z)),
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
        return (last_sizing = s);
    }

    virtual void init(Canvas *c)
    {
        if (!rect) {
            rect = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!label) {
            label = c->new_text();
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

        rect->pos = position;
        rect->set_origin(vec2(half_size));
        rect->set_halfsize(vec2(half_size));
        rect->set_radius(border_radius);
        rect->set_visible(visible);
        rect->set_fill_brush(fill_brush);
        rect->set_stroke_brush(stroke_brush);
        rect->set_stroke_width(border[0]);

        label->pos = position;
        label->set_text(text);
        label->set_size(font_size);
        label->set_face(get_font_face());
        label->set_fill_brush(text_brush);
        label->set_stroke_brush(Brush{BrushNone, {}, {}});
        label->set_stroke_width(0);
        label->set_halign(text_halign_center);
        label->set_valign(text_valign_center);
        label->set_visible(visible);

        valid = true;
    }
};

struct Button : Visible
{
    std::string text;

    Rectangle *rect;
    Text *label;
    vec2 scale;

    Button() :
        Visible("Button"),
        rect(nullptr),
        label(nullptr)
    {
        load_properties();
    }

    virtual std::string get_text() { return text; }
    virtual void set_text(std::string str) { text = str; invalidate(); }

    virtual Sizing calc_size()
    {
        assert(label);

        label->set_text(text);
        label->set_size(font_size);
        label->set_face(get_font_face());

        scale = label->get_text_size() * vec2(1.0f,text_leading);
        vec3 sz = p() + b() + m() + vec3(scale, 0);

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
        return (last_sizing = s);
    }

    virtual void init(Canvas *c)
    {
        if (!rect) {
            rect = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!label) {
            label = c->new_text();
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

        rect->pos = position;
        rect->set_origin(vec2(half_size));
        rect->set_halfsize(vec2(half_size));
        rect->set_radius(border_radius);
        rect->set_visible(visible);
        rect->set_fill_brush(fill_brush);
        rect->set_stroke_brush(stroke_brush);
        rect->set_stroke_width(border[0]);

        label->pos = position;
        label->set_text(text);
        label->set_size(font_size);
        label->set_face(get_font_face());
        label->set_fill_brush(text_brush);
        label->set_stroke_brush(Brush{BrushNone, {}, {}});
        label->set_stroke_width(0);
        label->set_halign(text_halign_center);
        label->set_valign(text_valign_center);
        label->set_visible(visible);

        valid = true;
    }

    virtual bool dispatch(Event *e)
    {
        return false;
    }
};

struct Slider : Visible
{
    axis_2D axis;
    float control_size;
    float bar_thickness;
    float value;
    std::function<void(float)> callback;
    bool inside;

    Rectangle *rect;
    Circle *circle;

    Slider() :
        Visible("Slider"),
        axis(horizontal),
        value(0.0f),
        callback(),
        inside(false),
        rect(nullptr),
        circle(nullptr)
    {
        load_properties();
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
        set_control_size(d->get_float(class_name, "control-size", 10.0f));
        set_bar_thickness(d->get_float(class_name, "bar-thickness", 5.0f));
    }

    virtual axis_2D get_orientation() { return axis; }
    virtual void set_orientation(axis_2D v) { axis = v; invalidate(); }
    virtual float get_control_size() { return control_size; }
    virtual void set_control_size(float v) { control_size = v; invalidate(); }
    virtual float get_bar_thickness() { return bar_thickness; }
    virtual void set_bar_thickness(float v) { bar_thickness = v; invalidate(); }
    virtual float get_value() { return value; }
    virtual std::function<void(float)> get_callback() { return callback; }
    virtual void set_callback(std::function<void(float)> cb) { callback = cb; }

    virtual void set_value(float v)
    {
        if (v == value) {
            return;
        }
        value = v;
        invalidate();
        if (callback) {
            callback(value);
        }
    }

    virtual Sizing calc_size()
    {
        vec3 control_size = vec3(vec2(get_control_size() * 2), 0) +
            m() + b() + p();
        vec3 min_size = vec3( std::max(minimum_size.x,control_size.x),
                              std::max(minimum_size.y,control_size.y),
                              std::max(minimum_size.z,control_size.z) );
        vec3 pref_size = vec3( std::max(preferred_size.x,min_size.x),
                               std::max(preferred_size.y,min_size.y),
                               std::max(preferred_size.z,min_size.z) );
        Sizing s = { min_size, pref_size };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return (last_sizing = s);
    }

    virtual void init(Canvas *c)
    {
        if (!rect) {
            rect = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!circle) {
            circle = c->new_circle(vec2(0),0);
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};
        Brush text_brush{BrushSolid, {}, { text_color }};

        vec3 size_remaining = assigned_size - m() - b() - p();
        vec3 half_size = size_remaining / 2.0f;

        rect->pos = position;
        rect->set_radius(border_radius);
        rect->set_visible(visible);
        rect->set_fill_brush(fill_brush);
        rect->set_stroke_brush(stroke_brush);
        rect->set_stroke_width(border[0]);

        circle->set_origin(vec2(control_size));
        circle->set_radius(control_size);
        circle->set_visible(visible);
        circle->set_fill_brush(fill_brush);
        circle->set_stroke_brush(stroke_brush);
        circle->set_stroke_width(border[0]);

        if (axis == horizontal) {
            float control_offset = -half_size.x + size_remaining.x * value;
            rect->set_origin(vec2(half_size.x, bar_thickness/2.0f));
            rect->set_halfsize(vec2(half_size.x, bar_thickness/2.0f));
            circle->pos = position + vec3(control_offset, 0, 0);
        }

        if (axis == vertical) {
            float control_offset = -half_size.y + size_remaining.y * (1.0f - value);
            rect->set_origin(vec2(bar_thickness/2.0f, half_size.y));
            rect->set_halfsize(vec2(bar_thickness/2.0f, half_size.y));
            circle->pos = position + vec3(0, control_offset, 0);
        }

        valid = true;
    }

    virtual bool dispatch(Event *e)
    {
        MouseEvent *me = reinterpret_cast<MouseEvent*>(e);

        if (e->type != mouse) {
            return false;
        }

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;
        vec3 bar_dist = me->pos - vec3(rect->pos,0);
        float bar_offset = bar_thickness/2.0f + border[0];
        float control_dist = glm::distance(vec3(circle->pos,0), me->pos);
        bool in_control = control_dist < (control_size + border[0]);

        float new_value = 0.0f;
        bool in_bar = false;

        if (axis == horizontal) {
            new_value = (bar_dist.x + half_size.x) / size_remaining.x;
            in_bar = bar_dist.x >= -half_size.x && bar_dist.x <= half_size.x &&
                     bar_dist.y >= -bar_offset && bar_dist.y <= bar_offset;
        }

        if (axis == vertical) {
            new_value = 1.0f - ((bar_dist.y + half_size.y) / size_remaining.y);
            in_bar = bar_dist.y >= -half_size.y && bar_dist.y <= half_size.y &&
                     bar_dist.x >= -bar_offset && bar_dist.x <= bar_offset;
        }

        if (e->qualifier == pressed && !inside) {
            inside = in_control || in_bar;
        }

        if (inside) {
            set_value(std::min(std::max(new_value, 0.0f), 1.0f));
        }

        if (e->qualifier == released && inside) {
            inside = false;
        }

        return inside;
    }
};

struct Switch : Visible
{
    vec2 control_size;
    float control_radius;
    bool value;
    color active_color;
    color inactive_color;
    std::function<void(bool)> callback;
    bool inside;

    Rectangle *rect;
    Circle *circle;

    Switch() :
        Visible("Switch"),
        value(false),
        callback(),
        inside(false),
        rect(nullptr),
        circle(nullptr)
    {
        load_properties();
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
        set_control_size(d->get_vec2(class_name, "control-size", vec2(30.0f,15.0f)));
        set_control_radius(d->get_float(class_name, "control-radius", 12.5f));
        set_active_color(d->get_color(class_name, "active-color", color("#8080d0")));
        set_inactive_color(d->get_color(class_name, "inactive-color", color("#505050")));
    }

    virtual vec2 get_control_size() { return control_size; }
    virtual void set_control_size(vec2 v) { control_size = v; invalidate(); }
    virtual float get_control_radius() { return control_radius; }
    virtual void set_control_radius(float v) { control_radius = v; invalidate(); }
    virtual color get_active_color() { return active_color; }
    virtual void set_active_color(color c) { active_color = c; invalidate(); }
    virtual color get_inactive_color() { return inactive_color; }
    virtual void set_inactive_color(color c) { inactive_color = c; invalidate(); }
    virtual bool get_value() { return value; }
    virtual std::function<void(float)> get_callback() { return callback; }
    virtual void set_callback(std::function<void(bool)> cb) { callback = cb; }

    virtual void set_value(bool v)
    {
        if (v == value) {
            return;
        }
        value = v;
        invalidate();
        if (callback) {
            callback(value);
        }
    }

    virtual Sizing calc_size()
    {
        vec3 control_size = vec3(get_control_size() * vec2(2,1), 0) +
            m() + b() + p();
        vec3 min_size = vec3( std::max(minimum_size.x,control_size.x),
                              std::max(minimum_size.y,control_size.y),
                              std::max(minimum_size.z,control_size.z) );
        vec3 pref_size = vec3( std::max(preferred_size.x,min_size.x),
                               std::max(preferred_size.y,min_size.y),
                               std::max(preferred_size.z,min_size.z) );
        Sizing s = { min_size, pref_size };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return (last_sizing = s);
    }

    virtual void init(Canvas *c)
    {
        if (!rect) {
            rect = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!circle) {
            circle = c->new_circle(vec2(0),0);
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush active_brush{BrushSolid, {}, { active_color }};
        Brush inactive_brush{BrushSolid, {}, { inactive_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};
        Brush text_brush{BrushSolid, {}, { text_color }};

        vec3 size_remaining = assigned_size - m() - b() - p();
        vec3 half_size = size_remaining / 2.0f;

        rect->pos = position -half_size + vec3(control_size.x,half_size.y,0);
        rect->set_origin(control_size);
        rect->set_halfsize(control_size);
        rect->set_radius(border_radius);
        rect->set_visible(visible);
        rect->set_fill_brush(value ? active_brush : inactive_brush);
        rect->set_stroke_brush(stroke_brush);
        rect->set_stroke_width(border[0]);

        circle->pos = position -half_size +
            vec3(control_size.x, half_size.y, 0) *
            vec3(value ? 1.5f : 0.5f, 1, 1);
        circle->set_origin(control_size);
        circle->set_radius(control_radius);
        circle->set_visible(visible);
        circle->set_fill_brush(fill_brush);
        circle->set_stroke_brush(stroke_brush);
        circle->set_stroke_width(border[0]);

        valid = true;
    }

    virtual bool dispatch(Event *e)
    {
        MouseEvent *me = reinterpret_cast<MouseEvent*>(e);

        if (e->type != mouse) {
            return false;
        }

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;

        vec3 ctrl_dist = me->pos - vec3(rect->pos,0);
        bool in_control =
            ctrl_dist.y >= -control_size.y && ctrl_dist.y <= control_size.y &&
            ctrl_dist.x >= -control_size.x && ctrl_dist.x <= control_size.x;

        if (e->qualifier == pressed && !inside) {
            inside = in_control;
        }

        if (inside) {
            //
        }

        if (e->qualifier == released && inside) {
            inside = false;
            set_value(!value);
        }

        return inside;
    }
};

struct ChartData
{
    virtual size_t num_rows() = 0;
    virtual size_t num_columns() = 0;
    virtual float get_data(size_t col, size_t row) = 0;
};

struct ChartDataSimple : ChartData
{
    std::vector<float> data;

    ChartDataSimple(std::vector<float> data) : data(data) {}

    virtual size_t num_rows() { return data.size(); }
    virtual size_t num_columns() { return 1; }
    virtual float get_data(size_t col, size_t row) { return data[row]; }
};

enum location_2d
{
    top,
    bottom,
    left,
    right
};

struct ChartAxis : Visible
{
    static const bool debug = false;

    float value_min;
    float value_max;
    float value_range;
    float scale_min;
    float scale_max;
    float scale_incr;
    float scale_range;
    float preset_scale_min;
    float preset_scale_max;
    float left_space;
    float right_space;
    float top_space;
    float bottom_space;
    location_2d location;
    int precision;
    float text_margin;
    float tick_length;
    color tick_color;
    size_t num_labels;

    std::shared_ptr<ChartData> data;
    std::vector<float> scale_values;

    Path *line_path;
    std::vector<Text*> labels;

    ChartAxis() :
        Visible("ChartAxis"),
        value_min(std::numeric_limits<float>::quiet_NaN()),
        value_max(std::numeric_limits<float>::quiet_NaN()),
        value_range(std::numeric_limits<float>::quiet_NaN()),
        scale_min(std::numeric_limits<float>::quiet_NaN()),
        scale_max(std::numeric_limits<float>::quiet_NaN()),
        scale_incr(std::numeric_limits<float>::quiet_NaN()),
        preset_scale_min(std::numeric_limits<float>::quiet_NaN()),
        preset_scale_max(std::numeric_limits<float>::quiet_NaN()),
        left_space(0),
        right_space(0),
        top_space(0),
        bottom_space(0),
        location(left),
        num_labels(0),
        data(),
        scale_values(),
        line_path(nullptr),
        labels()
    {
        load_properties();
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
        set_precision(d->get_integer(class_name, "precision", 0));
        set_text_margin(d->get_float(class_name, "text-margin", 10.0f));
        set_tick_length(d->get_float(class_name, "tick-length", 5.0f));
        set_tick_color(d->get_color(class_name, "tick-color", color(1,1,1,1)));
    }

    virtual void set_data(std::shared_ptr<ChartData> data) { this->data = data; }

    virtual location_2d get_location() { return location; }
    virtual int get_precision() { return precision; }
    virtual float get_left_space() { return left_space; }
    virtual float get_right_space() { return right_space; }
    virtual float get_top_space() { return top_space; }
    virtual float get_bottom_space() { return bottom_space; }
    virtual float get_text_margin() { return text_margin; }
    virtual float get_tick_length() { return tick_length; }
    virtual color get_tick_color() { return tick_color; }

    virtual void set_location(location_2d v) { location = v; invalidate(); }
    virtual void set_precision(int v) { precision = v; }
    virtual void set_left_space(float v) { left_space = v; invalidate(); }
    virtual void set_right_space(float v) { right_space = v; invalidate(); }
    virtual void set_top_space(float v) { top_space = v; invalidate(); }
    virtual void set_bottom_space(float v) { bottom_space = v; invalidate(); }
    virtual void set_text_margin(float v) { text_margin = v; invalidate(); }
    virtual void set_tick_length(float v) { tick_length = v; invalidate(); }
    virtual void set_tick_color(color c) { tick_color = c; invalidate(); }

    void calc_scale(size_t column, float incr_size, float incr_space)
    {
        scale_values.clear();

        size_t n = data->num_rows();
        value_min = std::numeric_limits<float>::max();
        value_max = std::numeric_limits<float>::min();
        for (size_t i = 0; i < n; i++) {
            float v = (column == -1) ? i : data->get_data(column, i);
            value_min = std::min(value_min,v);
            value_max = std::max(value_max,v);
        }
        value_range = value_max - value_min;

        size_t max_labels = (size_t)(incr_space / incr_size);
        if (max_labels <= 0) {
            return;
        }

        float round_mag = powf(10, (float)precision);
        float round_incr = 1.0f / round_mag;

        scale_min = (preset_scale_min == preset_scale_min) ?
            preset_scale_min : floorf(value_min * round_mag) * round_incr;
        scale_max = (preset_scale_max == preset_scale_max) ?
            preset_scale_max : ceilf(value_max * round_mag) * round_incr;

        if (scale_min == scale_max) {
            scale_min -= round_incr;
            scale_max += round_incr;
        }
        scale_incr = round_incr;

        size_t num_labels = (size_t)(value_range / scale_incr);
        while (num_labels > max_labels) {
            scale_incr *= 10;
            num_labels = (size_t)(value_range / scale_incr);
        }

        scale_min = floorf(scale_min / scale_incr) * scale_incr;
        scale_max = ceilf(scale_max / scale_incr) * scale_incr;
        scale_range = scale_max - scale_min;
        if (debug) {
            Debug("scale_min=%f scale_max=%f scale_incr=%f scale_range=%f\n",
                scale_min, scale_max, scale_incr, scale_range);
        }
        for (float scale_val = scale_min; scale_val <= scale_max; scale_val += scale_incr) {
            scale_values.push_back(scale_val);
        }
    }

    virtual void init(Canvas *c)
    {
        if (!line_path) {
            line_path = c->new_path(vec2(0), vec2(0));
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        TextStyle text_style_left_default{
            get_font_size(), get_font_face(),
            text_halign_right, text_valign_center, "en",
            Brush{BrushSolid, { }, { color(1.0f,1.0f,1.0f,1.0f) }},
            Brush{BrushNone, { }, { }}
        };

        TextStyle text_style_bottom_default{
            get_font_size(), get_font_face(),
            text_halign_center, text_valign_bottom, "en",
            Brush{BrushSolid, { }, { color(1.0f,1.0f,1.0f,1.0f) }},
            Brush{BrushNone, { }, { }}
        };

        Brush tick_brush{BrushSolid, {}, { tick_color }};

        vec3 axis_space(left_space + right_space, top_space + bottom_space, 0);

        vec3 size_remaining = assigned_size;
        vec3 half_size = size_remaining / 2.0f;

        vec3 az = size_remaining - p() - b() - m();
        vec3 sz = az - axis_space;
        vec3 bz = vec3(padding[0] + border[0] + margin[0], padding[1] + border[1] + margin[1], 0);
        vec3 tz = bz + vec3(left_space, top_space, 0);

        line_path->clear();
        line_path->pos = position;
        line_path->set_offset({0, 0});
        line_path->set_size(size_remaining);
        line_path->set_fill_brush(tick_brush);
        line_path->set_stroke_brush(tick_brush);
        line_path->set_stroke_width(border[0]);

        for (size_t i = 0; i < scale_values.size(); i++) {
            float value = scale_values[i];
            std::stringstream ss;
            ss << std::setprecision(precision) << std::fixed << value;
            Text *text = nullptr;
            if (labels.size() < i + 1) {
                text = c->new_text();
                labels.push_back(text);
            } else {
                text = labels[i];
            }
            text->set_text(ss.str());

            if (location == left)
            {
                float x = tz.x - text_margin;
                float y = tz.y + sz.y * (scale_range - (value - scale_min))/scale_range;
                float tick_x1 = tz.x, tick_x2 = tick_x1 - tick_length;
                vec3 text_pos = position - half_size + vec3(x,y,0);
                text->set_text_style(text_style_left_default);
                text->set_position(text_pos);
                text->set_visible(true);
                line_path->new_line({tick_x1,y},{tick_x2,y});
            }
            else if (location == bottom)
            {
                float y = tz.y + sz.y + text_margin;
                float x = tz.x + sz.x * (value - scale_min)/scale_range;
                float tick_y1 = tz.y + sz.y, tick_y2 = tick_y1 + tick_length;
                vec3 text_pos = position - half_size + vec3(x,y,0);
                text->set_text_style(text_style_bottom_default);
                text->set_position(text_pos);
                text->set_visible(true);
                line_path->new_line({x,tick_y1},{x,tick_y2});
            }
        }
        for (size_t j = scale_values.size(); j < labels.size(); j++) {
            labels[j]->set_visible(false);
        }
    }
};

struct ChartGrid : Visible
{
    float left_space;
    float right_space;
    float top_space;
    float bottom_space;
    color grid_color;
    bool border_only;

    std::shared_ptr<ChartData> data;
    ChartAxis *axis;

    Path *grid_path;

    ChartGrid() :
        Visible("ChartGrid"),
        left_space(0),
        right_space(0),
        top_space(0),
        bottom_space(0),
        grid_color(),
        border_only(false),
        data(),
        axis(),
        grid_path(nullptr)
    {
        load_properties();
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
    }

    virtual void set_data(std::shared_ptr<ChartData> data) { this->data = data; }
    virtual void set_axis(ChartAxis *axis) { this->axis = axis; }

    virtual void set_left_space(float v) { left_space = v; invalidate(); }
    virtual void set_right_space(float v) { right_space = v; invalidate(); }
    virtual void set_top_space(float v) { top_space = v; invalidate(); }
    virtual void set_bottom_space(float v) { bottom_space = v; invalidate(); }
    virtual void set_grid_color(color c) { grid_color = c; invalidate(); }

    virtual float get_left_space() { return left_space; }
    virtual float get_right_space() { return right_space; }
    virtual float get_top_space() { return top_space; }
    virtual float get_bottom_space() { return bottom_space; }
    virtual color get_grid_color() { return grid_color; }

    virtual void init(Canvas *c)
    {
        if (!grid_path) {
            grid_path = c->new_path(vec2(0), vec2(0));
        }
    }

    virtual void layout(Canvas *c)
    {
        Brush grid_brush{BrushSolid, {}, { grid_color }};

        vec3 axis_space(left_space + right_space, top_space + bottom_space, 0);

        vec3 size_remaining = assigned_size;
        vec3 half_size = size_remaining / 2.0f;

        vec3 az = size_remaining - p() - b() - m();
        vec3 sz = az - axis_space;
        vec3 bz = vec3(padding[0] + border[0] + margin[0], padding[1] + border[1] + margin[1], 0);
        vec3 tz = bz + vec3(left_space, top_space, 0);

        grid_path->clear();
        grid_path->pos = position;
        grid_path->new_contour();
        grid_path->set_offset({0, 0});
        grid_path->set_size(size_remaining);
        grid_path->set_fill_brush(grid_brush);
        grid_path->set_stroke_brush(grid_brush);
        grid_path->set_stroke_width(border[0]);

        float scale_min = axis->scale_min;
        float scale_max = axis->scale_max;
        float scale_range = axis->scale_range;

        size_t n = data->num_rows();

        if (border_only) {
            grid_path->new_line({tz.x       ,tz.y       },{tz.x + sz.x,tz.y       });
            grid_path->new_line({tz.x + sz.x,tz.y       },{tz.x + sz.x,tz.y + sz.y});
            grid_path->new_line({tz.x + sz.x,tz.y + sz.y},{tz.x       ,tz.y + sz.y});
            grid_path->new_line({tz.x       ,tz.y + sz.y},{tz.x       ,tz.y       });
        } else {
            for (size_t i = 0; i < axis->scale_values.size(); i++) {
                float v = axis->scale_values[i];
                float y = tz.y + sz.y * (scale_range - (v - scale_min))/scale_range;
                float x1 = tz.x;
                float x2 = tz.x + sz.x;
                grid_path->new_line({x1,y},{x2,y});
            }
            for (size_t i = 0; i < n; i++) {
                float y1 = tz.y;
                float y2 = tz.y + sz.y;
                float x  = tz.x + sz.x * ((float)i) / (float)(n-1);
                grid_path->new_line({x,y1},{x,y2});
            }
        }
    }
};

struct ChartPlot : Visible
{
    float left_space;
    float right_space;
    float top_space;
    float bottom_space;
    color line_color;
    color point_color;
    float point_size;
    bool interpolate;

    std::shared_ptr<ChartData> data;
    ChartAxis *axis;

    Path *line_path;
    std::vector<Circle*> circles;

    ChartPlot() :
        Visible("ChartPlot"),
        left_space(0),
        right_space(0),
        top_space(0),
        bottom_space(0),
        line_color(),
        point_color(),
        point_size(0),
        interpolate(false),
        data(),
        axis(),
        line_path(),
        circles()
    {
        load_properties();
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
    }

    virtual void set_data(std::shared_ptr<ChartData> data) { this->data = data; }
    virtual void set_axis(ChartAxis *axis) { this->axis = axis; }

    virtual void set_left_space(float v) { left_space = v; invalidate(); }
    virtual void set_right_space(float v) { right_space = v; invalidate(); }
    virtual void set_top_space(float v) { top_space = v; invalidate(); }
    virtual void set_bottom_space(float v) { bottom_space = v; invalidate(); }
    virtual void set_line_color(color c) { line_color = c; invalidate(); }
    virtual void set_point_color(color c) { point_color = c; invalidate(); }
    virtual void set_point_size(float s) { point_size = s; invalidate(); }
    virtual void set_interpolate(bool b) { interpolate = b; invalidate(); }

    virtual float get_left_space() { return left_space; }
    virtual float get_right_space() { return right_space; }
    virtual float get_top_space() { return top_space; }
    virtual float get_bottom_space() { return bottom_space; }
    virtual color get_line_color() { return line_color; }
    virtual color get_point_color() { return point_color; }
    virtual float get_point_size() { return point_size; }
    virtual bool get_interpolate() { return interpolate; }

    virtual void init(Canvas *c)
    {
        if (!line_path) {
            line_path = c->new_path(vec2(0), vec2(0));
        }
    }

    virtual void layout(Canvas *c)
    {
        Brush none_brush{BrushNone, {}, {} };
        Brush line_brush{BrushSolid, {}, { line_color }};
        Brush point_brush{BrushSolid, {}, { point_color }};

        vec3 axis_space(left_space + right_space, top_space + bottom_space, 0);

        vec3 size_remaining = assigned_size;
        vec3 half_size = size_remaining / 2.0f;

        vec3 az = size_remaining - p() - b() - m();
        vec3 sz = az - axis_space;
        vec3 bz = vec3(padding[0] + border[0] + margin[0], padding[1] + border[1] + margin[1], 0);
        vec3 tz = bz + vec3(left_space, top_space, 0);

        float scale_min = axis->scale_min;
        float scale_max = axis->scale_max;
        float scale_range = axis->scale_range;

        size_t n = data->num_rows();

        line_path->clear();
        if (interpolate)
        {
            line_path->new_contour();
            for (size_t i = 0; i < n; i++) {
                float v1 = data->get_data(0, i == 0 ? i : i-1);
                float v2 = data->get_data(0, i);
                float v3 = data->get_data(0, i == n-1 ? i : i+1);
                float x1  = tz.x + sz.x * ((float)i-1.0f) / (float)(n-1);
                float x12 = tz.x + sz.x * ((float)i-0.5f) / (float)(n-1);
                float x12c = tz.x + sz.x * ((float)i-0.25f) / (float)(n-1);
                float x2  = tz.x + sz.x * ((float)i)      / (float)(n-1);
                float x23c = tz.x + sz.x * ((float)i+0.25f) / (float)(n-1);
                float x23 = tz.x + sz.x * ((float)i+0.5f) / (float)(n-1);
                float x3  = tz.x + sz.x * ((float)i+1.0f) / (float)(n-1);
                float y1 = tz.y + sz.y * (scale_range - (v1 - scale_min))/scale_range;
                float y2 = tz.y + sz.y * (scale_range - (v2 - scale_min))/scale_range;
                float y3 = tz.y + sz.y * (scale_range - (v3 - scale_min))/scale_range;
                float dy = (y3 - y1) / 2.0f;
                float y12 = (y1 + y2) / 2.0f;
                float y23 = (y2 + y3) / 2.0f;
                float y12c = y2 - 0.25f * dy;
                float y23c = y2 + 0.25f * dy;
                if (i != 0) line_path->new_quadratic_curve({x12,y12},{x12c,y12c},{x2,y2});
                if (i != n-1) line_path->new_quadratic_curve({x2,y2},{x23c,y23c},{x23,y23});
            }
        }
        else
        {
            line_path->new_contour();
            for (size_t i = 1; i < n; i++) {
                float v1 = data->get_data(0, i-1);
                float v2 = data->get_data(0, i);
                float x1 = tz.x + sz.x * (float)(i-1) / (float)(n-1);
                float x2 = tz.x + sz.x * (float)i     / (float)(n-1);
                float y1 = tz.y + sz.y * (scale_range - (v1 - scale_min))/scale_range;
                float y2 = tz.y + sz.y * (scale_range - (v2 - scale_min))/scale_range;
                line_path->new_line({x1,y1},{x2,y2});
            }
        }
        line_path->pos = position;
        line_path->set_offset({0, 0});
        line_path->set_size(size_remaining);
        line_path->set_fill_brush(line_brush);
        line_path->set_stroke_brush(line_brush);
        line_path->set_stroke_width(border[0]);

        {
            for (size_t i = 0; i < n; i++) {
                float value = data->get_data(0,i);
                float x = tz.x + sz.x * (float)i     / (float)(n-1);
                float y = tz.y + sz.y * (scale_range - (value - scale_min))/scale_range;
                vec3 pos = position - half_size + vec3(x,y,0);
                Circle *circle = nullptr;
                if (circles.size() < i + 1) {
                    circle = c->new_circle(vec2(0), 0);
                    circles.push_back(circle);
                } else {
                    circle = circles[i];
                }
                circle->set_position(pos);
                circle->set_radius(point_size);
                circle->set_fill_brush(point_brush);
                circle->set_stroke_brush(none_brush);
                circle->set_stroke_width(0);
                circle->set_visible(true);
            }
            for (size_t j = n; j < circles.size(); j++) {
                circles[j]->set_visible(false);
            }
        }
    }
};

struct Chart : Visible
{
    float left_space;
    float right_space;
    float top_space;
    float bottom_space;
    color grid_color;
    color tick_color;
    color line_color;
    color point_color;
    float point_size;
    bool interpolate;

    std::shared_ptr<ChartData> data;

    Rectangle *rect;
    Path *line_path;
    std::vector<Circle*> circles;

    ChartAxis left_axis;
    ChartAxis bottom_axis;
    ChartGrid grid;
    std::vector<std::unique_ptr<ChartPlot>> plots;

    Chart() :
        Visible("Chart"),
        rect(nullptr),
        line_path(nullptr),
        circles(),
        left_axis(),
        bottom_axis()
    {
        load_properties();
    }

    virtual void load_properties()
    {
        Visible::load_properties();
        Defaults *d = get_defaults();
        set_left_space(d->get_float(class_name, "axis-left-space", 60.0f));
        set_right_space(d->get_float(class_name, "axis-right-space", 10.0f));
        set_top_space(d->get_float(class_name, "axis-top-space", 0.0f));
        set_bottom_space(d->get_float(class_name, "axis-bottom-space", 30.0f));
        set_grid_color(d->get_color(class_name, "grid-color", color(1,1,1,1)));
        set_tick_color(d->get_color(class_name, "tick-color", color(1,1,1,1)));
        set_line_color(d->get_color(class_name, "line-color", color(1,1,1,1)));
        set_point_color(d->get_color(class_name, "point-color", color(1,1,1,1)));
        set_point_size(d->get_float(class_name, "point-size", 1));
        set_interpolate(d->get_boolean(class_name, "interpolate", true));
    }

    virtual void set_data(std::shared_ptr<ChartData> data) { this->data = data; invalidate(); }

    virtual void set_left_space(float v) { left_space = v; invalidate(); }
    virtual void set_right_space(float v) { right_space = v; invalidate(); }
    virtual void set_top_space(float v) { top_space = v; invalidate(); }
    virtual void set_bottom_space(float v) { bottom_space = v; invalidate(); }
    virtual void set_grid_color(color c) { grid_color = c; invalidate(); }
    virtual void set_tick_color(color c) { tick_color = c; invalidate(); }
    virtual void set_line_color(color c) { line_color = c; invalidate(); }
    virtual void set_point_color(color c) { point_color = c; invalidate(); }
    virtual void set_point_size(float s) { point_size = s; invalidate(); }
    virtual void set_interpolate(bool b) { interpolate = b; invalidate(); }

    virtual float get_left_space() { return left_space; }
    virtual float get_right_space() { return right_space; }
    virtual float get_top_space() { return top_space; }
    virtual float get_bottom_space() { return bottom_space; }
    virtual color get_grid_color() { return grid_color; }
    virtual color get_tick_color() { return tick_color; }
    virtual color get_line_color() { return line_color; }
    virtual color get_point_color() { return point_color; }
    virtual float get_point_size() { return point_size; }
    virtual bool get_interpolate() { return interpolate; }

    virtual Sizing calc_size()
    {
        vec3 axis_space = vec3(left_space + right_space,
            top_space + bottom_space, 0) + m() + b() + p();
        vec3 min_size = vec3( std::max(minimum_size.x,axis_space.x),
                              std::max(minimum_size.y,axis_space.y),
                              std::max(minimum_size.z,axis_space.z) );
        vec3 pref_size = vec3( std::max(preferred_size.x,min_size.x),
                               std::max(preferred_size.y,min_size.y),
                               std::max(preferred_size.z,min_size.z) );
        Sizing s = { min_size, pref_size };
        if (debug) {
            Debug("%s minimum=(%f,%f), preferred=(%f,%f)\n",
                __PRETTY_FUNCTION__,
                s.minimum.x, s.minimum.y,
                s.preferred.x, s.preferred.y);
        }
        return (last_sizing = s);
    }

    virtual void init(Canvas *c)
    {
        if (!rect) {
            rect = c->new_rounded_rectangle(vec2(0), vec2(0), 0.0f);
        }
        if (!line_path) {
            line_path = c->new_path(vec2(0), vec2(0));
        }
        if (plots.size() == 0) {
            plots.push_back(std::make_unique<ChartPlot>());
        }
    }

    virtual void layout(Canvas *c)
    {
        if (valid) return;

        Brush fill_brush{BrushSolid, {}, { fill_color }};
        Brush stroke_brush{BrushSolid, {}, { stroke_color }};

        vec3 axis_space(left_space + right_space, top_space + bottom_space, 0);

        vec3 size_remaining = assigned_size - m();
        vec3 half_size = size_remaining / 2.0f;

        rect->pos = position;
        rect->set_origin(half_size);
        rect->set_halfsize(half_size);
        rect->set_radius(border_radius);
        rect->set_visible(visible);
        rect->set_fill_brush(fill_brush);
        rect->set_stroke_brush(stroke_brush);
        rect->set_stroke_width(border[0]);

        bottom_axis.set_data(data);
        bottom_axis.grant_size(assigned_size);
        bottom_axis.set_position(get_position());
        bottom_axis.set_padding(get_padding());
        bottom_axis.set_border(get_border());
        bottom_axis.set_margin(get_margin());
        bottom_axis.set_precision(0);
        bottom_axis.set_location(bottom);
        bottom_axis.set_tick_color(get_tick_color());
        bottom_axis.set_left_space(get_left_space());
        bottom_axis.set_right_space(get_right_space());
        bottom_axis.set_top_space(get_top_space());
        bottom_axis.set_bottom_space(get_bottom_space());
        bottom_axis.calc_scale(-1, font_size * 1.5f, size_remaining.x);
        bottom_axis.init(c);
        bottom_axis.layout(c);

        left_axis.set_data(data);
        left_axis.grant_size(assigned_size);
        left_axis.set_position(get_position());
        left_axis.set_padding(get_padding());
        left_axis.set_border(get_border());
        left_axis.set_margin(get_margin());
        left_axis.set_precision(1);
        left_axis.set_location(left);
        left_axis.set_tick_color(get_tick_color());
        left_axis.set_left_space(get_left_space());
        left_axis.set_right_space(get_right_space());
        left_axis.set_top_space(get_top_space());
        left_axis.set_bottom_space(get_bottom_space());
        left_axis.calc_scale(0, font_size * 1.5f, size_remaining.y);
        left_axis.init(c);
        left_axis.layout(c);

        grid.set_data(data);
        grid.grant_size(assigned_size);
        grid.set_position(get_position());
        grid.set_padding(get_padding());
        grid.set_border(get_border());
        grid.set_margin(get_margin());
        grid.set_position(get_position());
        grid.set_left_space(get_left_space());
        grid.set_right_space(get_right_space());
        grid.set_top_space(get_top_space());
        grid.set_bottom_space(get_bottom_space());
        grid.set_grid_color(get_grid_color());
        grid.set_axis(&left_axis);
        grid.init(c);
        grid.layout(c);

        for (auto &p : plots) {
            p->grant_size(assigned_size);
            p->set_position(get_position());
            p->set_padding(get_padding());
            p->set_border(get_border());
            p->set_margin(get_margin());
            p->set_position(get_position());
            p->set_left_space(get_left_space());
            p->set_right_space(get_right_space());
            p->set_top_space(get_top_space());
            p->set_bottom_space(get_bottom_space());
            p->set_line_color(get_line_color());
            p->set_point_color(get_point_color());
            p->set_point_size(get_point_size());
            p->set_interpolate(get_interpolate());
            p->set_data(data);
            p->set_axis(&left_axis);
            p->init(c);
            p->layout(c);
        }

        valid = true;
    }
};

}