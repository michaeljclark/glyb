/*
 * bin_packer - 2D bin packer implementing the MAXRECTS-BSSF algorithm
 */

#include <vector>
#include <map>

#include "binpack.h"


/* helper, that adds non-empty rectangles */

static void add_rect(std::vector<bin_rect> &il, bin_rect r)
{
    if (r.area() != 0) {
        il.push_back(r);
    }
}

/*
 * We perform 3 tests on each dimension: less_equal, inside, greater_equal
 *
 * These are the 9 combinations for the x-dimension:
 *
 * oo |    |     a_x_le,b_x_le
 *  oo|    |     a_x_le,b_x_le,b_x_in
 *   o|o   |     a_x_le,b_x_in
 *    |oo  |     a_x_le,a_x_in,b_x_in
 *    | oo |     a_x_in,b_x_in
 *    |  oo|     a_x_in,b_x_in,b_x_ge
 *    |   o|o    a_x_in,b_x_ge
 *    |    |oo   a_x_ge,b_x_ge,a_x_in
 *    |    | oo  a_x_ge,b_x_ge
 * ooo|oooo|ooo  a_x_le,b_x_ge
 */

std::vector<bin_rect> bin_rect::intersect_subset(bin_rect o)
{
    std::vector<bin_rect> il;

    bool a_x_le = o.a.x <= a.x && o.a.x <= b.x;
    bool a_x_ge = o.a.x >= a.x && o.a.x >= b.x;
    bool a_x_in = o.a.x >= a.x && o.a.x <= b.x;

    bool b_x_le = o.b.x <= a.x && o.b.x <= b.x;
    bool b_x_ge = o.b.x >= a.x && o.b.x >= b.x;
    bool b_x_in = o.b.x >= a.x && o.b.x <= b.x;

    bool a_y_le = o.a.y <= a.y && o.a.y <= b.y;
    bool a_y_ge = o.a.y >= a.y && o.a.y >= b.y;
    bool a_y_in = o.a.y >= a.y && o.a.y <= b.y;

    bool b_y_le = o.b.y <= a.y && o.b.y <= b.y;
    bool b_y_ge = o.b.y >= a.y && o.b.y >= b.y;
    bool b_y_in = o.b.y >= a.y && o.b.y <= b.y;

    /*
     * Outside cases:
     *
     *                o ooo
     *   /---\   /---\o  /---\   /---\
     *   |   |   |   |o  |   |   |   |
     *   |   |   |   |   |   |   |   | 
     *  o|   |   |   |   |   |   |   |
     *  o\---/   \---/   \---/   \---/
     *  o                           ooo
     */

    if ((a_x_le && b_x_le) || (a_x_ge && b_x_ge) ||
        (a_y_le && b_y_le) || (a_y_ge && b_y_ge))
    {
        // do nothing
    }

    /*
     * Fully inside case (0-axis crossing):
     *
     *
     *   /---\
     *   |   |
     *   | o |
     *   |   |
     *   \---/
     *
     */

    else if (a_x_in && b_x_in && a_y_in && b_y_in)
    {
        add_rect(il, o);
    }

    /*
     * Overlap cases (1-axis crossing):
     *
     *                     o
     *   /---\   /---\   /-o-\   /---\
     *   |   |   |   |   | o |   |   |
     *   |   |  ooo  |   |   |   |  ooo
     *   | o |   |   |   |   |   |   |
     *   \ o-/   \---/   \---/   \---/
     *     o
     */

    else if (a_x_in && b_x_in && a_y_in && b_y_ge)
    {
        add_rect(il, { { o.a.x, o.a.y }, { o.b.x, b.y   } });
    }
    else if (a_x_le && b_x_in && a_y_in && b_y_in)
    {
        add_rect(il, { { a.x,   o.a.y }, { o.b.x, o.b.y } });
    }
    else if (a_x_in && b_x_in && a_y_le && b_y_in)
    {
        add_rect(il, { { o.a.x, a.y   }, { o.b.x, o.b.y } });
    }
    else if (a_x_in && b_x_ge && a_y_in && b_y_in)
    {
        add_rect(il, { { o.a.x, o.a.y }, { b.x,   o.b.y } });
    }

    /*
     * Overlap cases (2-axis crossing):
     *
     *          ooo         ooo
     *   /---\  ooo--\   /--ooo  /---\
     *   |   |  ooo  |   |  ooo  |   |
     *   |   |   |   |   |   |   |   |
     *  ooo  |   |   |   |   |   |  ooo
     *  ooo--/   \---/   \---/   \--ooo
     *  ooo                         ooo
     */

    else if (a_x_le && b_x_in && a_y_in && b_y_ge)
    {
        add_rect(il, { { a.x,   o.a.y }, { o.b.x, b.y   } });
    }
    else if (a_x_le && b_x_in && a_y_le && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { o.b.x, o.b.y } });
    }
    else if (a_x_in && b_x_ge && a_y_le && b_y_in)
    {
        add_rect(il, { { o.a.x, a.y   }, { b.x,   o.b.y } });
    }
    else if (a_x_in && b_x_ge && a_y_in && b_y_ge)
    {
        add_rect(il, { { o.a.x, o.a.y }, { b.x,   b.y   } });
    }

    /*
     * Overlap cases (3-axis crossing):
     *
     *  ooo     ooooooo     ooo
     *  ooo--\  ooooooo  /--ooo  /---\
     *  ooo  |  ooooooo  |  ooo  |   |
     *  ooo  |   |   |   |  ooo  |   |
     *  ooo  |   |   |   |  ooo ooooooo
     *  ooo--/   \---/   \--ooo ooooooo
     *  ooo                 ooo ooooooo
     */

    else if (a_x_le && b_x_in && a_y_le && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { o.b.x, b.y   } });
    }
    else if (a_x_le && b_x_ge && a_y_le && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.b.y } });
    }
    else if (a_x_in && b_x_ge && a_y_le && b_y_ge)
    {
        add_rect(il, { { o.a.x, a.y   }, { b.x,   b.y   } });
    }
    else if (a_x_le && b_x_ge && a_y_in && b_y_ge) 
    {
        add_rect(il, { { a.x,   o.a.y }, { b.x,   b.y   } });
    }

    /*
     * Overlap cases (3-axis crossing):
     *
     *     o
     *   /-o-\   /---\
     *   | o |   |   |
     *   | o |  ooooooo
     *   | o |   |   |
     *   \-o-/   \---/
     *     o
     */

    else if (a_x_in && b_x_in && a_y_le && b_y_ge)
    {
        add_rect(il, { { o.a.x, a.y   }, { o.b.x, b.y   } });
    }
    else if (a_x_le && b_x_ge && a_y_in && b_y_in)
    {
        add_rect(il, { { a.x,   o.a.y }, { b.x,   o.b.y } });
    }

    /*
     * Fully surrounded case (4-axis crossing):
     *
     *  ooooooo
     *  o/---\o
     *  o|ooo|o
     *  o|ooo|o
     *  o|ooo|o
     *  o\---/o
     *  ooooooo
     */
    else if (a_x_le && b_x_ge && a_y_le && b_y_ge) {
        add_rect(il, *this);
    }

    return il;
}

std::vector<bin_rect> bin_rect::disjoint_subset(bin_rect o)
{
    std::vector<bin_rect> il;

    /*
     * Calculate the disjoint set of maximal non intersecting rectangles.
     * These rectangles are for indexing free space in the MaxRect algo,
     * so the returned list of rectangles has overlaps between each axis.
     */

    bool a_x_le = o.a.x <= a.x && o.a.x <= b.x;
    bool a_x_ge = o.a.x >= a.x && o.a.x >= b.x;
    bool a_x_in = o.a.x >= a.x && o.a.x <= b.x;

    bool b_x_le = o.b.x <= a.x && o.b.x <= b.x;
    bool b_x_ge = o.b.x >= a.x && o.b.x >= b.x;
    bool b_x_in = o.b.x >= a.x && o.b.x <= b.x;

    bool a_y_le = o.a.y <= a.y && o.a.y <= b.y;
    bool a_y_ge = o.a.y >= a.y && o.a.y >= b.y;
    bool a_y_in = o.a.y >= a.y && o.a.y <= b.y;

    bool b_y_le = o.b.y <= a.y && o.b.y <= b.y;
    bool b_y_ge = o.b.y >= a.y && o.b.y >= b.y;
    bool b_y_in = o.b.y >= a.y && o.b.y <= b.y;

    /*
     * Outside cases:
     *
     *                o ooo
     *   /---\   /---\o  /---\   /---\
     *   |   |   |   |o  |   |   |   |
     *   |   |   |   |   |   |   |   | 
     *  o|   |   |   |   |   |   |   |
     *  o\---/   \---/   \---/   \---/
     *  o                           ooo
     */

    if ((a_x_le && b_x_le) || (a_x_ge && b_x_ge) ||
        (a_y_le && b_y_le) || (a_y_ge && b_y_ge))
    {
        add_rect(il, *this);
    }

    /*
     * Fully inside case (0-axis crossing):
     *
     *
     *   /---\
     *   |   |
     *   | o |
     *   |   |
     *   \---/
     *
     */

    else if (a_x_in && b_x_in && a_y_in && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
    }

    /*
     * Overlap cases (1-axis crossing):
     *
     *                     o
     *   /---\   /---\   /-o-\   /---\
     *   |   |   |   |   | o |   |   |
     *   |   |  ooo  |   |   |   |  ooo
     *   | o |   |   |   |   |   |   |
     *   \ o-/   \---/   \---/   \---/
     *     o
     */

    else if (a_x_in && b_x_in && a_y_in && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
    }
    else if (a_x_le && b_x_in && a_y_in && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
    }
    else if (a_x_in && b_x_in && a_y_le && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
    }
    else if (a_x_in && b_x_ge && a_y_in && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
    }

    /*
     * Overlap cases (2-axis crossing):
     *
     *          ooo         ooo
     *   /---\  ooo--\   /--ooo  /---\
     *   |   |  ooo  |   |  ooo  |   |
     *   |   |   |   |   |   |   |   |
     *  ooo  |   |   |   |   |   |  ooo
     *  ooo--/   \---/   \---/   \--ooo
     *  ooo                         ooo
     */

    else if (a_x_le && b_x_in && a_y_in && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
    }
    else if (a_x_le && b_x_in && a_y_le && b_y_in)
    {
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
    }
    else if (a_x_in && b_x_ge && a_y_le && b_y_in)
    {
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
    }
    else if (a_x_in && b_x_ge && a_y_in && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
    }

    /*
     * Overlap cases (3-axis crossing):
     *
     *  ooo     ooooooo     ooo
     *  ooo--\  ooooooo  /--ooo  /---\
     *  ooo  |  ooooooo  |  ooo  |   |
     *  ooo  |   |   |   |  ooo  |   |
     *  ooo  |   |   |   |  ooo ooooooo
     *  ooo--/   \---/   \--ooo ooooooo
     *  ooo                 ooo ooooooo
     */

    else if (a_x_le && b_x_in && a_y_le && b_y_ge)
    {
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
    }
    else if (a_x_le && b_x_ge && a_y_le && b_y_in)
    {
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
    }
    else if (a_x_in && b_x_ge && a_y_le && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
    }
    else if (a_x_le && b_x_ge && a_y_in && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
    }

    /*
     * Overlap cases (3-axis crossing):
     *
     *     o
     *   /-o-\   /---\
     *   | o |   |   |
     *   | o |  ooooooo
     *   | o |   |   |
     *   \-o-/   \---/
     *     o
     */

    else if (a_x_in && b_x_in && a_y_le && b_y_ge)
    {
        add_rect(il, { { a.x,   a.y   }, { o.a.x, b.y   } });
        add_rect(il, { { o.b.x, a.y   }, { b.x,   b.y   } });
    }
    else if (a_x_le && b_x_ge && a_y_in && b_y_in)
    {
        add_rect(il, { { a.x,   a.y   }, { b.x,   o.a.y } });
        add_rect(il, { { a.x,   o.b.y }, { b.x,   b.y   } });
    }

    /*
     * Fully surrounded case (4-axis crossing):
     *
     *  ooooooo
     *  o/---\o
     *  o|ooo|o
     *  o|ooo|o
     *  o|ooo|o
     *  o\---/o
     *  ooooooo
     */

    else if (a_x_le && b_x_ge && a_y_le && b_y_ge) {
        // do nothing
    }

    return il;
}

/*
 * bin_packer
 */

bin_packer::bin_packer(bin_point sz) : total(bin_rect(bin_point(),sz))
{
    reset();
}

void bin_packer::reset()
{
    alloc_map.clear();
    free_list.clear();
    free_list.push_back(total);
}

void bin_packer::set_bin_size(bin_point sz)
{
    total = bin_rect(bin_point(),sz);
    reset();
}

void bin_packer::split_intersecting_nodes(bin_rect b)
{
    /*  split nodes that overlap found rectangle */
    for (size_t i = 0; i < free_list.size();) {
        bin_rect c = free_list[i];
        if (c.intersects(b)) {
            std::vector<bin_rect> l = c.disjoint_subset(b);
            free_list.erase(free_list.begin() + i);
            std::copy(l.begin(), l.end(), std::back_inserter(free_list));
        } else {
            i++;
        }
    }    
}

void bin_packer::remove_containing_nodes()
{
    /* remove nodes contained by other nodes */
    size_t i = 0;
    while(i < free_list.size()) {
        size_t j = 0;
        while(j < free_list.size()) {
            if (i != j && free_list[i].contains(free_list[j])) {
                free_list.erase(free_list.begin() + j);
                if (i > j) {
                    i--; /* adjust outer loop iterator */
                }
            } else {
                j++;
            }
        }
        i++;
    }
}

std::pair<size_t,bin_rect> bin_packer::scan_bins(bin_point sz)
{
    /* loop through free list, and find shortest side */
    int best_ssz = -1;
    size_t best_idx = -1;
    bin_rect b(bin_point(0,0),bin_point(0,0));
    std::vector<bin_rect> best_list;
    for (size_t i = 0; i < free_list.size(); i++) {
        bin_rect c = free_list[i], d(c.a, c.a + sz);
        if (c.width() < sz.x || c.height() < sz.y) continue;
        std::vector<bin_rect> l = c.disjoint_subset(d);
        if (l.size() == 0) continue;
        int ssz = (std::min)(c.width() - sz.x, c.height() - sz.y);
        if (best_idx == size_t(-1) || ssz < best_ssz) {
            best_ssz = ssz;
            best_idx = i;
            best_list = l;
            b = d;
        }
    }
    return std::pair<size_t,bin_rect>(best_idx,b);
}

std::pair<bool,bin_rect> bin_packer::find_region(int idx, bin_point sz)
{
    /* find best fit from free list */
    auto r = scan_bins(sz);
    if (r.first == size_t(-1)) return std::pair<bool,bin_rect>(false,bin_rect());

    /* insert found rectangle into the index */
    alloc_map[idx] = r.second;

    /* split nodes that overlap found rectangle */
    split_intersecting_nodes(r.second);

    /* remove nodes contained by other nodes */
    remove_containing_nodes();

    return std::pair<bool,bin_rect>(true,r.second);
}

void bin_packer::dump()
{
    for (size_t i = 0; i < free_list.size(); i++) {
        bin_rect c = free_list[i];
        printf("[%zu] - (%d,%d - %d,%d) [%d,%d]\n",
            i, c.a.x, c.a.y, c.b.x, c.b.y, c.b.x-c.a.x, c.b.y-c.a.y);
    }
    for (auto i : alloc_map) {
        bin_rect c = i.second;
        printf("<%zu> - (%d,%d - %d,%d) [%d,%d]\n",
            i.first, c.a.x, c.a.y, c.b.x, c.b.y, c.b.x-c.a.x, c.b.y-c.a.y);
    }
}

size_t bin_packer::verify()
{
    /* verify allocated rectangles do not intersect free rectangles */
    size_t conflicts = 0;
    for (auto i : alloc_map) {
        bin_rect c = i.second;
        for (size_t j = 0; j < free_list.size(); j++) {
            bin_rect d = free_list[j];
            if (c.intersects(d)) {
                printf("free rect [%zu/%zu] (%d,%d - %d,%d) "
                    "intersects [%zu/%zu] (%d,%d - %d,%d)\n",
                    j, free_list.size(), c.a.x, c.a.y, c.b.x, c.b.y,
                    i.first, alloc_map.size(), d.a.x, d.a.y, d.b.x, d.b.y);
                conflicts++;
            }
        }
    }
    /* verify that allocated rectangles do not intersect with each other */
    for (auto i : alloc_map) {
        bin_rect c = i.second;
        for (auto j : alloc_map) {
            bin_rect d = j.second;
            if (i.first == j.first) continue;
            if (c.intersects(d)) {
                printf("alloc rect [%zu/%zu] (%d,%d - %d,%d) "
                    "intersects [%zu/%zu] (%d,%d - %d,%d)\n",
                    i.first, alloc_map.size(), c.a.x, c.a.y, c.b.x, c.b.y,
                    j.first, alloc_map.size(), d.a.x, d.a.y, d.b.x, d.b.y);
                conflicts++;
            }
        }
    }
    return conflicts;
}
