// MSXtoPNG Intruder専用版
// COLPALET.DATからパレット情報を切り出しておくこと

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"
#include "../aclib/accore.h"
#include "../aclib/acinternal.h"

#define MSX_COLUMNS 512
#define MSX_ROWS 212

const wchar_t* fnpal = L"COLPALET.DAT";

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi, * pFi_pal;

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

		struct image_info* pI = decode_MSX_I(pFi);
		if (pI == NULL) {
			continue;
		}
		wprintf_s(L"Start %3zu/%3zu %3zu*%3zu %s\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y, pI->sType);

		size_t canvas_x = MSX_COLUMNS;
		size_t canvas_y = MSX_ROWS;
		canvas_y = (pI->start_y + pI->len_y) > canvas_y ? (pI->start_y + pI->len_y) : canvas_y;

		png_bytep canvas;
		canvas = malloc(canvas_y * canvas_x);
		if (canvas == NULL) {
			wprintf_s(L"Memory allocation error. \n");
			free(pI->image);
			exit(-2);
		}

		memset(canvas, pI->BGcolor, canvas_y * canvas_x);
		for (size_t iy = 0; iy < pI->len_y; iy++) {
			memcpy_s(&canvas[(pI->start_y + iy) * canvas_x + pI->start_x], pI->len_x, &pI->image[iy * pI->len_x], pI->len_x);
		}
		free(pI->image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");
		if (wcslen(fname) != 7 || !iswdigit(fname[0]) || !iswdigit(fname[1]) || !iswdigit(fname[2]) || !iswalpha(fname[3]) || !iswdigit(fname[4]) || !iswdigit(fname[5]) || !iswdigit(fname[6])) {
			free(canvas);
			wprintf_s(L"Not suitable filename format %s.\n", *argv);
			continue;
		}
		wchar_t path_num[4], *stopscan;
		wcsncpy_s(path_num, 4, fname, 3);
		unsigned long num = wcstoul(path_num, &stopscan, 10);

		ecode = _wfopen_s(&pFi_pal, fnpal, L"rb");
		if (ecode) {
			free(canvas);
			wprintf_s(L"File open error %s.\n", fnpal);
			exit(ecode);
		}
		struct MSX_Palette Pal[100][16];

		size_t rcount = fread_s(Pal, sizeof(Pal), sizeof(Pal), 1, pFi);
		if (rcount != 1) {
			free(canvas);
			wprintf_s(L"File read error %s.\n", fnpal);
			fclose(pFi_pal);
			exit(-2);
		}
		fclose(pFi_pal);

		static png_color Pal8[COLOR16 + 1];

		for (size_t ci = 0; ci < COLOR16; ci++) {
			color_8to256(&Pal8[ci], Pal[num][ci].B, Pal[num][ci].R,Pal[num][ci].G);
		}
		color_8to256(&Pal8[COLOR16], 0, 0, 0);

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = Pal8;
		imgw.Trans = pI->Trans;
		imgw.nPal = pI->colors;
		imgw.nTrans = pI->colors;
		imgw.pXY = 2;

		imgw.image = malloc(canvas_y * sizeof(png_bytep));
		if (imgw.image == NULL) {
			fprintf_s(stderr, "Memory allocation error. \n");
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
