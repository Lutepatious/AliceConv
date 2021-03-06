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
// MSX版闘神都市のフォーマット
struct MSX_TT {
	unsigned __int16 Column_in; // divided by 2
	unsigned __int16 Row_in;
	unsigned __int16 Columns; // divided by 2
	unsigned __int16 Rows;
	struct MSX_Palette Pal3[16];
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_MSX_TT(FILE* pFi)
{
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct MSX_TT);
	struct MSX_TT* data = GC_malloc(fs.st_size);
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

	const size_t in_col = data->Column_in;
	const size_t in_x = in_col * 2;
	size_t in_y = data->Row_in;
	const size_t len_col = data->Columns;
	const size_t len_x = len_col * 2;
	size_t len_y = data->Rows;
	const size_t out_col = in_col + len_col;
	const size_t out_x = out_col * 2;
	size_t out_y = in_y + len_y;
	size_t len_decoded = len_y * len_col * 2;

	unsigned __int8* data_decoded = GC_malloc(len_decoded * sizeof(unsigned __int8));
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	size_t count = len, cp_len;
	unsigned __int8* src = data->body, * dst = data_decoded;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0x00:
			cp_len = *(src + 1);
			if (cp_len == 0) {
				break;
			}
			memcpy_s(dst, cp_len, dst - len_col * 2, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;
		case 0x01:
			cp_len = *(src + 1);
			memcpy_s(dst, cp_len, dst - len_col, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;
		case 0x02:
			cp_len = *(src + 1);
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			count -= 2;
			break;
		case 0x03:
			src++;
			*dst++ = *src++;
			break;
		default:
			*dst++ = *src++;
		}
	}

	size_t ratio_XY = len_decoded / (dst - data_decoded);

	if (ratio_XY == 1) {
		unsigned __int8* data_deinterlaced = GC_malloc(len_decoded);
		for (size_t y = 0; y < len_y; y++) {
			memcpy_s(data_deinterlaced + len_col * y * 2, len_col, data_decoded + len_col * y, len_col);
			memcpy_s(data_deinterlaced + len_col * (y * 2 + 1), len_col, data_decoded + len_col * (len_y + y), len_col);
		}
		data_decoded = data_deinterlaced;
		in_y *= 2;
		len_y *= 2;
		out_y *= 2;
	}
	else {
		len_decoded /= 2;
	}

	unsigned __int8* decode_buffer = convert_index4_to_index8_LE(data_decoded, len_decoded);

	static const wchar_t sType[] = L"MSX_TT";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * (colours + 1));
	png_bytep Trans = GC_malloc(sizeof(png_byte) * (colours + 1));
	memset(Trans, 0xFF, sizeof(png_byte) * (colours + 1));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		struct fPal8 inPal3;
		inPal3.R = data->Pal3[ci].R;
		inPal3.G = data->Pal3[ci].G;
		inPal3.B = data->Pal3[ci].B;
		color_8to256(&Pal8[ci], &inPal3);
	}
	color_8to256(&Pal8[colours], NULL);

	struct image_info* pI = GC_malloc(sizeof(struct image_info));
	pI->image = decode_buffer;
	pI->start_x = in_x;
	pI->start_y = in_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->colors = colours + 1;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = colours;

	return pI;
}