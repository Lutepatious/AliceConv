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
// GLフォーマット
struct GL {
	unsigned __int16 Start;
	unsigned __int8 Columns; // divided by 8
	unsigned __int8 Rows;
	unsigned __int16 U0;
	unsigned __int16 U1;
	struct GL_Palette3 Pal3[8];
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_GL(FILE* pFi)
{
	const size_t planes = 3;
	const size_t depth = 8;
	const unsigned colours = COLOR8;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct GL);
	struct GL* data = malloc(fs.st_size);
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

	const size_t start = _byteswap_ushort(data->Start) & 0x3FFF;
	const size_t start_x = start % 80 * 8;
	const size_t start_y = start / 80;
	const size_t len_col = data->Columns;
	const size_t len_x = len_col * 8;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_col * len_y * planes;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len, cp_len, cur_plane;
	unsigned __int8* src = data->body, * dst = data_decoded, * cp_src;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0x00:
			cp_len = *(src + 1) & 0x7F;
			if (*(src + 1) & 0x80) {
				cp_src = dst - len_col * planes * 4 + 1;
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
		case 0x0E:
			cp_len = *src - 6;
			cp_src = dst - len_col * planes * 4 + 1;
			memset(dst, *cp_src, cp_len);
			dst += cp_len;
			src++;
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

	// データの並びを[y][plane][x]から[y*x][plane]に変える。
	const size_t len_dot8 = len_col * len_y;
	struct plane4_dot8* buffer_dot8 = calloc(len_dot8, sizeof(struct plane4_dot8));
	if (buffer_dot8 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	struct plane4_dot8* buffer_dot8_dest = buffer_dot8;
	for (size_t iy = 0; iy < len_y; iy++) {
		for (size_t ix = 0; ix < len_col; ix++) {
			for (size_t ip = 0; ip < planes; ip++) {
					buffer_dot8_dest->pix8[ip] = data_decoded[(iy * planes + ip) * len_col + ix];
			}
			buffer_dot8_dest++;
		}
	}
	free(data_decoded);

	const size_t decode_len = len_x * len_y;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(buffer_dot8);
		exit(-2);
	}
	convert_plane4_dot8_to_index8(decode_buffer, buffer_dot8, len_dot8);
	free(buffer_dot8);

	static struct image_info I;
	static wchar_t sType[] = L"GL";
	static png_color Pal8[COLOR8 + 1];
	static png_byte Trans[COLOR8 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		struct fPal8 inPal3;
		inPal3.R = data->Pal3[ci].R;
		inPal3.G = data->Pal3[ci].G;
		inPal3.B = data->Pal3[ci].B;
		color_8to256(&Pal8[ci], &inPal3);
	}
	color_8to256(&Pal8[colours], NULL);

	I.image = decode_buffer;
	I.start_x = start_x;
	I.start_y = start_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.colors = colours + 1;
	I.Pal8 = Pal8;
	I.Trans = Trans;
	I.sType = sType;
	I.BGcolor = colours;

	free(data);
	return &I;
}