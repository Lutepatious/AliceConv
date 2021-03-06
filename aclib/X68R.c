#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "pngio.h"
#include "accore.h"
#include "acinternal.h"

// コンパイラオプションで構造体に隙間ができないよう、pragma packで詰めることを指定
#pragma pack (1)
// X68のRanceIIで使われている16色フォーマット 実態はパックトピクセル版のGL3
struct X68R {
	unsigned __int16 Sig;
	struct {
		unsigned __int8 R;
		unsigned __int8 G;
		unsigned __int8 B;
	} Pal4[16];
	unsigned __int16 Column_start; // divided by 2
	unsigned __int16 Row_start;
	unsigned __int16 Columns; // divided by 2
	unsigned __int16 Rows;
	unsigned __int16 U0;
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_X68R(FILE* pFi)
{
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct X68R);
	struct X68R* data = GC_malloc(fs.st_size);
	if (data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	const size_t rcount = fread_s(data, fs.st_size, 1, fs.st_size, pFi);
	if (rcount != fs.st_size) {
		wprintf_s(L"File read error.\n");
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	const size_t start_col = data->Column_start;
	const size_t start_x = start_col * 2;
	const size_t start_y = data->Row_start;
	const size_t len_col = data->Columns;
	const size_t len_x = len_col * 2;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_col * len_y;
	unsigned __int8* data_decoded = GC_malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	size_t count = len, cp_len;
	unsigned __int8* src = data->body, * dst = data_decoded;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0xFE:
			cp_len = *(src + 1);
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 4;
			count -= 3;
			break;
		case 0xFF:
			cp_len = *(src + 1);
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			count -= 2;
			break;
		default:
			*dst++ = *src++;
		}
	}

	unsigned __int8* decode_buffer = convert_index4_to_index8_BE(data_decoded, len_decoded);

	static const wchar_t sType[] = L"X68R";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * colours);
	png_bytep Trans = GC_malloc(sizeof(png_byte) * colours);
	memset(Trans, 0xFF, sizeof(png_byte) * colours);
	Trans[0] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		struct fPal8 inPal4;
		inPal4.R = data->Pal4[ci].R;
		inPal4.G = data->Pal4[ci].G;
		inPal4.B = data->Pal4[ci].B;
		color_16to256(Pal8 + ci, &inPal4);
	}

	struct image_info* pI = GC_malloc(sizeof(struct image_info));
	pI->image = decode_buffer;
	pI->start_x = start_x;
	pI->start_y = start_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->colors = colours;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = 0;

	return pI;
}