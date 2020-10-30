#!/usr/bin/python3

import sys

def capitalize(s):
	return s[0:1].upper() + s[1:]
def normalize(name):
	return ''.join(map(capitalize, name))

# read colors
names = set()
colors = []
with open("/etc/X11/rgb.txt", "r") as f:
	for l in f:
		r, g, b, *name = l.strip().split()
		s = normalize(name)
		if s not in names:
			names.add(s)
			try:
				v = int(r) | (int(g)<<8) | (int(b)<<16) | (255<<24)
				colors.append({ 'name': s, 'rgba': v })
			except ValueError:
				pass

# check command line arguments
filter = len(sys.argv) > 1 and sys.argv[1] == "--filter"

# print color array
print("static struct xcolor xcolortab[] = {")
for c in sorted(colors, key = lambda i: i['name']):
	x = sum(1 for c in c['name'] if c.isupper())
	if filter and x > 1 or c['name'][-1].isdigit():
		continue
	print("    { %-24s 0x%08x }," % ("\"%s\"," % c['name'], c['rgba']))
print("    { %-24s 0x%08x }," % ("0,", 0))
print("};")