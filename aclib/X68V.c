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
// X68のランス3オプションセット あぶない文化祭前夜で使われている256色フォーマット 但し要素はビッグエンディアンなのでバイトスワップを忘れずに
struct X68V {
	unsigned __int16 U0;
	unsigned __int16 U1;
	unsigned __int16 ColsBE;
	unsigned __int16 RowsBE;
	unsigned __int16 Pal5BE[0x100]; // Palette No. 0x00 to 0xFE 最後の1個は無効?
	unsigned __int8 body[];
};
#pragma pack()
struct image_info* decode_X68V(FILE* pFi)
{
	const size_t edge_len = 512;
	const unsigned colours = COLOR256;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct X68V);
	struct X68V* data = malloc(fs.st_size);
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

	size_t in_x = 96;
	size_t in_y = 56;
	size_t len_x = _byteswap_ushort(data->ColsBE);
	size_t len_y = _byteswap_ushort(data->RowsBE);
	size_t len_decoded = len_x * len_y;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	size_t count = len;
	__int64 cp_len;
	unsigned __int8* src = data->body, * dst = data_decoded, prev = ~*src, repeat = 0;
	while (count-- && (dst - data_decoded) < len_decoded) {
		if (repeat) {
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

	static struct image_info I;
	static wchar_t sType[] = L"X68V";
	static png_color Pal8[COLOR256];
	static png_byte Trans[COLOR256];

	memset(Trans, 0xFF, sizeof(Trans));

	union X68Pal_conv {
		struct X68_Palette5 Pal5;
		unsigned __int16 Pin;
	} P;
	for (size_t ci = 0; ci < colours; ci++) {
		P.Pin = _byteswap_ushort(data->Pal5BE[ci]);
		color_32to256(&Pal8[ci], P.Pal5.B, P.Pal5.R, P.Pal5.G);
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