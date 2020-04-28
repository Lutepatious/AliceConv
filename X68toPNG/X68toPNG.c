#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"

#pragma pack (1)
struct X68_palette {
	unsigned __int8 C0;
	unsigned __int8 C1;
	unsigned __int8 C2;
};

struct X68_header {
	unsigned __int16 Sig;
	struct X68_palette Pal[16];
	unsigned __int16 Column_start; // divided by 2
	unsigned __int16 Row_start;
	unsigned __int16 Column_len; // divided by 2
	unsigned __int16 Row_len;
	unsigned __int16 U0;
} hX68;

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

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		size_t rcount = fread_s(&hX68, sizeof(hX68), sizeof(hX68), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		size_t x68_len = fs.st_size - sizeof(hX68);
		unsigned __int8 *x68_data = malloc(x68_len);
		if (x68_data == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		rcount = fread_s(x68_data, x68_len, 1, x68_len, pFi);
		if (rcount != x68_len) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			free(x68_data);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		size_t x68_in_col = hX68.Column_start;
		size_t x68_in_x = x68_in_col * 2L;
		size_t x68_in_y = hX68.Row_start;
		size_t x68_len_col = hX68.Column_len;
		size_t x68_len_x = x68_len_col * 2L;
		size_t x68_len_y = hX68.Row_len;
		size_t x68_len_decoded = x68_len_col * x68_len_y;
		unsigned __int8 *x68_data_decoded = malloc(x68_len_decoded);
		if (x68_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(x68_data);
			exit(-2);
		}
		wprintf_s(L"Start %3zu/%3zu (%3zu/%3zu) X68 size %zu => %zu.\n", x68_in_x, x68_in_y, x68_len_x, x68_len_y, x68_len , x68_len_decoded);

		__int64 count = x68_len;
		size_t cp_len;

		unsigned __int8 *src = x68_data, *dst = x68_data_decoded;
		while (count-- > 0 && (dst - x68_data_decoded) < x68_len_decoded) {
			//			wprintf_s(L"%04X.\n", src - x68_data + sizeof(hx68));
			switch (*src) {
			case 0xFE:
				cp_len = *(src + 1);
				for (size_t len = 0; len < cp_len; len++) {
					memcpy_s(dst, 2, src + 2, 2);
					dst += 2;
				}
				src += 4;
				count -= 3;
				break;
			case 0xFF:
				cp_len = *(src + 1);
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
				break;
			default:
				*dst++ = *src++;
			}
		}

		free(x68_data);

		size_t decode_len = x68_len_decoded * 2;
		unsigned __int8* decode_buffer = malloc(decode_len);
		if (decode_buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(x68_data_decoded);
			exit(-2);
		}

		for (size_t i = 0; i < x68_len_decoded; i++) {
			decode_buffer[i * 2] = x68_data_decoded[i] & 0xF;
			decode_buffer[i * 2 + 1] = (x68_data_decoded[i] & 0xF0) >> 4;
		}

		free(x68_data_decoded);
		iInfo.image = decode_buffer;
		iInfo.start_x = x68_in_x;
		iInfo.start_y = x68_in_y;
		iInfo.len_x = x68_len_x;
		iInfo.len_y = x68_len_y;
		iInfo.colors = 16;

		size_t canvas_x = 640;
		size_t canvas_y = 400;
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

		for (size_t ci = 0; ci < iInfo.colors; ci++) {
			color_16to256(&pal[ci], hX68.Pal[ci].C2, hX68.Pal[ci].C0, hX68.Pal[ci].C1);
		}
		color_16to256(&pal[iInfo.colors], 0, 0, 0);

		png_byte trans[17] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pal;
		imgw.Trans = trans;
		imgw.nPal = iInfo.colors + 1;
		imgw.nTrans = iInfo.colors + 1;
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
