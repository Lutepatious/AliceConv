#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"
#include "../aclib/accore.h"

#define MSX_COLUMNS 512
#define MSX_ROWS 212

#pragma pack(1)
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

		struct image_info* pI = decode_MSX_TT(pFi);
		if (pI == NULL) {
			continue;
		}
		wprintf_s(L"Start %3zu/%3zu %3zu*%3zu %s\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y, pI->sType);
	
		unsigned pXY = (pI->len_x == MSX_COLUMNS && pI->len_y < MSX_ROWS) ? 2 : 1;

		size_t canvas_x = MSX_COLUMNS;
		size_t canvas_y = (pXY == 2) ? MSX_ROWS : MSX_ROWS * 2;
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

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pI->Pal8;
		imgw.Trans = pI->Trans;
		imgw.nPal = pI->colors;
		imgw.nTrans = pI->colors;
		imgw.pXY = pXY;

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
