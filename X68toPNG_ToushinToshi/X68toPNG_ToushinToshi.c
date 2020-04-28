#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"

#pragma pack (1)
union  X68_palette {
	struct {
		unsigned __int16 I : 1;
		unsigned __int16 B : 5;
		unsigned __int16 R : 5;
		unsigned __int16 G : 5;
	} Pal;
	unsigned __int16 Pin;
} P;

struct X68V_header {
	unsigned __int32 Sig;
	unsigned __int32 U0;
	unsigned __int16 U1;
	unsigned __int16 U2;
	unsigned __int16 Pal[0xC0]; // Palette No. 0x40 to 0xFF
	unsigned __int16 U3;
	unsigned __int16 Start;
	unsigned __int16 Cols;
	unsigned __int16 Rows;
} hX68V;

#define X68_LEN 512

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

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi;

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

		size_t rcount = fread_s(&hX68V, sizeof(hX68V), sizeof(hX68V), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}
		size_t x68_len = fs.st_size - sizeof(hX68V);


		unsigned __int8* x68_data = malloc(x68_len);
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

		size_t x68_in_x = (_byteswap_ushort(hX68V.Start / 2) % X68_LEN);
		size_t x68_in_y = (_byteswap_ushort(hX68V.Start / 2) / X68_LEN);
		size_t x68_len_x = _byteswap_ushort(hX68V.Cols);
		size_t x68_len_y = _byteswap_ushort(hX68V.Rows);
		size_t x68_len_decoded = x68_len_x * x68_len_y;
		unsigned __int8* x68_data_decoded = malloc(x68_len_decoded);
		if (x68_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(x68_data);
			exit(-2);
		}
		wprintf_s(L"Start %3zu/%3zu (%3zu/%3zu) X68 size %zu => %zu.\n", x68_in_x, x68_in_y, x68_len_x, x68_len_y, x68_len, x68_len_decoded);

		__int64 count = x68_len;
		__int64 cp_len;

		unsigned __int8* src = x68_data, * dst = x68_data_decoded, prev = *src;
		while (count-- > 0 && (dst - x68_data_decoded) < x68_len_decoded) {
			if (*(unsigned __int32 *)src == 0xFFFFFFFFUL) {
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
//		wprintf_s(L"%06I64X: %6I64d %6I64u\n", src - x68_data + sizeof(hX68V), count, dst - x68_data_decoded);


		free(x68_data);

		iInfo.image = x68_data_decoded;
		iInfo.start_x = x68_in_x;
		iInfo.start_y = x68_in_y;
		iInfo.len_x = x68_len_x;
		iInfo.len_y = x68_len_y;
		iInfo.colors = 256;

		size_t canvas_x = X68_LEN;
		size_t canvas_y = X68_LEN;
		unsigned __int8 t_color = 0;

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

		png_color pal[256] = { {0,0,0} };

		for (size_t ci = 0; ci < 192; ci++) {
			P.Pin = _byteswap_ushort(hX68V.Pal[ci]);
			//			wprintf_s(L"%3d %3d %3d.\n", P.Pal.B, P.Pal.R, P.Pal.G);
			color_32to256(&pal[ci + 0x40], P.Pal.B, P.Pal.R, P.Pal.G);
		}

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pal;
		imgw.Trans = NULL;
		imgw.nPal = iInfo.colors;
		imgw.nTrans = iInfo.colors;
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
