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
// GL2/GL3 フォーマット
struct GL3 {
	struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16];				// もしここが空っぽならFM TOWNS版ALICEの館CDに収録されたIntruderだと思われる
	unsigned __int16 Start;
	unsigned __int16 Columns; // divided by 8
	unsigned __int16 Rows;
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_GL3(FILE* pFi, int isGM3)
{
	const size_t planes = 4;
	const size_t depth = 8;
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct GL3);
	struct GL3* data = GC_malloc(fs.st_size);
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

	const size_t start = data->Start & 0x7FFF;
	const size_t start_x = start % 80 * 8;
	const size_t start_y = start / 80;
	const size_t len_col = data->Columns;
	const size_t len_x = len_col * 8;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_col * len_y * planes;
	unsigned __int8* data_decoded = GC_malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	size_t count = len, cp_len, cur_plane;
	unsigned __int8* src = data->body, * dst = data_decoded, * cp_src;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0x00:
			cp_len = *(src + 1) & 0x7F;
			if (*(src + 1) & 0x80) {
				cp_src = dst - len_col * planes * 2 + 1;
				memset(dst, *cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
			}
			else {
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
			}
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
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D:
			cp_len = *src - 6;
			cp_src = dst - len_col * planes * 2 + 1;
			memset(dst, *cp_src, cp_len);
			dst += cp_len;
			src++;
			break;
		case 0x0E:
			cur_plane = ((dst - data_decoded) / len_col) % planes;
			cp_len = *(src + 1) & 0x7F;
			cp_src = dst - len_col * (cur_plane - 2);
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;
		case 0x0F:
			cur_plane = ((dst - data_decoded) / len_col) % planes;
			cp_len = *(src + 1) & 0x7F;
			cp_src = dst - len_col * ((*(src + 1) & 0x80) ? cur_plane - 1 : cur_plane);
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;

		default:
			*dst++ = *src++;
		}
	}

	struct plane4_dot8* buffer_dot8 = convert_YPC_to_YCP(data_decoded, len_y, len_col, planes);
	unsigned __int8* decode_buffer = convert_plane4_dot8_to_index8(buffer_dot8, len_y * len_col);

	static const wchar_t sType[] = L"GL3";
	static const wchar_t sType_a[] = L"GM3";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * colours);
	png_bytep Trans = GC_malloc(sizeof(png_byte) * colours);
	memset(Trans, 0xFF, sizeof(png_byte)* colours);
	Trans[0] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		if (isGM3 == 1) {
			// GM3フォーマットのためのパレット情報
			static const struct fPal8_BRG Pal4[16] =
				{ { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
				  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
				  { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
				  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF } };

			struct fPal8 inPal4;
			inPal4.R = Pal4[ci].R;
			inPal4.G = Pal4[ci].G;
			inPal4.B = Pal4[ci].B;
			color_16to256(&Pal8[ci], &inPal4);
		}
		else {
			struct fPal8 inPal4;
			inPal4.R = data->Pal4[ci].R;
			inPal4.G = data->Pal4[ci].G;
			inPal4.B = data->Pal4[ci].B;
			color_16to256(&Pal8[ci], &inPal4);
		}
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
	pI->sType = (isGM3 == 1) ? sType_a : sType;
	pI->BGcolor = 0;

	return pI;
}