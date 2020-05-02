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
static struct X68V {
	unsigned __int16 U0;
	unsigned __int16 U1;
	unsigned __int16 ColsBE;
	unsigned __int16 RowsBE;
	unsigned __int16 Pal5BE[0x100]; // Palette No. 0x00 to 0xFE 最後の1個は無効?
	unsigned __int8 body[];
};

// VSP 256色フォーマット
static struct VSP256 {
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

// プレーンデータをパックトピクセルへ変換後にバイトスワップする処理の可読性を上げるための共用体
static union {
	__int64 a;
	__int8 a8[8];
} u;
#pragma pack()

// デコードされたプレーンデータをパックトピクセルに変換(4プレーン版)
unsigned __int8 convert_8dot_from_4plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	unsigned __int8 max_ccode = 0;
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0) | ((*(src + col * 3) & (1 << x)) ? 8 : 0);
				if (max_ccode < u.a8[x]) {
					max_ccode = u.a8[x];
				}
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 3;
	}
	return max_ccode + 1;
}

// デコードされたプレーンデータをパックトピクセルに変換(3プレーン版)
unsigned __int8 convert_8dot_from_3plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	unsigned __int8 max_ccode = 0;
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0);
				if (max_ccode < u.a8[x]) {
					max_ccode = u.a8[x];
				}
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 2;
	}
	return max_ccode + 1;
}

