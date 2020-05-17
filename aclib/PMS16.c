#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pngio.h"
#include "accore.h"
#include "acinternal.h"

// コンパイラオプションで構造体に隙間ができないよう、pragma packで詰めることを指定
#pragma pack (1)
struct PMS16 {
	unsigned __int16 Sig;
	unsigned __int16 Ver;
	unsigned __int16 len_hdr;
	unsigned __int8 depth;
	unsigned __int8 bits;
	unsigned __int32 U0;
	unsigned __int32 U1;
	unsigned __int32 Column_in;
	unsigned __int32 Row_in;
	unsigned __int32 Columns;
	unsigned __int32 Rows;
	unsigned __int32 offset_body;
	unsigned __int32 offset_Pal;
	unsigned __int32 U2;
	unsigned __int32 U3;
	unsigned __int8 data[];
};

struct COLOR_src_low {
	unsigned __int8 Rl : 2;
	unsigned __int8 Gl : 4;
	unsigned __int8 Bl : 2;
};

struct COLOR_src_high {
	unsigned __int8 Rh : 3;
	unsigned __int8 Gh : 2;
	unsigned __int8 Bh : 3;
};

struct COLOR_dest {
	unsigned __int16 Rl : 2;
	unsigned __int16 Rh : 3;
	unsigned __int16 Gl : 4;
	unsigned __int16 Gh : 2;
	unsigned __int16 Bl : 2;
	unsigned __int16 Bh : 3;
};

struct COLOR16RGB {
	unsigned __int16 B : 5;
	unsigned __int16 G : 6;
	unsigned __int16 R : 5;
};

struct COLOR32RGBA {
	unsigned __int8 R;
	unsigned __int8 G;
	unsigned __int8 B;
	unsigned __int8 A;
};
#pragma pack ()

static inline unsigned __int8 d5tod8(unsigned __int8 a)
{
	//	unsigned __int8 r = (double) (a) * 255.0L / 31.0L + 0.5L;
	unsigned __int8 r = ((unsigned)a * 527 + 23) >> 6;
	return r;
}

static inline unsigned __int8 d6tod8(unsigned __int8 a)
{
	//	unsigned __int8 r = (double) (a) * 255.0L / 63.0L + 0.5L;
	unsigned __int8 r = ((unsigned)a * 259 + 33) >> 6;
	return r;
}

static inline void direct16_to_direct32(struct COLOR32RGBA *d32, struct COLOR16RGB *d16, unsigned __int8 *alpha)
{
	d32->R = d5tod8(d16->R);
	d32->G = d6tod8(d16->G);
	d32->B = d5tod8(d16->B);
	d32->A = *alpha;
}

struct image_info* decode_PMS16(FILE* pFi)
{
	const unsigned colours = COLOR65536;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct PMS16);
	struct PMS16* data = malloc(fs.st_size);
	if (data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	const size_t rcount = fread_s(data, fs.st_size, 1, fs.st_size, pFi);
	if (rcount != fs.st_size) {
		wprintf_s(L"File read error.\n");
		free(data);
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	const size_t in_x = data->Column_in;
	const size_t in_y = data->Row_in;
	const size_t len_x = data->Columns;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_x;

	wprintf_s(L"%1u unkonwn %08lX %08lX %08lX %08lX\n", data->bits, data->U0, data->U1, data->U2, data->U3);

	unsigned __int16* data_decoded = malloc(len_decoded * sizeof(unsigned __int16));
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len, cp_len;
	unsigned __int8* src = data->data + data->offset_body - data->len_hdr;
	unsigned __int16* dst = data_decoded, * cp_src;

	while (count-- && (dst - data_decoded) < len_decoded) {
//		wprintf_s(L"%02X %02X, %zu %8lld ->%8lld\n", *src, *(src + 1), count, src - data->data + data->offset_body - data->len_hdr, dst - data_decoded);
		switch (*src) {
		case 0xF8:
			src++;
			*dst++ = *(unsigned __int16*)src;
			src += 2;
			count--;
			break;
		case 0xF9:
			cp_len = *(src + 1) + 1LL;
			static union {
				struct COLOR_src_high h;
				unsigned __int8 a8;
			} uh;
			static union {
				struct COLOR_src_low l;
				unsigned __int8 a8;
			} ul;
			static union {
				struct COLOR_dest d;
				unsigned __int16 a16;
			} ud;

			src += 2;
			count -= 2;
			uh.a8 = *src++;
			for (size_t len = 0; len < cp_len; len++) {
				ul.a8 = *src++;
				ud.d.Rh = uh.h.Rh;
				ud.d.Rl = ul.l.Rl;
				ud.d.Gh = uh.h.Gh;
				ud.d.Gl = ul.l.Gl;
				ud.d.Bh = uh.h.Bh;
				ud.d.Bl = ul.l.Bl;
				*dst++ = ud.a16;
				count--;
			}
			break;
		case 0xFA:
			*dst = *(dst - len_x + 1);
			dst++;
			src++;
			break;
		case 0xFB:
			*dst = *(dst - len_x - 1);
			dst++;
			src++;
			break;
		case 0xFC:
			cp_len = *(src + 1) + 2LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2 * sizeof(unsigned __int16), src + 2, 2 * sizeof(unsigned __int16));
				dst += 2;
			}
			src += 6;
			count -= 5;
			break;
		case 0xFD:
			cp_len = *(src + 1) + 3LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, sizeof(unsigned __int16), src + 2, sizeof(unsigned __int16));
				dst++;
			}
			src += 4;
			count -= 3;
			break;
		case 0xFE:
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - len_x * 2;
			memcpy_s(dst, cp_len * sizeof(unsigned __int16), cp_src, cp_len * sizeof(unsigned __int16));
			dst += cp_len;
			src += 2;
			count--;
			break;
		case 0xFF:
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - len_x;
			memcpy_s(dst, cp_len * sizeof(unsigned __int16), cp_src, cp_len * sizeof(unsigned __int16));
			dst += cp_len;
			src += 2;
			count--;
			break;
		default:
			*dst++ = *(unsigned __int16*)src;
			src += 2;
			count--;
		}
	}

	unsigned __int8* Alpha = malloc(len_decoded);
	if (Alpha == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}
	memset(Alpha, 0xFF, len_decoded);

	if (data->offset_Pal != 0) {
		size_t cp_len;
		unsigned __int8 *src = data->data + data->offset_Pal - data->len_hdr, *dst = Alpha, *cp_src;
		while ((dst - Alpha) < len_decoded) {
			switch (*src) {
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
				src++;
				*dst++ = *src++;
				break;
			case 0xFC:
				cp_len = *(src + 1) + 3LL;
				for (size_t len = 0; len < cp_len; len++) {
					memcpy_s(dst, 2, src + 2, 2);
					dst += 2;
				}
				src += 4;
				count -= 3;
				break;
			case 0xFD:
				cp_len = *(src + 1) + 4LL;
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
				break;
			case 0xFE:
				cp_len = *(src + 1) + 3LL;
				cp_src = dst - len_x * 2;
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0xFF:
				cp_len = *(src + 1) + 3LL;
				cp_src = dst - len_x;
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			default:
				*dst++ = *src++;
			}
		}
	}

	struct COLOR32RGBA* RGBA = malloc(len_decoded * sizeof(struct COLOR32RGBA));
	if (RGBA == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	for (size_t i = 0; i < len_decoded; i++) {
		union {
			struct COLOR16RGB c16;
			unsigned __int16 a16;
		} u;
		u.a16 = *(data_decoded + i);
		direct16_to_direct32(RGBA + i, &u.c16, Alpha + i);
	}

	free(data_decoded);
	free(Alpha);

	static struct image_info I;
	static wchar_t sType[] = L"PMS16";
	I.image = RGBA;
	I.start_x = in_x;
	I.start_y = in_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.offset_x = 0;
	I.offset_y = 0;
	I.colors = colours;
	I.sType = sType;

	free(data);
	return &I;
}