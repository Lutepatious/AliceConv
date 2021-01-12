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
// MSXフォーマット
struct MSX_GL {
	unsigned __int16 Start;
	unsigned __int8 Columns; // divided by 2
	unsigned __int8 Rows;
	unsigned __int16 Unknown[6];
	struct MSX_Palette Pal3[16];
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_MSX_GL(FILE* pFi)
{
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct MSX_GL);
	struct MSX_GL* data = GC_malloc(fs.st_size);
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

	const size_t start = data->Start;
	const size_t start_x = start % 256 * 2;
	const size_t start_y = start / 256;
	const size_t len_col = data->Columns ? data->Columns : 256;
	const size_t len_x = len_col * 2;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_col;
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
			cp_len = *(src + 1) ? *(src + 1) : 256;
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			count -= 2;
			break;
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			cp_len = *src;
			memset(dst, *(src + 1), cp_len);
			src += 2;
			dst += cp_len;
			count--;
			break;
		default:
			*dst++ = *src++;
		}
	}

	unsigned __int8* decode_buffer = convert_index4_to_index8_LE(data_decoded, len_decoded);

	static const wchar_t sType[] = L"MSX_GL";
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
	pI->start_x = start_x;
	pI->start_y = start_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->colors = colours + 1;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = colours;

	return pI;
}