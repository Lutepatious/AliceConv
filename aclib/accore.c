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

// PMS16で近似する色を圧縮表現している部分から復元する為のビットフィールド構造体
// pragma packをまとめたいのでdecode_d16関数本体から離れたここで定義している
// 下位計8ビット分
struct COLOR_src_low {
	unsigned __int8 Bl : 2;
	unsigned __int8 Gl : 4;
	unsigned __int8 Rl : 2;
};

// 上位計8ビット分
struct COLOR_src_high {
	unsigned __int8 Bh : 3;
	unsigned __int8 Gh : 2;
	unsigned __int8 Rh : 3;
};

// 合成計16ビット分
struct COLOR_dest {
	unsigned __int16 Bl : 2;
	unsigned __int16 Bh : 3;
	unsigned __int16 Gl : 4;
	unsigned __int16 Gh : 2;
	unsigned __int16 Rl : 2;
	unsigned __int16 Rh : 3;
};
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

// VSP256 PMS8 PMS16のアルファchに共通の8bit depthデコーダ
void decode_d8(unsigned __int8 *destination, unsigned __int8 *source, size_t length, size_t Col_per_Row)
{
	size_t cp_len;
	unsigned __int8* src = source, * dst = destination, * cp_src;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0xF8: // 何もしない、エスケープ用
		case 0xF9:
		case 0xFA:
		case 0xFB:
			src++;
			*dst++ = *src++;
			break;
		case 0xFC: // 2,3バイト目の2ピクセルの値を1バイト目で指定した回数繰り返す。
			cp_len = *(src + 1) + 3LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 4;
			break;
		case 0xFD: // 2バイト目の1ピクセルの値を1バイト目で指定した回数繰り返す。
			cp_len = *(src + 1) + 4LL;
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			break;
		case 0xFE: // 2行前の値を1バイト目で指定した長さでコピー
			cp_len = *(src + 1) + 3LL;
			cp_src = dst - Col_per_Row * 2;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		case 0xFF: // 1行前の値を1バイト目で指定した長さでコピー
			cp_len = *(src + 1) + 3LL;
			cp_src = dst - Col_per_Row;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		default:
			*dst++ = *src++;
		}
	}

}

// PMS16の16bit depthデコーダ 入力が8bit幅で出力が16bit幅であることに注意
void decode_d16(unsigned __int16* destination, unsigned __int8* source, size_t length, size_t Col_per_Row)
{
	size_t cp_len;
	unsigned __int8* src = source;
	unsigned __int16* dst = destination, * cp_src;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0xF8: // 何もしない、エスケープ用
			src++;
			*dst++ = *(unsigned __int16*)src;
			src += 2;
			break;
		case 0xF9: // 連続する近似した色を圧縮、1バイト目に長さ、2バイト目が各色上位ビット(B3G2R3)、3バイト目以降が各色下位ビット(B2G4R2)。
			cp_len = *(src + 1) + 1LL;
			// 下3つの共用体は処理系によってはエラーになるのでswitch文の外に出した方が良さそうだが可読性を上げるためにここに放置
			static union {
				struct COLOR_src_high h;
				unsigned __int8 a8;
			} uh;
			static union {
				struct COLOR_src_low l;
				unsigned __int8 a8;
			} ul;
			static union {
				struct COLOR_dest d;
				unsigned __int16 a16;
			} ud;

			src += 2;
			uh.a8 = *src++;
			ud.d.Rh = uh.h.Rh;
			ud.d.Gh = uh.h.Gh;
			ud.d.Bh = uh.h.Bh;
			for (size_t len = 0; len < cp_len; len++) {
				ul.a8 = *src++;
				ud.d.Rl = ul.l.Rl;
				ud.d.Gl = ul.l.Gl;
				ud.d.Bl = ul.l.Bl;
				*dst++ = ud.a16;
			}
			break;
		case 0xFA: // 1行と1ピクセル後の値をコピー
			*dst = *(dst - Col_per_Row + 1);
			dst++;
			src++;
			break;
		case 0xFB: // 1行と1ピクセル前の値をコピー
			*dst = *(dst - Col_per_Row - 1);
			dst++;
			src++;
			break;
		case 0xFC: // 2-5バイト目の2ピクセルの値を1バイト目で指定した回数繰り返す。
			cp_len = *(src + 1) + 2LL;
			for (size_t len = 0; len < cp_len; len++) {
				wmemcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 6;
			break;
		case 0xFD: // 2,3バイト目の1ピクセルの値を1バイト目で指定した回数繰り返す。
			cp_len = *(src + 1) + 3LL;
			wmemset(dst, *(unsigned __int16 *)(src + 2), cp_len);
			dst += cp_len;
			src += 4;
			break;
		case 0xFE: // 2行前の値を1バイト目で指定した長さでコピー
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - Col_per_Row * 2;
			wmemcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		case 0xFF: // 1行前の値を1バイト目で指定した長さでコピー
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - Col_per_Row;
			wmemcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		default:
			*dst++ = *(unsigned __int16*)src;
			src += 2;
		}
	}
}