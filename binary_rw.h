// Simple I/O Streams for bytes

#ifndef BINARY_RW_H
#define BINARY_RW_H

#include "nob.h"

typedef struct BReader {
	const char *data;
	size_t count;
} BReader;

enum {
	BNULL,
	BU8,
	BU16,
	BU32,
	BU64,
	BN,
	BSN,
};

bool BReadU8(BReader* br, uint8_t*  out);
bool BReadU16(BReader* br, uint16_t* out);
bool BReadU32(BReader* br, uint32_t* out);
bool BReadU64(BReader* br, uint64_t* out);
bool BReadN(BReader* br, char* out, size_t n);
Nob_String_Builder BReadSB(BReader* br, size_t n);

typedef struct BWriter {
	char*  items;
	size_t count;
	size_t capacity;
} BWriter;

extern BWriter bw_temp;

void BWriteU8(BWriter* bw, const uint8_t  in);
void BWriteU16(BWriter* bw, const uint16_t in);
void BWriteU32(BWriter* bw, const uint32_t in);
void BWriteU64(BWriter* bw, const uint64_t in);
void BWriteN(BWriter* bw, const char* in, size_t n);
void BWriteSN(BWriter* bw, const char* in, size_t n); // [ size(4) | data(N) ]
BWriter _BWriteBuild(BWriter* bw, ...);
void BWriterFree(BWriter bw);

#endif /* BINARY_RW_H */

#ifdef BINARY_RW_IMPLEMENTATION

#define BR_READ_OUT(type_, amount_) \
	do { \
		if (amount_ > br->count) \
			return false; \
		memcpy(out, br->data, amount_); \
		br->data += amount_; \
		br->count -= amount_; \
		return true; \
	} while(0);

bool BReadU8(BReader* br, uint8_t* out) { BR_READ_OUT(uint8_t,  1ull); }
bool BReadU16(BReader* br, uint16_t* out) { BR_READ_OUT(uint16_t, 2ull); }
bool BReadU32(BReader* br, uint32_t* out) { BR_READ_OUT(uint32_t, 4ull); }
bool BReadU64(BReader* br, uint64_t* out) { BR_READ_OUT(uint64_t, 8ull); }
bool BReadN(BReader* br, char* out, size_t n) { BR_READ_OUT(uint8_t,  n); }
Nob_String_Builder BReadSB(BReader* br, size_t n) {
	Nob_String_Builder sb = {0};
	nob_da_reserve(&sb, n);
	sb.count = n;
	if (!BReadN(br, sb.items, sb.count))
		return (Nob_String_Builder){ 0 };
	return sb;
}

BWriter bw_temp;

#define BR_WRITE_IN(in_, amount_) \
	nob_sb_append_buf(bw, (uint8_t*)in_, amount_);

void BWriteU8(BWriter* bw, const uint8_t  in) { BR_WRITE_IN(&in, 1); }
void BWriteU16(BWriter* bw, const uint16_t in) { BR_WRITE_IN(&in, 2); }
void BWriteU32(BWriter* bw, const uint32_t in) { BR_WRITE_IN(&in, 4); }
void BWriteU64(BWriter* bw, const uint64_t in) { BR_WRITE_IN(&in, 8); }
void BWriteN(BWriter* bw, const char* in, size_t n) { BR_WRITE_IN(in, n); }
void BWriteSN(BWriter* bw, const char* in, size_t n) { // TODO: same with BR
	BWriteU32(bw, (uint32_t)n);
	BWriteN(bw, in, n);
}
BWriter _BWriterAppend(BWriter* bw, ...) {
	BWriter bw_new = {0};
	if (bw == NULL) { bw = &bw_new; }
	va_list args;
	va_start(args, bw);
	while (1) {
		int type = va_arg(args, int);
		switch (type) {
			case BU8:
				BWriteU8(bw, (uint8_t)va_arg(args, int));
				break;
			case BU16:
				BWriteU16(bw, (uint16_t)va_arg(args, int));
				break;
			case BU32:
				BWriteU32(bw, (uint32_t)va_arg(args, int));
				break;
			case BU64:
				BWriteU64(bw, (uint64_t)va_arg(args, int));
				break;
			case BN:
				{
				uint32_t len = (uint32_t)va_arg(args, int);
				char* buf = va_arg(args, char*);
				BWriteN(bw, buf, len);
				}
				break;
			case BSN:
				{
				uint32_t len = (uint32_t)va_arg(args, int);
				char* buf = va_arg(args, char*);
				BWriteSN(bw, buf, len);
				}
				break;
			case BNULL:
				goto defer;
		}
	}
defer:
	va_end(args);
	return bw_new;
}
#define BWriterAppend(...) _BWriterAppend(__VA_ARGS__, BNULL)
void BWriterFree(BWriter bw) { nob_sb_free(bw); }

#endif /* BINARY_RW_IMPLEMENTATION */
