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
	struct MSX_LP* data = malloc(fs.st_size);
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

	const size_t start_x = 0;
	const size_t start_y = 0;
	const size_t len_col = 256;
	const size_t len_x = len_col * 2;
	size_t len_y = 212;
	size_t len_decoded = len_y * len_col;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
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

	size_t decode_len = len_x * len_y;
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

	struct fPal8 Pal3[8] = { { 0x0, 0x0, 0x0 }, { 0x0, 0x0, 0x7 }, { 0x0, 0x7, 0x0 }, { 0x0, 0x7, 0x7 },
								{ 0x7, 0x0, 0x0 }, { 0x7, 0x0, 0x7 }, { 0x7, 0x7, 0x0 }, { 0x7, 0x7, 0x7 } };


	static struct image_info I;
	static wchar_t sType[] = L"MSX_LP";
	static png_color Pal8[COLOR8 + 1];
	static png_byte Trans[COLOR8 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		color_8to256(&Pal8[ci], &Pal3[ci]);
	}
	color_8to256(&Pal8[colours], NULL);

	I.image = decode_buffer;
	I.start_x = start_x;
	I.start_y = start_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.offset_x = offset_x;
	I.offset_y = 0;
	I.colors = colours + 1;
	I.Pal8 = Pal8;
	I.Trans = Trans;
	I.sType = sType;
	I.BGcolor = colours;

	free(data);
	return &I;
}