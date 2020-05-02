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
// VSP 200ラインフォーマット
struct VSP200l {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct GL_Palette3 Pal3[8];
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_VSP200l(FILE* pFi)
{
	const size_t planes = 3;
	const size_t depth = 8;
	const unsigned colours = COLOR8;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct VSP200l);
	struct VSP200l* data = malloc(fs.st_size);
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

	const size_t in_col = data->Column_in;
	const size_t in_x = in_col * 8;
	const size_t in_y = data->Row_in;
	const size_t out_col = data->Column_out;
	const size_t out_x = out_col * 8;
	const size_t out_y = data->Row_out;
	const size_t len_col = data->Column_out - data->Column_in;
	const size_t len_x = len_col * 8;
	const size_t len_y = data->Row_out - data->Row_in;
	const size_t len_decoded = len_y * len_col * planes;

	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len, cp_len, cur_plane;
	unsigned __int8* src = data->body, * dst = data_decoded, * cp_src, negate = 0;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0x00:
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - len_y * planes;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;
		case 0x01:
			cp_len = *(src + 1) + 1LL;
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			count -= 2;
			break;
		case 0x02:
			cp_len = *(src + 1) + 1LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 4;
			count -= 3;
			break;
		case 0x03:
			cur_plane = ((dst - data_decoded) / len_y) % planes;
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - len_y * cur_plane;
			if (negate) {
				for (size_t len = 0; len < cp_len; len++) {
					*dst++ = ~*cp_src++;
				}
			}
			else {
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
			}
			src += 2;
			count--;
			negate = 0;
			break;
		case 0x04:
			cur_plane = ((dst - data_decoded) / len_y) % planes;
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - len_y * (cur_plane - 1);
			if (negate) {
				for (size_t len = 0; len < cp_len; len++) {
					*dst++ = ~*cp_src++;
				}
			}
			else {
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
			}
			src += 2;
			count--;
			negate = 0;
			break;
		case 0x05:
			wprintf_s(L"No! You must NOT.\n");
			cur_plane = ((dst - data_decoded) / len_y) % planes;
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - len_y * (cur_plane - 2);
			if (negate) {
				for (size_t len = 0; len < cp_len; len++) {
					*dst++ = ~*cp_src++;
				}
			}
			else {
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
			}
			src += 2;
			count--;
			negate = 0;
			break;
		case 0x06:
			src++;
			negate = 1;
			break;
		case 0x07:
			src++;
			*dst++ = *src++;
			break;
		default:
			*dst++ = *src++;
		}
	}

	unsigned __int8* data_decoded2 = malloc(len_decoded);
	if (data_decoded2 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	for (size_t ix = 0; ix < len_col; ix++) {
		for (size_t ip = 0; ip < planes; ip++) {
			for (size_t iy = 0; iy < len_y; iy++) {
				data_decoded2[iy * len_col * planes + ip * len_col + ix] = data_decoded[ix * len_y * planes + ip * len_y + iy];
			}
		}
	}
	free(data_decoded);

	size_t decode_len = len_x * len_y;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded2);
		exit(-2);
	}

	convert_8dot_from_3plane_to_8bitpackedpixel(decode_buffer, data_decoded2, len_col, len_y);
	free(data_decoded2);

	static struct image_info I;
	static wchar_t sType[] = L"VSP200l";
	static png_color Pal8[COLOR8 + 1];
	static png_byte Trans[COLOR8 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		color_8to256(&Pal8[ci], data->Pal3[ci].B, data->Pal3[ci].R, data->Pal3[ci].G);
	}
	color_8to256(&Pal8[colours], 0, 0, 0);

	I.image = decode_buffer;
	I.start_x = in_x;
	I.start_y = in_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.colors = colours + 1;
	I.Pal8 = Pal8;
	I.Trans = Trans;
	I.sType = sType;

	free(data);
	return &I;
}