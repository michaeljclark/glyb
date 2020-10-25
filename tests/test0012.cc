#undef NDEBUG
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cassert>

#include <string>

#include "bitcode.h"


/*
 * test cases
 */

static std::string to_binary(uint64_t symbol, size_t bit_width)
{
    static const char* arr[] = { "▄", "▟", "▙", "█" };
    std::string s;
    for (ssize_t i = bit_width-2; i >= 0; i-=2) {
        s.append(arr[(symbol>>i) & 3]);
    }
    return s;
}

void print_buffer(std::vector<uint8_t> &buf)
{
    ssize_t stride = 16;
    for (ssize_t i = 0; i < buf.size(); i += stride) {
        printf("      ");
        for (ssize_t j = i+stride-1; j >= i; j--) {
            if (j >= buf.size()) printf("     ");
            else printf(" 0x%02hhX",
                buf[j]);
        }
        printf("\n");
        printf("%04zX: ", i & 0xffff);
        for (ssize_t j = i+stride-1; j >= i; j--) {
            if (j >= buf.size()) printf(" ░░░░");
            else printf(" %s", to_binary(buf[j], 8).c_str());
        }
        printf("\n");
    }
}

void bitcode_test(vector_writer &vw, const char* name, const char* buf, size_t len)
{
    printf("\n%s:\n", name);
    print_buffer(vw.buffer);
    assert(vw.buffer.size() == len);
    assert(memcmp(vw.buffer.data(), buf, len) == 0);
}

void test_bitcode_fixed_8_8_8()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_fixed(0x0a, 8);
    bw.write_fixed(0x0b, 8);
    bw.write_fixed(0x0c, 8);
    bw.flush();
    bitcode_test(vw, "fixed.8.8.8", "\x0A\x0B\x0C", 3);

    vr.set(vw.buffer);
    assert(br.read_fixed(8) == 0x0a);
    assert(br.read_fixed(8) == 0x0b);
    assert(br.read_fixed(8) == 0x0c);
}

void test_bitcode_fixed_32_32()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_fixed(0xdeadbeef, 32);
    bw.write_fixed(0xfeedbeef, 32);
    bw.flush();
    bitcode_test(vw, "fixed.32.32", "\xEF\xBE\xAD\xDE\xEF\xBE\xED\xFE", 8);

    vr.set(vw.buffer);
    assert(br.read_fixed(32) == 0xdeadbeef);
    assert(br.read_fixed(32) == 0xfeedbeef);
}

void test_bitcode_fixed_8_32_32_8()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_fixed(0xff, 8);
    bw.write_fixed(0xdeadbeef, 32);
    bw.write_fixed(0xfeedbeef, 32);
    bw.write_fixed(0xff, 8);
    bw.flush();
    bitcode_test(vw, "fixed.8.32.32.8", "\xFF\xEF\xBE\xAD\xDE\xEF\xBE\xED\xFE\xFF", 10);

    vr.set(vw.buffer);
    assert(br.read_fixed(8) == 0xff);
    assert(br.read_fixed(32) == 0xdeadbeef);
    assert(br.read_fixed(32) == 0xfeedbeef);
    assert(br.read_fixed(8) == 0xff);
}

void test_bitcode_fixed_64_64()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_fixed(0x0001020304050607, 64);
    bw.write_fixed(0x08090a0b0c0d0e0f, 64);
    bw.flush();
    bitcode_test(vw, "fixed.64.64", "\x07\x06\x05\x04\x03\x02\x01\x00\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08", 16);

    vr.set(vw.buffer);
    assert(br.read_fixed(64) == 0x0001020304050607);
    assert(br.read_fixed(64) == 0x08090a0b0c0d0e0f);
}

void test_bitcode_vlu_7()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<7)-1);
    bw.flush();
    bitcode_test(vw, "vlu.7", "\xFE", 1);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<7)-1);
}

void test_bitcode_vlu_14()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<14)-1);
    bw.flush();
    bitcode_test(vw, "vlu.14", "\xFD\xFF", 2);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<14)-1);
}

void test_bitcode_vlu_21()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<21)-1);
    bw.flush();
    bitcode_test(vw, "vlu.21", "\xFB\xFF\xFF", 3);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<21)-1);
}

void test_bitcode_vlu_28()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<28)-1);
    bw.flush();
    bitcode_test(vw, "vlu.28", "\xF7\xFF\xFF\xFF", 4);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<28)-1);
}

void test_bitcode_vlu_35()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<35)-1);
    bw.flush();
    bitcode_test(vw, "vlu.35", "\xEF\xFF\xFF\xFF\xFF", 5);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<35)-1);
}

void test_bitcode_vlu_42()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<42)-1);
    bw.flush();
    bitcode_test(vw, "vlu.42", "\xDF\xFF\xFF\xFF\xFF\xFF", 6);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<42)-1);
}

void test_bitcode_vlu_49()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<49)-1);
    bw.flush();
    bitcode_test(vw, "vlu.49", "\xBF\xFF\xFF\xFF\xFF\xFF\xFF", 7);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<49)-1);
}

void test_bitcode_vlu_56()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<56)-1);
    bw.flush();
    bitcode_test(vw, "vlu.56", "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<56)-1);
}

void test_bitcode_vlu_mixed()
{
    vector_writer vw;
    vector_reader vr;
    bitcode_writer bw(&vw);
    bitcode_reader br(&vr);

    bw.write_vlu((1ull<<7)-1);
    bw.write_vlu((1ull<<14)-1);
    bw.write_vlu((1ull<<21)-1);
    bw.write_vlu((1ull<<28)-1);
    bw.write_vlu((1ull<<35)-1);
    bw.write_vlu((1ull<<42)-1);
    bw.write_vlu((1ull<<49)-1);
    bw.write_vlu((1ull<<56)-1);
    bw.flush();
    bitcode_test(vw, "vlu.7.14.21.28.35.42.49.56",
        "\xFE\xFD\xFF\xFB\xFF\xFF\xF7\xFF"
        "\xFF\xFF\xEF\xFF\xFF\xFF\xFF\xDF"
        "\xFF\xFF\xFF\xFF\xFF\xBF\xFF\xFF"
        "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF"
        "\xFF\xFF\xFF\xFF", 36);

    vr.set(vw.buffer);
    assert(br.read_vlu() == (1ull<<7)-1);
    assert(br.read_vlu() == (1ull<<14)-1);
    assert(br.read_vlu() == (1ull<<21)-1);
    assert(br.read_vlu() == (1ull<<28)-1);
    assert(br.read_vlu() == (1ull<<35)-1);
    assert(br.read_vlu() == (1ull<<42)-1);
    assert(br.read_vlu() == (1ull<<49)-1);
    assert(br.read_vlu() == (1ull<<56)-1);
}

void test_bitcode()
{
    test_bitcode_fixed_8_8_8();
    test_bitcode_fixed_32_32();
    test_bitcode_fixed_8_32_32_8();
    test_bitcode_fixed_64_64();
    test_bitcode_vlu_7();
    test_bitcode_vlu_14();
    test_bitcode_vlu_21();
    test_bitcode_vlu_28();
    test_bitcode_vlu_35();
    test_bitcode_vlu_42();
    test_bitcode_vlu_49();
    test_bitcode_vlu_56();
    test_bitcode_vlu_mixed();
}

int main(int argc, char **argv)
{
	test_bitcode();
}