#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>

#include <vector>
#include <numeric>
#include <functional>


/*
 * ~~~===~~~
 *
 *  Optimized integer division (libdivide)
 *
 * ~~~===~~~
 */

/*
  zlib License
  ------------

  Copyright (C) 2010 - 2019 ridiculous_fish, <libdivide@ridiculousfish.com>
  Copyright (C) 2016 - 2019 Kim Walisch, <kim.walisch@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
 */

#define LIBDIVIDE_DIV_U32
#define DIV_U32_MULT_FORMAT "{ magic=0x%08x, more=%d }"
#define DIV_U32_MULT_FIELDS m.magic, m.more

struct div_u32_mult_inv
{
    uint32_t magic;
    uint8_t more;
};

static div_u32_mult_inv find_div_u32_mult_inv(uint32_t d)
{
    uint32_t floor_log_2_d = __builtin_clz(d) ^ 31;

    if ((d & (d - 1)) == 0) {
        return (div_u32_mult_inv) { 0, (uint8_t)(floor_log_2_d - 1) };
    }

    uint64_t n = (1ULL << (floor_log_2_d + 32));
    uint32_t magic = n / d;
    uint32_t rem = (uint32_t)(n - magic * (uint64_t)d);

    assert(rem > 0 && rem < d);
    const uint32_t e = d - rem;

    // We have to use the general 33-bit algorithm.  We need to compute
    // (2**power) / d. However, we already have (2**(power-1))/d and
    // its remainder.  By doubling both, and then correcting the
    // remainder, we can compute the larger division.
    // don't care about overflow here - in fact, we expect it
    magic = (magic << 1) + ((rem << 1) >= d || (rem << 1) < rem);

    return (div_u32_mult_inv) { 1 + magic, (uint8_t)floor_log_2_d };
}

static uint32_t div_u32(uint32_t n, div_u32_mult_inv x)
{
    uint64_t q = ((uint64_t)n * x.magic) >> 32;
    uint32_t t = ((n - q) >> 1) + q;
    return t >> x.more;
}

/*
 * ~~~===~~~
 *
 *  Bit manipluation
 *
 * ~~~===~~~
 */

#if defined (_MSC_VER)
#include <intrin.h>
#endif

/* endian helpers using type punning */

typedef union { uint8_t a[8]; uint64_t b; } __bitcast_u64;

static inline uint64_t le64(uint64_t x)
{
    __bitcast_u64 y = {
        .a = { (uint8_t)(x), (uint8_t)(x >> 8),
               (uint8_t)(x >> 16), (uint8_t)(x >> 24),
               (uint8_t)(x >> 32), (uint8_t)(x >> 40),
               (uint8_t)(x >> 48), (uint8_t)(x >> 56) }
    };
    return y.b;
}

/*! clz */
template <typename T>
inline int clz(T val)
{
    const int bits = sizeof(T) << 3;
    unsigned count = 0, found = 0;
    for (int i = bits - 1; i >= 0; --i) {
        count += !(found |= val & T(1)<<i ? 1 : 0);
    }
    return count;
}

/*! ctz */
template <typename T>
inline int ctz(T val)
{
    const int bits = sizeof(T) << 3;
    unsigned count = 0, found = 0;
    for (int i = 0; i < bits; ++i) {
        count += !(found |= val & T(1)<<i ? 1 : 0);
    }
    return count;
}

/* ctz specializations */
#if defined (__GNUC__)
template<> inline int clz(unsigned val) { return __builtin_clz(val); }
template<> inline int clz(unsigned long val) { return __builtin_clzll(val); }
template<> inline int clz(unsigned long long val) { return __builtin_clzll(val); }
template<> inline int ctz(unsigned val) { return __builtin_ctz(val); }
template<> inline int ctz(unsigned long val) { return __builtin_ctzll(val); }
template<> inline int ctz(unsigned long long val) { return __builtin_ctzll(val); }
#endif
#if defined (_MSC_VER)
#if defined (_M_X64)
template<> inline int clz(unsigned val)
{
    return (int)_lzcnt_u32(val);
}
template<> inline int clz(unsigned long long val)
{
    return (int)_lzcnt_u64(val);
}
template<> inline int ctz(unsigned val)
{
    return (int)_tzcnt_u32(val);
}
template<> inline int ctz(unsigned long long val)
{
    return (int)_tzcnt_u64(val);
}
#else
template<> inline int clz(unsigned val)
{
    unsigned long count;
    return _BitScanReverse(&count, val) ^ 31;
}
template<> inline int clz(unsigned long long val)
{
    unsigned long count;
    return _BitScanReverse64(&count, val) ^ 63;
}
template<> inline int ctz(unsigned val)
{
    unsigned long count;
    return _BitScanForward(&count, val);
}
template<> inline int ctz(unsigned long long val)
{
    unsigned long count;
    return _BitScanForward64(&count, val);
}
#endif
#endif

/*
 * ~~~===~~~
 *
 *  Variable length unary coding
 *
 * ~~~===~~~
 */

static const int bits_per_unit = 7;

struct vlu_result
{
    uint64_t val;
    int64_t shamt;
};

/*
 * vlu_encoded_size_56 - VLU8 packet size in bytes
 */
static int vlu_encoded_size_56(uint64_t num, uint64_t limit = 8)
{
    if (!num) return 1;
    int lz = clz(num);
    int t1 = ((sizeof(num) << 3) - lz - 1) / bits_per_unit;
    bool cont = t1 >= limit;
    return cont ? limit : t1 + 1;
}

/*
 * vlu_decoded_size_56 - VLU8 packet size in bytes
 */
static int vlu_decoded_size_56(uint64_t uvlu, uint64_t limit = 8)
{
    int t1 = ctz(~uvlu);
    bool cont = t1 >= limit;
    int shamt = cont ? limit : t1 + 1;
    return shamt;
}

/*
 * vlu_encode_56 - VLU8 encoding with continuation support
 *
 * returns {
 *   val:   encoded value
 *   shamt: shift value from 1 to 8, or -1 for continuation
 * }
 */
static struct vlu_result vlu_encode_56(uint64_t num, uint64_t limit = 8)
{
    if (!num) return vlu_result{ 0, 1 };
    int lz = clz(num);
    int t1 = ((sizeof(num) << 3) - lz - 1) / bits_per_unit;
    bool cont = t1 >= limit;
    int shamt = cont ? limit : t1 + 1;
    uint64_t uvlu = (num << shamt)
        | ((1ull << (shamt-1))-1)
        | ((uint64_t)cont << (limit-1));
    return vlu_result{ uvlu, shamt | -(int64_t)cont };
}

/*
 * vlu_decode_56 - VLU8 decoding with continuation support
 *
 * @param vlu value to decode
 * @param limit for continuation
 * @returns (struct vlu_result) {
 *   val:   decoded value
 *   shamt: shift value from 1 to 8, or -1 for continuation
 * }
 */
static vlu_result vlu_decode_56(uint64_t vlu, uint64_t limit = 8)
{
    int t1 = ctz(~vlu);
    bool cont = t1 >= limit;
    int shamt = cont ? limit : t1 + 1;
    uint64_t mask = ~(-(int64_t)!cont << (shamt * bits_per_unit));
    uint64_t num = (vlu >> shamt) & mask;
    return vlu_result{ num, shamt | -(int64_t)cont };
}


/*
 * ~~~===~~~
 *
 *  IO reader and writer
 *
 * ~~~===~~~
 */

struct reader
{
    virtual void reset() = 0;
    virtual ssize_t read(void *buf, size_t len) = 0;
    virtual void seek(ssize_t offset) = 0;
    virtual size_t tell() const = 0;
};

struct writer
{
    virtual void reset() = 0;
    virtual ssize_t write(const void *buf, size_t len) = 0;
    virtual void seek(ssize_t offset) = 0;
    virtual size_t tell() const = 0;
};

struct vector_buffer
{
    std::vector<uint8_t> buffer;
    size_t offset;

    vector_buffer() : buffer(), offset(0) {}
    vector_buffer(const std::vector<uint8_t> &buffer) : buffer(buffer), offset(0) {}

    void set(const std::vector<uint8_t> &buffer, size_t offset = 0)
    {
        this->buffer = buffer;
        this->offset = offset;
    }

    std::pair<std::vector<uint8_t>,size_t> get()
    {
        return { buffer, offset };
    }

    void reset()
    {
        buffer.clear();
        offset = 0;
    }

    void seek(ssize_t o) { offset = o; }
    size_t tell() const { return offset; }
};

struct vector_reader : reader, vector_buffer
{
    vector_reader() : vector_buffer() {}
    vector_reader(const std::vector<uint8_t> &buffer) : vector_buffer(buffer) {}

    virtual void reset() { vector_buffer::reset(); }
    virtual void seek(ssize_t o) { vector_buffer::seek(o); }
    virtual size_t tell() const { return vector_buffer::tell(); }

    virtual ssize_t read(void *buf, size_t len)
    {
        size_t limit = std::min(len, buffer.size() - offset);
        memcpy(buf, buffer.data() + offset, limit);
        offset += limit;
        return limit;
    }
};

struct vector_writer : writer, vector_buffer
{
    vector_writer() : vector_buffer() {}
    vector_writer(const std::vector<uint8_t> &buffer) : vector_buffer(buffer) {}

    virtual void reset() { vector_buffer::reset(); }
    virtual void seek(ssize_t o) { vector_buffer::seek(o); }
    virtual size_t tell() const { return vector_buffer::tell(); }

    virtual ssize_t write(const void *buf, size_t len)
    {
        buffer.resize(offset + len);
        memcpy(buffer.data() + offset, buf, len);
        offset += len;
        return len;
    }
};

/*
 * ~~~===~~~
 *
 *  Bitcode reader and writer
 *
 * ~~~===~~~
 */

static constexpr uint64_t msk(size_t w)
{
    return w == 64 ? -1ull : (1ull << w) - 1;
}

union u64u {
    uint64_t w;
    uint8_t buf[8];
};

struct bitcode_reader
{
    u64u data; /* little-endian buffer */
    size_t mark; /* buffered bits */
    reader *in; /* input reader */

    bitcode_reader() : data{.w=0}, mark(0), in(nullptr) {}
    bitcode_reader(reader *in) : data{.w=0}, mark(0), in(in) {}

    /* used and available free space in buffer */
    size_t used() const { return mark; }
    size_t avail() const { return (sizeof(data)<<3) - mark; }

    void seek(ssize_t offset)
    {
        mark = 0;
        data.w = 0;
        if (in) in->seek(offset);
    }
    ssize_t tell() const { return in->tell() + (mark & ~7); }

    void reset()
    {
        mark = 0;
        data.w = 0;
        if (in) in->reset();
    }

    void sync()
    {
        size_t bits = avail() & ~7; /* round *down* to byte */

        if (avail() == 0) {
            return;
        } else if (avail() == bits) {
            size_t len = in->read(data.buf + (mark>>3), bits>>3);
            mark += (len<<3);
        } else if (bits > 0) {
            u64u d;
            size_t len = in->read(d.buf, bits>>3);
            size_t chunk = len<<3;
            data.w = le64(((le64(d.w) & msk(chunk)) << mark) | (le64(data.w) & msk(mark)));
        }
    }

    uint64_t read_vlu()
    {
        sync();
        vlu_result result = vlu_decode_56(le64(data.w));
        uint64_t symbol = result.val;
        size_t read = std::min((size_t)(result.shamt<<3), mark);
        mark -= read;
        data.w = le64(le64(data.w) >> read);
        return symbol;
    }

    uint64_t read_fixed(size_t bit_width)
    {
        if (mark < bit_width) sync();
        uint64_t symbol = le64(data.w) & msk(bit_width);
        size_t read = std::min((size_t)bit_width, mark);
        mark -= read;
        data.w = le64(le64(data.w) >> read);
        return symbol;
    }
};

struct bitcode_writer
{
    u64u data; /* little-endian buffer */
    size_t mark; /* buffered bits */
    writer *out; /* output writer */

    bitcode_writer() : data{.w=0}, mark(0), out(nullptr) {}
    bitcode_writer(writer *out) : data{.w=0}, mark(0), out(out) {}

    /* used and available free space in buffer */
    size_t used() const { return mark; }
    size_t avail() const { return (sizeof(data)<<3) - mark; }

    void seek(ssize_t offset)
    {
        mark = 0;
        data.w = 0;
        if (out) out->seek(offset);
    }
    ssize_t tell() const { return out->tell() + (mark & ~7); }

    void reset()
    {
        mark = 0;
        data.w = 0;
        if (out) out->reset();
    }

    void pad()
    {
        mark = (mark + 7) & ~7; /* round *up* to byte */
    }

    void flush()
    {
        pad();
        sync();
    }

    void sync()
    {
        size_t bits = used() & ~7; /* round *down* to byte */

        if (bits == 0) return;

        out->write(data.buf, (bits>>3));
        data.w = le64(le64(data.w) >> bits);
        mark -= bits;
    }

    void write_vlu(uint64_t symbol)
    {
        vlu_result r = vlu_encode_56(symbol);
        while (r.shamt) {
            if (avail() == 0) sync();
            size_t chunk = std::min((size_t)(r.shamt<<3), avail());
            data.w = le64(((r.val & msk(chunk)) << mark) | (le64(data.w) & msk(mark)));
            r.shamt -= (chunk>>3);
            r.val >>= chunk;
            mark += chunk;
        }
    }

    void write_fixed(uint64_t symbol, size_t bit_width)
    {
        while (bit_width) {
            if (avail() == 0) sync();
            size_t chunk = std::min(bit_width, avail());
            data.w = le64(((symbol & msk(chunk)) << mark) | (le64(data.w) & msk(mark)));
            bit_width -= chunk;
            symbol >>= chunk;
            mark += chunk;
        }
    }
};

/*
 * ~~~===~~~
 *
 *  Entropy Coding,
 *  Sachin Garg, 2006.
 *
 * ~~~===~~~
 *
 *  Range coder based upon the carry-less implementation,
 *  derived from work by Dmitry Subbotin.
 *
 */

template <typename WORD_T,
          WORD_T _Top = 1ull << 56,
          WORD_T _Bottom = 1ull << 48,
          WORD_T _MaxRange = _Bottom>
struct RangeCoder
{
    static const WORD_T Top = _Top;
    static const WORD_T Bottom = _Bottom;
    static const WORD_T MaxRange = _MaxRange;

    enum {
        WORD_BITS = sizeof(WORD_T)<<3,
        WORD_SHIFT = (sizeof(WORD_T)-1)<<3,
        WORD_BYTES = sizeof(WORD_T),
    };

    WORD_T Low,Range;
    WORD_T Code;

    uint32_t LastRange;
    div_u32_mult_inv InvRange;

    bitcode_reader *In;
    bitcode_writer *Out;

    RangeCoder() :
        Low(0), Range(-1), Code(0), LastRange(0), In(nullptr), Out(nullptr) {}
    RangeCoder(bitcode_reader *In) :
        Low(0), Range(-1), Code(0), LastRange(0), In(In), Out(nullptr) {}
    RangeCoder(bitcode_writer *Out) :
        Low(0), Range(-1), Code(0), LastRange(0), In(nullptr), Out(Out) {}

    WORD_T DivideRange(WORD_T Range, uint32_t TotalRange)
    {
        return Range / TotalRange;
    }

    void EncodeRange(uint32_t SymbolLow, uint32_t SymbolHigh, uint32_t TotalRange)
    {
        Range = DivideRange(Range, TotalRange);
        Low += SymbolLow * Range;
        Range *= SymbolHigh - SymbolLow;

        while ((Low ^ (Low + Range)) < Top || Range < Bottom && ((Range = -Low & (Bottom - 1)), 1))
        {
            Out->write_fixed(Low >> WORD_SHIFT, 8);
            Range <<= 8;
            Low <<= 8;
        }
    }

    uint32_t GetCurrentCount(uint32_t TotalRange)
    {
        Range = DivideRange(Range, TotalRange);
        return (Code - Low) / Range;
    }

    void RemoveRange(uint32_t SymbolLow, uint32_t SymbolHigh, uint32_t /*TotalRange*/)
    {
        Low += SymbolLow * Range;
        Range *= SymbolHigh - SymbolLow;

        while ((Low ^ Low+Range) < Top || Range < Bottom && ((Range = -Low & (Bottom - 1)), 1))
        {
            Code = (Code << 8) | In->read_fixed(8);
            Range <<= 8;
            Low <<= 8;
        }
    }

    void Prime()
    {
        for (size_t i = 0; i < WORD_BYTES; i++) {
            Code = (Code << 8) | In->read_fixed(8);
        }
    }

    void Flush()
    {
        for (size_t i = 0; i < WORD_BYTES; i++) {
            Out->write_fixed(Low >> WORD_SHIFT, 8);
            Low <<= 8;
        }
    }
};

using RangeCoder64 = RangeCoder<uint64_t, 1ull << 56, 1ull << 48, 1ull << 48>;
using RangeCoder32 = RangeCoder<uint32_t, 1u << 24, 1u << 16, 1u << 16>;

template <>
uint32_t RangeCoder<uint32_t, 1u << 24, 1u << 16, 1u << 16>::DivideRange(uint32_t Range, uint32_t TotalRange)
{
    /* return Range / TotalRange; */
    if (LastRange != TotalRange) {
        InvRange = find_div_u32_mult_inv(TotalRange);
        LastRange = TotalRange;
    }
    return div_u32(Range, InvRange);
}

/*
 * frequency table
 */

static const size_t FreqIntervalMask = 0xff;

enum FreqMode {
    FreqModeDynPerSymbol = 1,   /* update cumulative frequencies every sym */
    FreqModeDynPerInterval = 2, /* update cumulative frequencies periodically */
};

struct FreqTable
{
    std::vector<size_t> Freq;
    std::vector<size_t> CumFreq;

    FreqTable(size_t num_syms)
        : Freq(num_syms), CumFreq(num_syms)
    {
        for (size_t i = 0, sz = Freq.size(); i < sz; i++) {
            Freq[i] = 1;
            CumFreq[i] = i+1;
        }
    }

    void UpdateInterval(size_t sym, size_t MaxRange, size_t i);
    void UpdateSymbol(size_t sym, size_t MaxRange, size_t i);
    void Update(FreqMode freq, size_t sym, size_t MaxRange, size_t i);
    void ToCumulative(size_t MaxRange);
};

/* convert frequency table to cumulative */
static void to_cumualtive(std::vector<size_t> &CumFreq,
    const std::vector<size_t> &Freq)
{
    std::partial_sum(Freq.begin(), Freq.end(), CumFreq.begin());
}

/* rescale regular frequency table */
static void rescale_frequency(std::vector<size_t> &F)
{
    for (auto i = F.begin(), j = F.end(); i != j; i++) {
        *i /= 2;
        if (*i < 1) {
            *i = 1;
        }
    }
}

/* rescale cumulative frequency table */
static void rescale_cumualtive(std::vector<size_t> &F)
{
    for (auto i = F.begin()+1, j = F.end(); i != j; i++) {
        *i /= 2;
        if (*i <= *(i-1)) {
            *i = *(i-1) + 1;
        }
    }
}

/* update cumulative frequency table every interval */
void FreqTable::UpdateInterval(size_t sym, size_t MaxRange, size_t i)
{
    Freq[sym]++;

    if ((i & FreqIntervalMask) == 0) {
        to_cumualtive(CumFreq, Freq);
        if (CumFreq.back() >= MaxRange) {
            rescale_frequency(Freq);
            to_cumualtive(CumFreq, Freq);
        }
    }
}

/* update cumulative frequency table every sym */
void FreqTable::UpdateSymbol(size_t sym, size_t MaxRange, size_t i)
{
    for (size_t j = sym, sz = CumFreq.size(); j < sz; j++) {
        CumFreq[j]++;
    }
    if (CumFreq.back() >= MaxRange) {
        rescale_cumualtive(CumFreq);
    }
}

/* initialize cumulative frequency table */
void FreqTable::Update(FreqMode freq, size_t sym, size_t MaxRange, size_t i)
{
    switch (freq) {
    case FreqModeDynPerSymbol:   UpdateSymbol(sym, MaxRange, i);   break;
    case FreqModeDynPerInterval: UpdateInterval(sym, MaxRange, i); break;
    }
}

/* convert frequency to cumulative frequency */
void FreqTable::ToCumulative(size_t MaxRange)
{
    to_cumualtive(CumFreq, Freq);
    while (CumFreq.back() > MaxRange) {
        rescale_frequency(Freq);
        to_cumualtive(CumFreq, Freq);
    }
}