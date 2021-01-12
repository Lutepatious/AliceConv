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
#pragma pack(1)
// MSX版Little Princessのフォーマット
struct MSX_LP {
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_MSX_LP(FILE* pFi)
{
	const unsigned colours = COLOR8;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct MSX_LP);
	struct MSX_LP* data = GC_malloc(fs.st_size);
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

	const size_t start_x = 0;
	const size_t start_y = 0;
	const size_t len_col = 256;
	const size_t len_x = len_col * 2;
	size_t len_y = 212;
	size_t len_decoded = len_y * len_col;
	unsigned __int8* data_decoded = GC_malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	__int64 count = len, cp_len;
	unsigned __int8* src = data->body, * dst = data_decoded, prev = ~*src, repeat = 0;

	while (count-- > 0 && (dst - data_decoded) < len_decoded) {
		if ((src - data) % 0x100 == 0) {
			if (*src == 1 && *(src + 1) == 1 && *(src + 2) == 1 && *(src + 3) == 1) {
				break;
			}
			repeat = 0;
			prev = ~*src;
		}
		if (*src == 0xFF && *(src + 1) == 0x1A) {
			break;
		}
		else if (repeat) {
			repeat = 0;
			cp_len = *src - 2; // range -2 to 253. Minus cancells previous data. 
			if (cp_len > 0) {
				memset(dst, prev, cp_len);
			}
			dst += cp_len;
			src++;
			prev = ~*src;
		}
		else if (*src == prev) {
			repeat = 1;
			prev = *src;
			*dst++ = *src++;
		}
		else {
			repeat = 0;
			prev = *src;
			*dst++ = *src++;
		}
	}

	size_t lines = (dst - data_decoded) / 0x100;
	size_t offset_x = 0;
	if (lines < 150) {
		len_y = 140;
		if (*(unsigned __int32*)data_decoded == 0x00L) {
			offset_x = 8;
		}
	}
	else if (lines < 190) {
		len_y = 180;
	}
	else if (lines < 200) {
		len_y = 192;
	}
	else {
		len_y = 200;
	}
	len_decoded = len_y * len_col;

	unsigned __int8* decode_buffer = convert_index4_to_index8_LE(data_decoded, len_decoded);

	static const wchar_t sType[] = L"MSX_LP";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * (colours + 1));
	png_bytep Trans = GC_malloc(sizeof(png_byte) * (colours + 1));
	memset(Trans, 0xFF, sizeof(png_byte) * (colours + 1));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		static const struct fPal8_BRG Pal3[8] =
		{ { 0x0, 0x0, 0x0 }, { 0x0, 0x0, 0x7 }, { 0x0, 0x7, 0x0 }, { 0x0, 0x7, 0x7 },
		  { 0x7, 0x0, 0x0 }, { 0x7, 0x0, 0x7 }, { 0x7, 0x7, 0x0 }, { 0x7, 0x7, 0x7 } };

		struct fPal8 inPal3;
		inPal3.R = Pal3[ci].R;
		inPal3.G = Pal3[ci].G;
		inPal3.B = Pal3[ci].B;
		color_8to256(&Pal8[ci], &inPal3);
	}
	color_8to256(&Pal8[colours], NULL);

	struct image_info* pI = GC_malloc(sizeof(struct image_info));
	pI->image = decode_buffer;
	pI->start_x = start_x;
	pI->start_y = start_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->offset_x = offset_x;
	pI->offset_y = 0;
	pI->colors = colours + 1;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = colours;

	return pI;
}