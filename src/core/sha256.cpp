// SHA-256 (FIPS 180-4), for checking a downloaded update against the sum the Homebrew Channel's repository gives for it.

#include "core/sha256.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

uint32_t rotr (uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct State {
	uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	unsigned char block[64];
	size_t used = 0;
	uint64_t bits = 0;

	void compress () {
		uint32_t w[64];
		for (int i = 0; i < 16; i++) w[i] = (uint32_t)block[i * 4] << 24 | (uint32_t)block[i * 4 + 1] << 16 | (uint32_t)block[i * 4 + 2] << 8 | block[i * 4 + 3];
		for (int i = 16; i < 64; i++) {
			uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
		for (int i = 0; i < 64; i++) {
			uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
			uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
			hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
	}
	void add (const unsigned char *p, size_t n) {
		bits += (uint64_t)n * 8;
		while (n) {
			size_t take = std::min(n, 64 - used);
			memcpy(block + used, p, take);
			used += take; p += take; n -= take;
			if (used == 64) { compress(); used = 0; }
		}
	}
	std::string finish () {
		uint64_t total = bits;
		unsigned char pad = 0x80;
		add(&pad, 1);
		unsigned char zero = 0;
		while (used != 56) add(&zero, 1);
		unsigned char len[8];
		for (int i = 0; i < 8; i++) len[i] = (unsigned char)(total >> (56 - i * 8));
		add(len, 8);
		char out[65];
		for (int i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", h[i]);
		return std::string(out, 64);
	}
};

}  // namespace

std::string sha256_hex (const std::string &data) {
	State s;
	s.add((const unsigned char *)data.data(), data.size());
	return s.finish();
}

std::string sha256_file (const std::string &path) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return "";
	State s;
	unsigned char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.add(buf, n);
	bool bad = ferror(f);
	fclose(f);
	return bad ? "" : s.finish();
}
