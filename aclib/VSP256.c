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
// VSP 256色フォーマット
struct VSP256 {
	unsigned __int16 Column_in;
	unsigned __int16 Row_in;
	unsigned __int16 Column_out;
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[24];
	struct {
		unsigned __int8 R;
		unsigned __int8 G;
		unsigned __int8 B;
	} Pal8[256];
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_VSP256(FILE* pFi)
{
	const unsigned colours = COLOR256;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct VSP256);
	struct VSP256* data = malloc(fs.st_size);
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
	const size_t out_x = data->Column_out + 1LL;
	const size_t out_y = data->Row_out + 1LL;
	const size_t len_x = out_x - in_x;
	const size_t len_y = out_y - in_y;
	const size_t len_decoded = len_y * len_x;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len, cp_len;
	unsigned __int8* src = data->body, * dst = data_decoded, * cp_src;
	while (count-- && (dst - data_decoded) < len_decoded) {
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

	static struct image_info I;
	static wchar_t sType[] = L"VSP256";
	static png_color Pal8[COLOR256];
	static png_byte Trans[COLOR256];

	memset(Trans, 0xFF, sizeof(Trans));

	for (size_t ci = 0; ci < colours; ci++) {
		color_256to256(&Pal8[ci], data->Pal8[ci].B, data->Pal8[ci].R, data->Pal8[ci].G);
	}

	I.image = data_decoded;
	I.start_x = in_x;
	I.start_y = in_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.colors = colours;
	I.Pal8 = Pal8;
	I.Trans = Trans;
	I.sType = sType;

	free(data);
	return &I;
}
