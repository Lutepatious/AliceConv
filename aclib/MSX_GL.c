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
	struct MSX_GL* data = malloc(fs.st_size);
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

	const size_t start = data->Start;
	const size_t start_x = start % 256 * 2;
	const size_t start_y = start / 256;
	const size_t len_col = data->Columns ? data->Columns : 256;
	const size_t len_x = len_col * 2;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_col;
	unsigned __int8* data_decoded = calloc(len_decoded, sizeof(unsigned __int8));
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
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

	size_t decode_len = len_y * len_x;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	for (size_t i = 0; i < len_decoded; i++) {
		decode_buffer[i * 2] = (data_decoded[i] & 0xF0) >> 4;
		decode_buffer[i * 2 + 1] = data_decoded[i] & 0xF;
	}

	free(data_decoded);

	static struct image_info I;
	static wchar_t sType[] = L"MSX_GL";
	static png_color Pal8[COLOR16 + 1];
	static png_byte Trans[COLOR16 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		color_8to256(&Pal8[ci], data->Pal3[ci].B, data->Pal3[ci].R, data->Pal3[ci].G);
	}
	color_8to256(&Pal8[colours], 0, 0, 0);

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