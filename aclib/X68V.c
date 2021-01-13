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
	struct X68V* data = GC_malloc(fs.st_size);
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

	// スタートアドレスの決め方が不明なので決め打ちにした。
	const size_t in_x = 96;
	const size_t in_y = 56;
	const size_t len_x = _byteswap_ushort(data->ColsBE);
	const size_t len_y = _byteswap_ushort(data->RowsBE);
	const size_t len_decoded = len_x * len_y;
	unsigned __int8* data_decoded = GC_malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
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

	static const wchar_t sType[] = L"X68V";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * colours);
	png_bytep Trans = GC_malloc(sizeof(png_byte) * colours);
	memset(Trans, 0xFF, sizeof(png_byte) * colours);

	for (size_t ci = 0; ci < colours; ci++) {
		union X68Pal_conv {
			struct X68_Palette5 Pal5;
			unsigned __int16 Pin;
		} P;
		P.Pin = _byteswap_ushort(data->Pal5BE[ci]);

		struct fPal8 inPal5;
		inPal5.R = P.Pal5.R;
		inPal5.G = P.Pal5.G;
		inPal5.B = P.Pal5.B;
		color_32to256(Pal8 + ci, &inPal5);
	}

	struct image_info* pI = GC_malloc(sizeof(struct image_info));
	pI->image = data_decoded;
	pI->start_x = in_x;
	pI->start_y = in_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->colors = colours;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = 0;

	return pI;
}