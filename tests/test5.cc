#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cassert>
#include <cmath>
#include <ctime>

#include <vector>
#include <map>
#include <memory>
#include <tuple>
#include <atomic>
#include <mutex>

#include "binpack.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "image.h"
#include "glyph.h"
#include "text.h"

void tr(text_container t, size_t offset, size_t count)
{
	static int test_count = 0;

	std::string before = t.as_plaintext();
	t.erase(offset, count);
	std::string after = t.as_plaintext();
	bool pass = (after == before.erase(offset, count));
	printf("test-%d before=%-20s after=%-20s : %s (%zu, %zu)\n",
		test_count++, before.c_str(), after.c_str(),
		pass ? "PASS" : "FAIL", offset, count);
}

void t1()
{
	text_container t;
	t.append(text_part("0124"));
	t.append(text_part("5678"));
	t.append(text_part("9abc"));
	tr(t, 0, 12);
	tr(t, 2, 2);
	tr(t, 2, 4);
	tr(t, 2, 6);
	tr(t, 2, 8);
	tr(t, 8, 2);
	tr(t, 8, 4);
	tr(t, 1, 2);
	tr(t, 10, 2);
}

void ti(text_container t, size_t offset, std::string s)
{
	static int test_count = 0;

	std::string before = t.as_plaintext();
	t.insert(offset, s);
	std::string after = t.as_plaintext();
	bool pass = (after == before.insert(offset, s));
	printf("test-%d before=%-20s after=%-20s : %s (%zu, %s)\n",
		test_count++, before.c_str(), after.c_str(),
		pass ? "PASS" : "FAIL", offset, s.c_str());
}

void t2()
{
	text_container t;
	t.append(text_part("01"));
	t.append(text_part("23"));
	ti(t, 0, "_");
	ti(t, 1, "_");
	ti(t, 2, "_");
	ti(t, 3, "_");
	ti(t, 4, "_");
}

void ti(text_container t, size_t offset, text_part s)
{
	static int test_count = 0;

	std::string before = t.as_plaintext();
	t.insert(offset, s);
	std::string after = t.as_plaintext();
	bool pass = (after == before.insert(offset, s.text));
	printf("test-%d before=%-20s after=%-20s : %s (%zu, %s)\n",
		test_count++, before.c_str(), after.c_str(),
		pass ? "PASS" : "FAIL", offset, s.text.c_str());
}

void t3()
{
	text_container t;
	t.append(text_part("01"));
	t.append(text_part("23"));
	ti(t, 0, text_part("_"));
	ti(t, 1, text_part("_"));
	ti(t, 2, text_part("_"));
	ti(t, 3, text_part("_"));
	ti(t, 4, text_part("_"));
}

void t4()
{
	text_container t;
	t.append(text_part("01", {{ "0", "0" }} ));
	t.append(text_part("23", {{ "1", "1" }} ));
	ti(t, 0, text_part("_"));
	ti(t, 1, text_part("_"));
	ti(t, 2, text_part("_"));
	ti(t, 3, text_part("_"));
	ti(t, 4, text_part("_"));
}

void tm(text_container t, size_t offset, size_t count, std::string attr, std::string value)
{
	static int test_count = 0;

	printf("before : %s\n", t.to_string().c_str());
	std::string before = t.as_plaintext();
	t.mark(offset, count, attr, value);
	printf("after  : %s\n", t.to_string().c_str());
	std::string after = t.as_plaintext();
	bool pass = (before == after);
	printf("test-%d before=%-20s after=%-20s : %s add_attr(%zu, %zu, %s=%s)\n",
		test_count++, before.c_str(), after.c_str(),
		pass ? "PASS" : "FAIL", offset, count, attr.c_str(), value.c_str());
}


void t5()
{
	text_container t;
	t.append(text_part("01"));
	t.append(text_part("23", {{ "1", "1" }} ));
	tm(t, 0, 1, "3", "3");
	tm(t, 0, 2, "3", "3");
	tm(t, 0, 3, "3", "3");
	tm(t, 0, 4, "3", "3");
	tm(t, 1, 1, "3", "3");
	tm(t, 1, 2, "3", "3");
	tm(t, 1, 3, "3", "3");
	tm(t, 2, 1, "3", "3");
	tm(t, 2, 2, "3", "3");
}

void tu(text_container t, size_t offset, size_t count, std::string attr)
{
	static int test_count = 0;

	printf("before : %s\n", t.to_string().c_str());
	std::string before = t.as_plaintext();
	t.unmark(offset, count, attr);
	printf("after  : %s\n", t.to_string().c_str());
	std::string after = t.as_plaintext();
	bool pass = (before == after);
	printf("test-%d before=%-20s after=%-20s : %s del_attr(%zu, %zu, %s)\n",
		test_count++, before.c_str(), after.c_str(),
		pass ? "PASS" : "FAIL", offset, count, attr.c_str());
}

void t6()
{
	text_container t;
	t.append(text_part("01", {{ "1", "1" }} ));
	t.append(text_part("23", {{ "1", "1" }} ));
	tu(t, 0, 1, "1");
	tu(t, 0, 2, "1");
	tu(t, 0, 3, "1");
	tu(t, 0, 4, "1");
	tu(t, 1, 1, "1");
	tu(t, 1, 2, "1");
	tu(t, 1, 3, "1");
	tu(t, 2, 1, "1");
	tu(t, 2, 2, "1");
}

int main()
{
	t1();
	t2();
	t3();
	t4();
	t5();
	t6();
}