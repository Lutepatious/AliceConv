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
struct PMS8 {
	unsigned __int16 Sig;
	unsigned __int16 Ver;
	unsigned __int16 len_hdr;
	unsigned __int8 depth;
	unsigned __int8 bits;
	unsigned __int32 U0;
	unsigned __int32 U1;
	unsigned __int32 Column_in;
	unsigned __int32 Row_in;
	unsigned __int32 Columns;
	unsigned __int32 Rows;
	unsigned __int32 offset_body;
	unsigned __int32 offset_Pal;
	unsigned __int32 U2;
	unsigned __int32 U3;
	unsigned __int8 data[];
};
#pragma pack ()

struct image_info* decode_PMS8(FILE* pFi)
{
	const unsigned colours = COLOR256;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct PMS8);
	struct PMS8* data = malloc(fs.st_size);
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
	const size_t len_x = data->Columns;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_x;

	wprintf_s(L"%1u %1u unkonwn %08lX %08lX %08lX %08lX\n"
		, data->Ver, data->bits, data->U0, data->U1, data->U2, data->U3);


	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len, cp_len;
	unsigned __int8* src = data->data + data->offset_body - data->len_hdr, * dst = data_decoded, * cp_src;
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
	static wchar_t sType[] = L"PMS8";
	static png_color Pal8[COLOR256];
	static png_byte Trans[COLOR256];

	memset(Trans, 0xFF, sizeof(Trans));

	struct fPal8 *inPal8 = data->data + data->offset_Pal - data->len_hdr;
	for (size_t ci = 0; ci < colours; ci++) {
		color_256to256(&Pal8[ci], inPal8++);
	}

	I.image = data_decoded;
	I.start_x = in_x;
	I.start_y = in_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.offset_x = 0;
	I.offset_y = 0;
	I.colors = colours;
	I.Pal8 = Pal8;
	I.Trans = Trans;
	I.sType = sType;
	I.BGcolor = 0;

	free(data);
	return &I;
}