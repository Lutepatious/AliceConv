#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pngio.h"
#include "accore.h"

// コンパイラオプションで構造体に隙間ができないよう、pragma packで詰めることを指定
#pragma pack (1)
// PC88系のGLフォーマットのパレット情報
static struct GL_Palette3 {
	unsigned __int16 B : 3;
	unsigned __int16 R : 3;
	unsigned __int16 u0 : 2;
	unsigned __int16 G : 3;
	unsigned __int16 u1 : 3;
	unsigned __int16 u2 : 2;
};

// X68000フォーマットのパレット情報
static struct X68_Palette5 {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
};

// GLフォーマット
static struct GL {
	unsigned __int16 Start;
	unsigned __int8 Columns; // divided by 8
	unsigned __int8 Rows;
	unsigned __int16 U0;
	unsigned __int16 U1;
	struct GL_Palette3 Pal3[8];
	unsigned __int8 body[];
};

// GL2/GL3 フォーマット
static struct GL3 {
	struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16];				// もしここが空っぽならFM TOWNS版ALICEの館CDに収録されたIntruderだと思われる
	unsigned __int16 Start;
	unsigned __int16 Columns; // divided by 8
	unsigned __int16 Rows;
	unsigned __int8 body[];
};

// X68のRanceIIで使われている16色フォーマット 実態はパックトピクセル版のGL
static struct X68R {
	unsigned __int16 Sig;
	struct {
		unsigned __int8 R;
		unsigned __int8 G;
		unsigned __int8 B;
	} Pal4[16];
	unsigned __int16 Column_start; // divided by 2
	unsigned __int16 Row_start;
	unsigned __int16 Column_len; // divided by 2
	unsigned __int16 Row_len;
	unsigned __int16 U0;
	unsigned __int8 body[];
};

// VSP 200ラインフォーマット
static struct VSP200l {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct GL_Palette3 Pal3[8];
	unsigned __int8 body[];
};

// VSP 16色フォーマット
static struct VSP {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16];
	unsigned __int8 body[];
};

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

// プレーンデータをパックトピクセルに変換後にバイトスワップする処理の可読性を上げるための共用体
static union {
	__int64 a;
	__int8 a8[8];
} u;
#pragma pack()

// デコードされたプレーンデータをパックトピクセルに変換(4プレーン版)
static unsigned __int8 convert_8dot_from_4plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
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
static unsigned __int8 convert_8dot_from_3plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
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

#define COLOR8 (8)
#define COLOR16 (16)

struct image_info* decode_GL(FILE* pFi)
{
	const size_t planes = 3;
	const size_t depth = 8;
	const unsigned colours = COLOR8;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	size_t len = fs.st_size - sizeof(struct GL);
	struct GL* data = malloc(fs.st_size);
	if (data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	size_t rcount = fread_s(data, fs.st_size, 1, fs.st_size, pFi);
	if (rcount != fs.st_size) {
		wprintf_s(L"File read error.\n");
		free(data);
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	size_t start = _byteswap_ushort(data->Start) & 0x3FFF;
	size_t start_x = start % 80 * 8;
	size_t start_y = start / 80;
	size_t len_decoded = data->Rows * data->Columns * planes;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}
	size_t count = len, cp_len, cur_plane;
	unsigned __int8* src = data->body, * dst = data_decoded, * cp_src;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0x00:
			cp_len = *(src + 1) & 0x7F;
			if (*(src + 1) & 0x80) {
				cp_src = dst - data->Columns * planes * 4 + 1;
				memset(dst, *cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
			}
			else {
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
			}
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
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D:
		case 0x0E:
			cp_len = *src - 6;
			cp_src = dst - data->Columns * planes * 4 + 1;
			memset(dst, *cp_src, cp_len);
			dst += cp_len;
			src++;
			break;
		case 0x0F:
			cur_plane = ((dst - data_decoded) / data->Columns) % planes;
			cp_len = *(src + 1) & 0x7F;
			cp_src = dst - data->Columns * ((*(src + 1) & 0x80) ? cur_plane - 1 : cur_plane);
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;

		default:
			*dst++ = *src++;
		}
	}

	size_t decode_len = data->Rows * data->Columns * depth;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	convert_8dot_from_3plane_to_8bitpackedpixel(decode_buffer, data_decoded, data->Columns, data->Rows);
	free(data_decoded);

	static struct image_info I;
	static png_color Pal8[COLOR8 + 1];
	static png_byte Trans[COLOR8 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		color_8to256(&Pal8[ci], data->Pal3[ci].B, data->Pal3[ci].R, data->Pal3[ci].G);
	}
	color_8to256(&Pal8[colours], 0, 0, 0);

	I.image = decode_buffer;
	I.start_x = start_x;
	I.start_y = start_y;
	I.len_x = data->Columns * 8LL;
	I.len_y = data->Rows;
	I.colors = colours + 1;
	I.Pal8 = Pal8;
	I.Trans = Trans;

	free(data);
	return &I;
}

struct image_info* decode_GL3(FILE* pFi, int isGM3)
{
	const size_t planes = 4;
	const size_t depth = 8;
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	size_t len = fs.st_size - sizeof(struct GL3);
	struct GL3* data = malloc(fs.st_size);
	if (data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	size_t rcount = fread_s(data, fs.st_size, 1, fs.st_size, pFi);
	if (rcount != fs.st_size) {
		wprintf_s(L"File read error.\n");
		free(data);
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	size_t start = data->Start & 0x7FFF;
	size_t start_x = start % 80 * 8;
	size_t start_y = start / 80;
	size_t len_decoded = data->Rows * data->Columns * planes;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}
	size_t count = len, cp_len, cur_plane;
	unsigned __int8* src = data->body, * dst = data_decoded, * cp_src;
	while (count-- && (dst - data_decoded) < len_decoded) {
		switch (*src) {
		case 0x00:
			cp_len = *(src + 1) & 0x7F;
			if (*(src + 1) & 0x80) {
				cp_src = dst - data->Columns * planes * 2 + 1;
				memset(dst, *cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
			}
			else {
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
			}
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
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D:
			cp_len = *src - 6;
			cp_src = dst - data->Columns * planes * 2 + 1;
			memset(dst, *cp_src, cp_len);
			dst += cp_len;
			src++;
			break;
		case 0x0E:
			cur_plane = ((dst - data_decoded) / data->Columns) % planes;
			cp_len = *(src + 1) & 0x7F;
			cp_src = dst - data->Columns * (cur_plane - 2);
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;
		case 0x0F:
			cur_plane = ((dst - data_decoded) / data->Columns) % planes;
			cp_len = *(src + 1) & 0x7F;
			cp_src = dst - data->Columns * ((*(src + 1) & 0x80) ? cur_plane - 1 : cur_plane);
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			count--;
			break;

		default:
			*dst++ = *src++;
		}
	}


	size_t decode_len = data->Rows * data->Columns * depth;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	convert_8dot_from_4plane_to_8bitpackedpixel(decode_buffer, data_decoded, data->Columns, data->Rows);
	free(data_decoded);

	static struct image_info I;
	static png_color Pal8[COLOR16];
	static png_byte Trans[COLOR16];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[0] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		if (isGM3 == 1) {
//			GM3フォーマットのためのパレット情報
			static const 	struct {
				unsigned __int8 B;
				unsigned __int8 R;
				unsigned __int8 G;
			} Pal4[16] = { { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
							{ 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
							{ 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
							{ 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF } };
			color_16to256(&Pal8[ci], Pal4[ci].B, Pal4[ci].R, Pal4[ci].G);
		}
		else {
			color_16to256(&Pal8[ci], data->Pal4[ci].B, data->Pal4[ci].R, data->Pal4[ci].G);
		}
	}

	I.image = decode_buffer;
	I.start_x = start_x;
	I.start_y = start_y;
	I.len_x = data->Columns * 8LL;
	I.len_y = data->Rows;
	I.colors = colours;
	I.Pal8 = Pal8;
	I.Trans = Trans;

	free(data);
	return &I;
}