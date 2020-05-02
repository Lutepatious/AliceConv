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
// X68の闘神都市の一部で使われている256色フォーマット 但し要素はビッグエンディアンなのでバイトスワップを忘れずに
static struct X68T {
	unsigned __int32 Sig;
	unsigned __int32 U0;
	unsigned __int16 U1;
	unsigned __int16 U2;
	unsigned __int16 Pal5BE[0xC0]; // Palette No. 0x40 to 0xFF
	unsigned __int16 U3;
	unsigned __int16 StartBE;
	unsigned __int16 ColsBE;
	unsigned __int16 RowsBE;
	unsigned __int8 body[];
};
#pragma pack()
struct image_info* decode_X68T(FILE* pFi)
{
	const size_t x68_edge_len = 512;
	const unsigned colours = COLOR256;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct X68T);
	struct X68T* data = malloc(fs.st_size);
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

	const size_t in = _byteswap_ushort(data->StartBE) / 2;
	const size_t in_x = in % x68_edge_len;
	const size_t in_y = in / x68_edge_len;
	const size_t len_x = _byteswap_ushort(data->ColsBE);
	const size_t len_y = _byteswap_ushort(data->RowsBE);
	const size_t len_decoded = len_x * len_y;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len, cp_len;
	unsigned __int8* src = data->body, * dst = data_decoded, prev = *src;
	while (count-- && (dst - data_decoded) < len_decoded) {
		if (*(unsigned __int32*)src == 0xFFFFFFFFUL) {
			break;
		}
		switch (*src) {
		case 0x00:
			cp_len = *(src + 2);
			memset(dst, *(src + 1), cp_len);
			dst += cp_len;
			src += 3;
			count -= 2;
			break;
		case 0x01:
			cp_len = *(src + 3);
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 1, 2);
				dst += 2;
			}
			src += 4;
			count -= 3;
			break;
		default:
			*dst++ = *src++;
		}
	}

	static struct image_info I;
	static png_color Pal8[COLOR256];
	static png_byte Trans[COLOR256];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[0] = 0;

	union X68Pal_conv {
		struct X68_Palette5 Pal5;
		unsigned __int16 Pin;
	} P;
	for (size_t ci = 0; ci < colours - 0x40; ci++) {
		P.Pin = _byteswap_ushort(data->Pal5BE[ci]);
		color_32to256(&Pal8[ci + 0x40], P.Pal5.B, P.Pal5.R, P.Pal5.G);
	}
	color_32to256(&Pal8[colours], 0, 0, 0);

	I.image = data_decoded;
	I.start_x = in_x;
	I.start_y = in_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.colors = colours;
	I.Pal8 = Pal8;
	I.Trans = Trans;

	free(data);
	return &I;
}