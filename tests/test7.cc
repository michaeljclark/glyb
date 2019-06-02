#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cassert>

#include <string>

#include "color.h"

void p(color c)
{
	printf("color=(%f, %f, %f, %f) rgba32=0x%08x\n",
		c.r, c.g, c.b, c.a, c.rgba32());
}

void t1()
{
	color c("#808080ff");
	p(c);
	assert(c.r <= 0.51f);
	assert(c.g <= 0.51f);
	assert(c.b <= 0.51f);
	assert(c.a == 1.0f);
	assert(c.rgba32() == 0xff808080);
}

int main()
{
	t1();
}