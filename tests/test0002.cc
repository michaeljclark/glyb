#include <cmath>
#include <cassert>
#include <cstdlib>

#include <vector>
#include <map>

#include "binpack.h"

int main()
{
    bin_packer p(bin_point(10,10));
    assert(p.find_region(1,bin_point(1,1)).first);
    assert(p.find_region(2,bin_point(1,1)).first);
    assert(p.find_region(3,bin_point(1,1)).first);
    assert(p.find_region(4,bin_point(1,1)).first);
    assert(p.find_region(5,bin_point(2,2)).first);
    assert(p.find_region(6,bin_point(2,2)).first);
    assert(p.find_region(7,bin_point(3,1)).first);
    assert(p.find_region(8,bin_point(3,1)).first);
    assert(p.find_region(9,bin_point(3,1)).first);
    p.dump();
}
