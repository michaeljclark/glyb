#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <string>

#include "glm/glm.hpp"
#include "geometry.h"

const struct { intersect_2d key; const char *name; } map[] = {

    { intersect_2d::none,              "none" },

    { intersect_2d::inner,             "inner" },
    { intersect_2d::north,             "north" },
    { intersect_2d::east,              "east" },
    { intersect_2d::south,             "south" },
    { intersect_2d::west,              "west" },

    { intersect_2d::north_east,        "north_east" },
    { intersect_2d::south_east,        "south_east" },
    { intersect_2d::south_west,        "south_west" },
    { intersect_2d::north_west,        "north_west" },

    { intersect_2d::north_south,       "north_south" },
    { intersect_2d::east_west,         "east_west" },

    { intersect_2d::left,              "left" },
    { intersect_2d::top,               "top" },
    { intersect_2d::bottom,            "bottom" },
    { intersect_2d::right,             "right" },
    { intersect_2d::surrounded,        "surrounded" },

    { intersect_2d::inner_north,       "inner_north" },
    { intersect_2d::inner_north_east,  "inner_north_east" },
    { intersect_2d::inner_east,        "inner_east" },
    { intersect_2d::inner_south_east,  "inner_south_east" },
    { intersect_2d::inner_south,       "inner_south" },
    { intersect_2d::inner_south_west,  "inner_south_west" },
    { intersect_2d::inner_west,        "inner_west" },
    { intersect_2d::inner_north_west,  "inner_north_west" },
    { intersect_2d::inner_north_south, "inner_north_south" },
    { intersect_2d::inner_east_west,   "inner_east_west" },
    { intersect_2d::inner_left,        "inner_left" },
    { intersect_2d::inner_top,         "inner_top" },
    { intersect_2d::inner_bottom,      "inner_bottom" },
    { intersect_2d::inner_right,       "inner_right" },
    { intersect_2d::inner_surrounded,  "inner_surrounded" },

    { intersect_2d::none, nullptr },
};

std::string to_string(intersect_2d n)
{
    for (auto *p = map; p->name; p++) {
        if (p->key == n) return p->name;
    }
    return "unknown";
}

std::string to_string(rect_2d r)
{
    char buf[64];
    snprintf(buf, sizeof(buf),
        "{{%4.1f,%4.1f },{%4.1f,%4.1f }}",
        r.p0.x, r.p0.y, r.p1.x, r.p1.y);
    return std::string(buf);
}

void test(rect_2d a, rect_2d b, intersect_2d m)
{
    intersect_2d n = intersect(a, b);
    printf("intersect(a: %s, b: %s) -> %-20s # %4s\n",
        to_string(a).c_str(), to_string(b).c_str(),
        to_string(m).c_str(), ((n&m) == m) ? "PASS" : "FAIL"
    );
    assert((n&m) == m);
}

int main(int argc, char **argv)
{
    test({{3,3},{6,6}}, {{2,2},{7,7}}, intersect_2d::inner);
    test({{4,1},{5,2}}, {{2,2},{7,7}}, intersect_2d::inner_north);
    test({{6,4},{8,5}}, {{2,2},{7,7}}, intersect_2d::inner_east);
    test({{4,6},{5,8}}, {{2,2},{7,7}}, intersect_2d::inner_south);
    test({{1,4},{3,5}}, {{2,2},{7,7}}, intersect_2d::inner_west);
    test({{6,1},{8,3}}, {{2,2},{7,7}}, intersect_2d::inner_north_east);
    test({{1,1},{3,3}}, {{2,2},{7,7}}, intersect_2d::inner_north_west);
    test({{6,6},{8,8}}, {{2,2},{7,7}}, intersect_2d::inner_south_east);
    test({{1,6},{3,8}}, {{2,2},{7,7}}, intersect_2d::inner_south_west);
    test({{4,1},{5,8}}, {{2,2},{7,7}}, intersect_2d::inner_north_south);
    test({{1,4},{8,5}}, {{2,2},{7,7}}, intersect_2d::inner_east_west);
    test({{1,1},{4,8}}, {{2,2},{7,7}}, intersect_2d::inner_left);
    test({{1,1},{8,3}}, {{2,2},{7,7}}, intersect_2d::inner_top);
    test({{6,1},{8,8}}, {{2,2},{7,7}}, intersect_2d::inner_right);
    test({{1,6},{8,8}}, {{2,2},{7,7}}, intersect_2d::inner_bottom);
    test({{1,1},{8,8}}, {{2,2},{7,7}}, intersect_2d::inner_surrounded);
    test({{4,0},{5,1}}, {{2,2},{7,7}}, intersect_2d::north);
    test({{8,4},{9,5}}, {{2,2},{7,7}}, intersect_2d::east);
    test({{4,8},{5,9}}, {{2,2},{7,7}}, intersect_2d::south);
    test({{0,4},{1,5}}, {{2,2},{7,7}}, intersect_2d::west);
    test({{8,0},{9,1}}, {{2,2},{7,7}}, intersect_2d::north_east);
    test({{0,0},{1,1}}, {{2,2},{7,7}}, intersect_2d::north_west);
    test({{8,8},{9,9}}, {{2,2},{7,7}}, intersect_2d::south_east);
    test({{0,8},{1,9}}, {{2,2},{7,7}}, intersect_2d::south_west);

    return 0;
}
