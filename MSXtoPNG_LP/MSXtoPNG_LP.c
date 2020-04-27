// MSXtoPNG Little Vampire専用版

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"

#define MSX_COLS 512
#define MSX_ROWS 212

#pragma pack(1)
// MSXのフォーマットのパレット情報
struct MSX_Palette {
	unsigned __int16 C0 : 4;
	unsigned __int16 C1 : 4;
	unsigned __int16 C2 : 4;
	unsigned __int16 u0 : 4;
} Pal[8] = { { 0x0, 0x0, 0x0 }, { 0x0, 0x0, 0x7 }, { 0x0, 0x7, 0x0 }, { 0x0, 0x7, 0x7 },
				{ 0x7, 0x0, 0x0 }, { 0x7, 0x0, 0x7 }, { 0x7, 0x7, 0x0 }, { 0x7, 0x7, 0x7 } };

// MSXのフォーマットのヘッダ
struct MSXo_header {
	unsigned __int8 M1;
	unsigned __int8 NegCols; // ~(Cols - 1)
	unsigned __int8 Rows;
} hMSXo;
#pragma pack()

// イメージ情報保管用構造体
struct image_info {
	unsigned __int8* image;
	unsigned __int8(*Palette)[256][3];
	size_t start_x;
	size_t start_y;
	size_t len_x;
	size_t len_y;
	unsigned colors;
} iInfo;

enum fmt_cg { NONE, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS, PMS16, QNT, MSX };

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		enum fmt_cg g_fmt = NONE;

		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		size_t msx_len = fs.st_size;
		unsigned __int8* msx_data = malloc(msx_len);
		if (msx_data == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(msx_data, msx_len, 1, msx_len, pFi);
		if (rcount != msx_len) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			free(msx_data);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		size_t msx_len_x = 256;
		size_t msx_len_y = 212;
		size_t msx_len_decoded = msx_len_y * msx_len_x;
		unsigned __int8* msx_data_decoded = calloc(msx_len_decoded, sizeof(unsigned __int8));
		if (msx_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(msx_data);
			exit(-2);
		}
		wprintf_s(L"%s: MSX size %zu => %zu.\n", *argv, msx_len, msx_len_decoded);
		__int64 count = msx_len, cp_len;
		size_t rows_real = 0;
		unsigned __int8* src = msx_data, * dst = msx_data_decoded, * cp_src, prev = ~*src, repeat = 0;

		while (count-- > 0 && (dst - msx_data_decoded) < msx_len_decoded) {
			if ((src - msx_data) % 0x100 == 0) {
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
		free(msx_data);

		size_t lines = (dst - msx_data_decoded) / 0x100;
		size_t x_offset = 0;
		if (lines < 150) {
			msx_len_y = 140;
			if (*(unsigned __int32*)msx_data_decoded == 0x00L) {
				x_offset = 8;
			}
		}
		else if (lines < 190) {
			msx_len_y = 180;
		}
		else if (lines < 200) {
			msx_len_y = 192;
		}
		else {
			msx_len_y = 200;
		}
		msx_len_decoded = msx_len_y * msx_len_x;

		size_t decode_len = msx_len_y * msx_len_x * 2;
		unsigned __int8* decode_buffer = malloc(decode_len);
		if (decode_buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(msx_data_decoded);
			exit(-2);
		}

		for (size_t i = 0; i < msx_len_decoded; i++) {
			decode_buffer[i * 2] = (msx_data_decoded[i] & 0xF0) >> 4;
			decode_buffer[i * 2 + 1] = msx_data_decoded[i] & 0xF;
		}

		free(msx_data_decoded);
		iInfo.image = decode_buffer;
		iInfo.start_x = 0;
		iInfo.start_y = 0;
		iInfo.len_x = msx_len_x * 2;
		iInfo.len_y = msx_len_y;
		iInfo.colors = 8;

		size_t canvas_x = (msx_len_y == 140) ? 352 : iInfo.len_x;
		size_t canvas_y = iInfo.len_y;
		unsigned __int8 t_color = 8;

		canvas_y = (iInfo.start_y + iInfo.len_y) > canvas_y ? (iInfo.start_y + iInfo.len_y) : canvas_y;


		unsigned __int8* canvas;
		canvas = malloc(canvas_y * canvas_x);
		if (canvas == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(iInfo.image);
			exit(-2);
		}

		memset(canvas, t_color, canvas_y * canvas_x);
		for (size_t iy = 0; iy < iInfo.len_y; iy++) {
			memcpy_s(&canvas[iy * canvas_x], canvas_x, &iInfo.image[iy * iInfo.len_x + x_offset], canvas_x);
		}
		free(iInfo.image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		png_color pal[9] = { {0,0,0} };

		for (size_t ci = 0; ci < iInfo.colors; ci++) {
			color_8to256(&pal[ci], Pal[ci].C0, Pal[ci].C1, Pal[ci].C2);
		}
		color_8to256(&pal[iInfo.colors], 0, 0, 0);


		png_byte trans[9] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pal;
		imgw.Trans = trans;
		imgw.nPal = iInfo.colors + 1;
		imgw.nTrans = iInfo.colors + 1;
		imgw.pXY = 2;

		imgw.image = malloc(canvas_y * sizeof(png_bytep));
		if (imgw.image == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			free(canvas);
			exit(-2);
		}
		for (size_t j = 0; j < canvas_y; j++)
			imgw.image[j] = (png_bytep)&canvas[j * canvas_x];

		void* res = png_create(&imgw);
		if (res == NULL) {
			wprintf_s(L"File %s create/write error\n", path);
		}

		free(imgw.image);
		free(canvas);
	}
}
