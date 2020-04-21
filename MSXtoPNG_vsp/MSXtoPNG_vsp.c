#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"

#define BPP 8
#define ROWS 480
#define MSX_ROWS 212


// MSXのフォーマットのパレット情報
struct MSX_Palette {
	unsigned __int16 C0 : 4;
	unsigned __int16 C1 : 4;
	unsigned __int16 C2 : 4;
	unsigned __int16 u0 : 4;
};

// MSXのフォーマットのヘッダ
struct MSXVSP_header {
	unsigned __int16 Column_start; // divided by 2
	unsigned __int16 Row_start;
	unsigned __int16 Columns; // divided by 2
	unsigned __int16 Rows;
	struct MSX_Palette Palette[16];
} hMSXVSP;

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

		unsigned __int8 hbuf[0x40];
		size_t rcount = fread_s(hbuf, sizeof(hbuf), sizeof(hbuf), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		fseek(pFi, 0, SEEK_SET);

		rcount = fread_s(&hMSXVSP, sizeof(hMSXVSP), sizeof(hMSXVSP), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		size_t msx_len = fs.st_size - sizeof(hMSXVSP);
		unsigned __int8* msx_data = malloc(msx_len);
		if (msx_data == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		rcount = fread_s(msx_data, msx_len, 1, msx_len, pFi);
		if (rcount != msx_len) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			free(msx_data);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		const size_t msxvsp_in_col = hMSXVSP.Column_start;
		const size_t msxvsp_in_x = msxvsp_in_col * 2;
		size_t msxvsp_in_y = hMSXVSP.Row_start;
		const size_t msxvsp_len_col = hMSXVSP.Columns;
		const size_t msxvsp_len_x = msxvsp_len_col * 2;
		size_t msxvsp_len_y = hMSXVSP.Rows;
		const size_t msxvsp_out_col = msxvsp_in_col + msxvsp_len_col;
		const size_t msxvsp_out_x = msxvsp_out_col * 2;
		size_t msxvsp_out_y = hMSXVSP.Row_start + msxvsp_len_y;
		size_t msxvsp_len_decoded = msxvsp_len_y * msxvsp_len_col * 2;

		wprintf_s(L"File %s:%3zu/%3zu - %3zu/%3zu VSP size %zu => %zu.\n", *argv, msxvsp_in_x, msxvsp_in_y, msxvsp_out_x, msxvsp_out_y, msx_len, msxvsp_len_decoded);
		unsigned __int8* msx_data_decoded = calloc(msxvsp_len_decoded, sizeof(unsigned __int8));
		if (msx_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(msx_data);
			exit(-2);
		}

		__int64 count = msx_len;
		size_t cp_len;
		unsigned __int8* src = msx_data, * dst = msx_data_decoded;
		while (count-- > 0 && (dst - msx_data_decoded) < msxvsp_len_decoded) {
			switch (*src) {
//			wprintf_s(L"%06zX: %02X %02X %02X.\n", src - msx_data, *src, *(src + 1), *(src + 2));
			case 0x00:
				cp_len = *(src + 1);
				if (cp_len == 0) {
					break;
				}
				memcpy_s(dst, cp_len, dst - msxvsp_len_col * 2, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0x01:
				cp_len = *(src + 1);
				memcpy_s(dst, cp_len, dst - msxvsp_len_col, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0x02:
				cp_len = *(src + 1);
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
				break;
			case 0x03:
				src++;
				*dst++ = *src++;
				break;
			default:
				*dst++ = *src++;
			}
		}

		free(msx_data);

		int ratio_XY = msxvsp_len_decoded / (dst - msx_data_decoded);

		if (ratio_XY == 1) {
			unsigned __int8* msx_data_deinterlaced = malloc(msxvsp_len_decoded);
			for (size_t y = 0; y < msxvsp_len_y; y++) {
				memcpy_s(msx_data_deinterlaced + msxvsp_len_col * y * 2, msxvsp_len_col, msx_data_decoded + msxvsp_len_col * y, msxvsp_len_col);
				memcpy_s(msx_data_deinterlaced + msxvsp_len_col * (y * 2 + 1), msxvsp_len_col, msx_data_decoded + msxvsp_len_col * (msxvsp_len_y + y), msxvsp_len_col);
			}
			free(msx_data_decoded);
			msx_data_decoded = msx_data_deinterlaced;
			msxvsp_in_y *= 2;
			msxvsp_len_y *= 2;
			msxvsp_out_y *= 2;
		}
		else {
			msxvsp_len_decoded /= 2;
		}

		size_t decode_len = msxvsp_len_y * msxvsp_len_x;
		unsigned __int8* decode_buffer = malloc(decode_len);
		if (decode_buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(msx_data_decoded);
			exit(-2);
		}

		for (size_t i = 0; i < msxvsp_len_decoded; i++) {
			decode_buffer[i * 2] = (msx_data_decoded[i] & 0xF0) >> 4;
			decode_buffer[i * 2 + 1] = msx_data_decoded[i] & 0xF;
		}

		free(msx_data_decoded);
		iInfo.image = decode_buffer;
		iInfo.start_x = msxvsp_in_x;
		iInfo.start_y = msxvsp_in_y;
		iInfo.len_x = msxvsp_len_x;
		iInfo.len_y = msxvsp_len_y;
		iInfo.colors = 16;

		size_t canvas_x = 512;
		size_t canvas_y = (ratio_XY == 1) ? 424 : 212;
		unsigned __int8 t_color = 0x10;

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
			memcpy_s(&canvas[(iInfo.start_y + iy) * canvas_x + iInfo.start_x], iInfo.len_x, &iInfo.image[iy * iInfo.len_x], iInfo.len_x);
		}
		free(iInfo.image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		png_color pal[17] = { {0,0,0} };

		for (size_t ci = 0; ci < 16; ci++) {
			pal[ci].blue = (hMSXVSP.Palette[ci].C0 * 0x24) | ((hMSXVSP.Palette[ci].C0 & 4) ? 1 : 0);
			pal[ci].red = (hMSXVSP.Palette[ci].C1 * 0x24) | ((hMSXVSP.Palette[ci].C1 & 4) ? 1 : 0);
			pal[ci].green = (hMSXVSP.Palette[ci].C2 * 0x24) | ((hMSXVSP.Palette[ci].C2 & 4) ? 1 : 0);
		}
		pal[16].blue = pal[16].red = pal[16].green = 0;

		png_byte trans[17] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pal;
		imgw.Trans = trans;
		imgw.nPal = 17;
		imgw.nTrans = 17;
		imgw.pXY = 1;

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
