#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../aclib/pngio.h"

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		wchar_t path_odd[_MAX_PATH];
		wchar_t path_even[_MAX_PATH];
		wchar_t path[_MAX_PATH];
		wchar_t outpath[_MAX_PATH];
		wchar_t fname_odd[_MAX_FNAME];
		wchar_t fname_even[_MAX_FNAME];
		wchar_t fname[_MAX_FNAME];
		wchar_t outname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*++argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		if (wcslen(fname) != 8) {
			wprintf_s(L"Unexpected filename %s.\n", *argv);
			continue;
		}

		wchar_t path_a = fname[3], * stopscan;
		unsigned long num1, num2;
		unsigned long num1_odd, num2_odd;
		unsigned long num1_even, num2_even;

		num1 = wcstoul(fname, &stopscan, 10);
		num2 = wcstoul(stopscan + 1, &stopscan, 10);

		if (num1 & 1) {
			num1_odd = num1;
			num2_odd = num2;
			num1_even = num1 + 1;
			num2_even = num2 + 1;
			swprintf(outname, _MAX_FNAME, L"%03ld-%03ld", num1_odd, num1_even);
		}
		else {
			num1_odd = num1 - 1;
			num2_odd = num2 - 1;
			num1_even = num1;
			num2_even = num2;
			swprintf(outname, _MAX_FNAME, L"%03ld-%03ld", num1_even, num1_odd);
		}

		swprintf(fname_odd, _MAX_FNAME, L"%03ld%c%03ld", num1_odd, path_a, num2_odd);
		swprintf(fname_even, _MAX_FNAME, L"%03ld%c%03ld", num1_even, path_a, num2_even);
		_wmakepath_s(path_odd, _MAX_PATH, drive, dir, fname_odd, L".png");
		_wmakepath_s(path_even, _MAX_PATH, drive, dir, fname_even, L".png");
		_wmakepath_s(outpath, _MAX_PATH, drive, dir, outname, L".png");

		//		wprintf_s(L"Filename %s.\n", path);

		struct fPNG* pimg_odd = png_open(path_odd);
		struct fPNG* pimg_even = png_open(path_even);

		if (!pimg_odd || !pimg_even) {
			wprintf_s(L"PNG read error.\n");
			continue;
		}
		//		wprintf_s(L"%s: %3zu %3zu %3d %3d.\n", path_odd, pimg_odd->Cols, pimg_odd->Rows, pimg_odd->nPal, pimg_odd->nTrans);
		//		wprintf_s(L"%s: %3zu %3zu %3d %3d.\n", path_even, pimg_even->Cols, pimg_even->Rows, pimg_even->nPal, pimg_even->nTrans);

		int c_mismatch = 0;
		if (pimg_odd->Cols != pimg_even->Cols || pimg_odd->Rows != pimg_even->Rows || pimg_odd->nPal != pimg_even->nPal || pimg_odd->nTrans != pimg_even->nTrans || pimg_odd->depth != pimg_even->depth) {
			wprintf_s(L"Basic image infomations do not match.\n");
			c_mismatch = 1;
		}

		for (size_t i = 0; i < pimg_odd->nPal; i++) {
			if ((pimg_odd->Pal)[i].blue != (pimg_even->Pal)[i].blue || (pimg_odd->Pal)[i].red != (pimg_even->Pal)[i].red || (pimg_odd->Pal)[i].green != (pimg_even->Pal)[i].green) {
				wprintf_s(L"Palettes do not match.\n");
				c_mismatch = 1;
				break;
			}
		}
		for (size_t i = 0; i < pimg_odd->nTrans; i++) {
			if (*(pimg_odd->Trans + i) != *(pimg_even->Trans + i)) {
				wprintf_s(L"Transparents do not match.\n");
				c_mismatch = 1;
				break;
			}
		}

		if (c_mismatch) {
			png_close(pimg_odd);
			png_close(pimg_even);
			continue;
		}

		struct fPNGw imgw;
		imgw.outfile = outname;
		imgw.depth = pimg_odd->depth;
		imgw.image = malloc(sizeof(png_bytep) * pimg_odd->Rows * 2);
		imgw.Cols = pimg_odd->Cols;
		imgw.Rows = pimg_odd->Rows * 2;
		imgw.Pal = pimg_odd->Pal;
		imgw.Trans = pimg_odd->Trans;
		imgw.nPal = pimg_odd->nPal;
		imgw.nTrans = pimg_odd->nTrans;
		imgw.pXY = 1;

		for (size_t i = 0 ; i < pimg_odd->Rows ; i++) {
			*(imgw.image + i * 2) = *(pimg_odd->image + i);
			*(imgw.image + i * 2 + 1) = *(pimg_even->image + i);
		}
		void *res = png_create(&imgw);

		png_close(pimg_odd);
		png_close(pimg_even);
		if (res == NULL) {
			wprintf_s(L"File %s create/write error\n", path);
		}
		free(imgw.image);
	}
}