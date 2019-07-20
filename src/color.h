// See LICENSE for license details.

#pragma once

/* color */

class color
{
public:
    float r, g, b, a;
    
    color();
    color(unsigned rgba);
    color(float r, float g, float b, float a);
    color(const color &o);
    color(std::string hex);
        
    std::string to_string() const;
    unsigned int rgba32() const;

    color saturate(float f) const;
    color brighten(float f) const;

    bool operator==(const color &o);
    bool operator!=(const color &o);
};

inline color::color() : r(0), g(0), b(0), a(0) {}

inline color::color(unsigned rgba) :
	r(((rgba >> 24) & 0x0ff) / 256.0f),
	g(((rgba >> 16) & 0xff) / 256.0f),
	b(((rgba >> 8) & 0xff) / 256.0f),
	a((rgba & 0xff) / 256.0f) {}

inline color::color(float r, float g, float b, float a) :
	r(r), g(g), b(b), a(a) {}

inline color::color(const color &o) :
	r(o.r), g(o.g), b(o.b), a(o.a) {}

inline color::color(std::string hex)
{
    if (!hex.length()) {
        r = g = b = a = 1.0f;
        return;
    }
    if (hex[0] == '#') {
        hex = hex.substr(1);
    }
    unsigned rgba = 0;
    sscanf(hex.c_str(), "%x", &rgba);
    if (hex.length() == 8) {
        r = ((rgba >> 24) & 0xff) / 255.0f;
        g = ((rgba >> 16) & 0xff) / 255.0f;
        b = ((rgba >> 8)  & 0xff) / 255.0f;
        a = (rgba & 0xff) / 255.0f;
    } else if (hex.length() == 6) {
        r = ((rgba >> 16) & 0xff) / 255.0f;
        g = ((rgba >> 8)  & 0xff) / 255.0f;
        b = (rgba & 0xff) / 255.0f;
        a = 1.0f;
    } else if (hex.length() == 2) {
        r = g = b = (rgba & 0xff) / 255.0f;
        a = 1.0f;
    } else {
        r = g = b = a = 1.0f;
    }
}

inline char hexdigit(int d) { return (d < 10) ? '0' + d : 'A' + (d-10); }

inline std::string color::to_string() const
{
    char buf[10];
    union { unsigned rgba; unsigned char c[4]; } tou32 = { rgba32() };
    sprintf(buf, "#%c%c%c%c%c%c%c%c",
            hexdigit(tou32.c[0] / 16), hexdigit(tou32.c[0] % 16),
            hexdigit(tou32.c[1] / 16), hexdigit(tou32.c[1] % 16),
            hexdigit(tou32.c[2] / 16), hexdigit(tou32.c[2] % 16),
            hexdigit(tou32.c[3] / 16), hexdigit(tou32.c[3] % 16));
    return std::string(buf);
}

inline unsigned int color::rgba32() const
{
    union { unsigned char c[4]; unsigned rgba; } tou32 = {
        (unsigned char)std::max(0, std::min(255, (int)(r * 255.0f))),
        (unsigned char)std::max(0, std::min(255, (int)(g * 255.0f))),
        (unsigned char)std::max(0, std::min(255, (int)(b * 255.0f))),
        (unsigned char)std::max(0, std::min(255, (int)(a * 255.0f)))
    };
    return tou32.rgba;
}

inline color color::saturate(float f) const
{
    float l = 0.299f * r + 0.587f * g + 0.114f * b; // CCIR601 perceived luminance
    return color(f * r + (1.0f-f) * l, f * g + (1.0f-f) * l, f * b + (1.0f-f) * l, a);
}

inline color color::brighten(float f) const
{
    return color(r * f > 1.0f ? 1.0f : r * f,
                 g * f > 1.0f ? 1.0f : g * f,
                 b * f > 1.0f ? 1.0f : b * f, a);
}

inline bool color::operator==(const color &o)
{
    return r == o.r && g == o.g && b == o.b && a == o.a;
}

inline bool color::operator!=(const color &o)
{
    return !(*this == o);
}
