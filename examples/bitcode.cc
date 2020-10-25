/*
 * order0 entropy coding
 *
 * - compress and decompress commands with stats
 *
 * Entropy Coding, Sachin Garg, 2006.
 *
 * Range coder based upon the carry-less implementation,
 * derived from work by Dmitry Subbotin.
 */

#undef NDEBUG
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <climits>
#include <cerrno>
#include <ctime>

#include <algorithm>
#include <numeric>
#include <vector>

#include <sys/stat.h>

#include "bitcode.h"

/*
 * file io
 */

/*
 * read file into std::vector using buffered file IO
 */
static size_t read_file(std::vector<uint8_t> &buf, const char* filename)
{
    FILE *f;
    struct stat statbuf;
    if ((f = fopen(filename, "r")) == nullptr) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        exit(1);
    }
    if (fstat(fileno(f), &statbuf) < 0) {
        fprintf(stderr, "fstat: %s\n", strerror(errno));
        exit(1);
    }
    buf.resize(statbuf.st_size);
    size_t len = fread(buf.data(), 1, buf.size(), f);
    assert(buf.size() == len);
    fclose(f);
    return buf.size();
}

/*
 * write file from std::vector using buffered file IO
 */
static size_t write_file(std::vector<uint8_t> &buf, const char* filename)
{
    FILE *f;
    if ((f = fopen(filename, "w")) == nullptr) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        exit(1);
    }
    size_t len = fwrite(buf.data(), 1, buf.size(), f);
    assert(buf.size() == len);
    fclose(f);
    return buf.size();
}

/*
 * order0 range encode using adaptive frequencies
 */
template <typename Coder>
static size_t order0_encode(bitcode_reader &in, bitcode_writer &out,
	size_t input_size, FreqMode freq)
{
	Coder c(&out);
	FreqTable t(256);

	for (size_t i = 0; i < input_size; i++)
	{
		uint8_t sym = in.read_fixed(8);
		c.EncodeRange(!sym ? 0 : t.CumFreq[sym-1], t.CumFreq[sym], t.CumFreq.back());
		t.Update(freq, sym, c.MaxRange, i);
	}

	c.Flush();
	out.flush();

	return out.tell();
}

/*
 * order0 range decode using adaptive frequencies
 */
template <typename Coder>
static size_t order0_decode(bitcode_reader &in, bitcode_writer &out,
	size_t output_size, FreqMode freq)
{
	Coder c(&in);
	FreqTable t(256);

	c.Prime();

	for (size_t i = 0; i < output_size; i++)
	{
		size_t Count = c.GetCurrentCount(t.CumFreq.back());

		auto si = std::upper_bound(t.CumFreq.begin(), t.CumFreq.end(), Count);
		size_t sym = std::distance(t.CumFreq.begin(), si);

		out.write_fixed(sym, 8);
		c.RemoveRange(!sym ? 0 : t.CumFreq[sym-1], t.CumFreq[sym], t.CumFreq.back());
		t.Update(freq, sym, c.MaxRange, i);
	}

	out.flush();

	return out.tell();
}

/*
 * order0 entropy coding compress and decompress commands with stats
 */

static clock_t start, end;
static size_t loops = 1;
static size_t input_size, output_size, decode_size;

static double time_secs(clock_t start, clock_t end, size_t loops)
{
	return (double) (end - start) / CLOCKS_PER_SEC / (double)loops;
}

static void print_results(const char* op, clock_t start, clock_t end,
	size_t input_size, size_t output_size, size_t TimingSize)
{
	printf("%s: %zu -> %zu in %6.2f secs (%8.2f ns/byte)\n",
		op, input_size, output_size, time_secs(start, end, loops),
		time_secs(start, end, loops) / (double)TimingSize * 1e9);
}

static void do_compress(const char* in_filename, const char* out_filename,
	FreqMode freq)
{
	vector_reader in;
	vector_writer out;
    bitcode_reader bin(&in);
    bitcode_writer bout(&out);

	input_size = read_file(in.buffer, in_filename);
	for (size_t i = 0; i < sizeof(size_t); i++) {
		out.buffer.push_back(((uint8_t*)&input_size)[i]);
	}
	out.buffer.resize(input_size + input_size/2);

	start = clock();
	for (size_t l = 0; l < loops; l++) {
		bin.seek(0);
		bout.seek(sizeof(size_t));
		output_size = order0_encode<RangeCoder32>(bin, bout, input_size, freq);
	}
	end = clock();

	out.buffer.resize(output_size += sizeof(size_t));
	write_file(out.buffer, out_filename);
	print_results("Encode0", start, end, input_size, output_size, input_size);
}

static void do_decompress(const char* in_filename, const char* out_filename,
	FreqMode freq)
{
	vector_reader in;
	vector_writer out;
    bitcode_reader bin(&in);
    bitcode_writer bout(&out);

	input_size = read_file(in.buffer, in_filename);
	for (size_t i = 0; i < sizeof(size_t); i++) {
		((uint8_t *)&output_size)[i] = bin.read_fixed(8);
	}
	out.buffer.resize(output_size);

	start = clock();
	for (size_t l = 0; l < loops; l++) {
		bin.seek(sizeof(size_t));
		bout.seek(0);
		decode_size = order0_decode<RangeCoder32>(bin, bout, output_size, freq);
		assert(output_size == decode_size);
	}
	end = clock();

	write_file(out.buffer, out_filename);
	print_results("Decode0", start, end, input_size, output_size, output_size);
}

int main(int argc,char *argv[])
{
	loops = (argc == 6 ? atoi(argv[5]) : loops);
	if (argc < 5) {
		fprintf(stderr, "Usage: c|d s|p input_dataName output_dataName [loops]\n"
						"c: compress\n"
						"d: decompress\n"
						"s: freq_dyn_sym\n"
						"i: freq_dyn_interval\n");
		exit(9);
	} else if (argv[1][0] == 'c' && argv[2][0] == 's') {
		do_compress(argv[3], argv[4], FreqModeDynPerSymbol);
	} else if (argv[1][0] == 'c' && argv[2][0] == 'i') {
		do_compress(argv[3], argv[4], FreqModeDynPerInterval);
	} else if (argv[1][0] == 'd' && argv[2][0] == 's') {
		do_decompress(argv[3], argv[4], FreqModeDynPerSymbol);
	} else if (argv[1][0] == 'd' && argv[2][0] == 'i') {
		do_decompress(argv[3], argv[4], FreqModeDynPerInterval);
	} else {
		fprintf(stderr, "%s: '%s' unknown command", argv[0], argv[1]);
		exit(9);
	}
	return 0;
}
