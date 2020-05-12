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
// プレーンデータをパックトピクセルへ変換後にバイトスワップする処理の可読性を上げるための共用体
static union {
	__int64 a;
	__int8 a8[8];
} u;
#pragma pack()

// デコードされたプレーンデータをパックトピクセルに変換(4プレーン版)
void convert_8dot_from_4plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0) | ((*(src + col * 3) & (1 << x)) ? 8 : 0);
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 3;
	}
}

// デコードされたプレーンデータをパックトピクセルに変換(3プレーン版)
void convert_8dot_from_3plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0);
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 2;
	}
}