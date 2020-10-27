#!/usr/bin/python3
print("#pragma once\n")
print("struct xcolor { const char* name; uint32_t rgba; };\n")
print("static struct xcolor xcolortab[] = {")
with open("/etc/X11/rgb.txt", "r") as f:
	for l in f:
		r, g, b, *name = l.strip().split()
		s = ' '.join(name)
		try:
			v = int(r) | (int(g)<<8) | (int(b)<<16) | (255<<24)
			print("    { %-24s 0x%08x }," % ("\"%s\"," % s, v))
		except ValueError:
			pass
print("    { %-24s 0x%08x }," % ("0,", 0))
print("};")