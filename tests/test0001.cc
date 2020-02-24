#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include <vector>
#include <map>

#include "binpack.h"

void test(bin_rect r1, bin_rect r2)
{
    auto l1 = r1.intersect_subset(r2);
    auto l2 = r1.disjoint_subset(r2);
    printf("A = (%d,%d - %d,%d), B = (%d,%d - %d,%d):\n",
        r1.a.x, r1.a.y, r1.b.x, r1.b.y, r2.a.x, r2.a.y, r2.b.x, r2.b.y);
    printf("\tA ∩ B:\n");
    for (auto r : l1) {
        printf("\t\t(%d,%d - %d,%d)\n", r.a.x, r.a.y, r.b.x, r.b.y);
    }
    printf("\tA - A ∩ B:\n");
    for (auto r : l2) {
        printf("\t\t(%d,%d - %d,%d)\n", r.a.x, r.a.y, r.b.x, r.b.y);
    }
}

void run_tests()
{
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
    test(bin_rect({1,1},{4,4}), bin_rect({2,2},{3,3}));

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
    test(bin_rect({2,2},{5,5}), bin_rect({3,3},{4,6}));
    test(bin_rect({2,2},{5,5}), bin_rect({1,3},{3,4}));
    test(bin_rect({2,2},{5,5}), bin_rect({3,1},{4,4}));
    test(bin_rect({2,2},{5,5}), bin_rect({3,3},{6,4}));

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
    test(bin_rect({2,2},{4,4}), bin_rect({1,3},{3,5}));
    test(bin_rect({2,2},{4,4}), bin_rect({1,1},{3,3}));
    test(bin_rect({2,2},{4,4}), bin_rect({3,1},{5,3}));
    test(bin_rect({2,2},{4,4}), bin_rect({3,3},{5,5}));

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
    test(bin_rect({2,2},{3,3}), bin_rect({1,1},{4,4}));

}

int main()
{
    run_tests();
}
