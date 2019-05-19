/*
 * bin_packer - 2D bin packer implementing the MAXRECTS-BSSF algorithm
 */

#pragma once

struct bin_point
{
    int x, y;

    bin_point() : x(0), y(0) {}
    bin_point(int s) : x(s), y(s) {}
    bin_point(int x, int y) : x(x), y(y) {}
    bin_point(const bin_point &o) : x(o.x), y(o.y) {}

    bool operator==(const bin_point &o) { return x == o.x && y == o.y; }
    bool operator!=(const bin_point &o) { return !(*this == o); }

    bin_point& operator+=(const bin_point &o) { x += o.x; y += o.y; return *this; }
    bin_point& operator-=(const bin_point &o) { x -= o.x; y -= o.y; return *this; }
    
    bin_point operator+(const bin_point &o) const { return bin_point(x + o.x, y + o.y); }
    bin_point operator-(const bin_point &o) const { return bin_point(x - o.x, y - o.y); }
        
    bin_point operator+(const int i) const { return bin_point(x + i, y + i); }
    bin_point operator-(const int i) const { return bin_point(x - i, y - i); }

    bool operator==(const bin_point &o) const { return x == o.x && y == o.y; }
    bool operator<(const bin_point &o) const { return x < o.x || (x == o.x && y < o.y); }

    /* axiomatically define other comparisons in terms of "equals" and "less than" */
    bool operator!=(const bin_point &o) const { return !(*this == o); }
    bool operator<=(const bin_point &o) const { return *this < o || *this == o; }
    bool operator>(const bin_point &o) const { return !(*this <= o); }
    bool operator>=(const bin_point &o) const { return !(*this < o) || *this == o; }
};

struct bin_rect
{
    bin_point a, b;

    inline bin_rect() : a(0), b(0) {}
    inline bin_rect(bin_point a, bin_point b) : a(a), b(b) { if (a > b) std::swap(a, b); }
    inline bin_rect(const bin_rect &o) : a(o.a), b(o.b) {}

    inline int width() { return b.x - a.x; }
    inline int height() { return b.y - a.y; }
    inline int area() { return (b.x - a.x) * (b.y - a.y); }
    inline bin_point size() { return bin_point(b.x - a.x, b.y - a.y); }

    inline bool operator==(const bin_rect &o) { return a == o.a && b == o.b; }
    inline bool operator!=(const bin_rect &o) { return !(*this == o); }

    inline bool contains(bin_rect o)
    {
        return ( a.x <= o.a.x && b.x >= o.b.x && a.y <= o.a.y && b.y >= o.b.y );
    }

    inline bool intersects(bin_rect o)
    {
        return ( a.x < o.b.x && b.x > o.a.x && a.y < o.b.y && b.y > o.a.y );
    }

    std::vector<bin_rect> intersect_subset(bin_rect o);
    std::vector<bin_rect> disjoint_subset(bin_rect o);
};

struct bin_packer
{
    bin_rect total;
    std::vector<bin_rect> free_list;
    std::map<size_t,bin_rect> alloc_map;
    size_t contained_min;

    bin_packer() = delete;
    bin_packer(bin_point sz);

    void reset();
    void set_bin_size(bin_point sz);
    void split_intersecting_nodes(bin_rect b);
    void remove_containing_nodes();
    std::pair<size_t,bin_rect>  scan_bins(bin_point sz);
    std::pair<bool,bin_rect> find_region(int idx, bin_point sz);
    size_t verify();
    void dump();
};