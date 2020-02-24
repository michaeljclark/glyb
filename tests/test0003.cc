#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <cmath>

#include <chrono>
#include <vector>
#include <map>

#include "binpack.h"

void stats(bin_packer &bp)
{
    int alloc_area = 0;
    for (auto i : bp.alloc_map) {
        alloc_area += i.second.area();
    }
    float alloc_percent = ((float)alloc_area/(float)bp.total.area()) * 100.0f;
    printf("------------------------------\n");
    printf("free list node count = %zu\n", bp.free_list.size());
    printf("alloc map node count = %zu\n", bp.alloc_map.size());
    printf("bin dimensions       = %d,%d\n", bp.total.width(), bp.total.height());
    printf("bin total area       = %d\n", bp.total.area());
    printf("bin allocated area   = %d\n", alloc_area);
    printf("bin utilization      = %4.1f%%\n", alloc_percent);
}

int r(int b, int v)
{
    return b + (int)floorf(((float)rand()/(float)RAND_MAX)*(float)(v));
}

void run_test(int w, int h, int b, int v)
{
    bin_packer bp(bin_point(w,h));

    size_t i = 1;
    srand(1);
    const auto t1 = std::chrono::high_resolution_clock::now();
    for (;;) {
        auto l = bp.find_region(i++,bin_point(r(b,v),r(b,v)));
        if (!l.first) break;
    };
    const auto t2 = std::chrono::high_resolution_clock::now();
    float runtime = (float)std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() / 1.e9f;
    bp.dump();
    bp.verify();
    stats(bp);
    printf("runtime              = %f seconds\n", runtime);
}

int main()
{
    //run_test(512,512,1,31);
    //run_test(1024,1024,1,31);
    run_test(1024,1024,16,16);
}
