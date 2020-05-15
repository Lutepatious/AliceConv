#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"
#include "../aclib/tiffrw.h"
#include "../aclib/accore.h"


const static png_byte table2to8[4] = { 0, 0x7F, 0xBF, 0xFF };
const static png_byte table3to8[8] = { 0, 0x3F, 0x5F, 0x7F, 0x9F, 0xBF, 0xDF, 0xFF };

#pragma pack(1)
struct color8 {
	unsigned __int8 B : 2;
	unsigned __int8 R : 3;
	unsigned __int8 G : 3;
};
#pragma pack()

int wmain(int argc, wchar_t** argv)
{
	while (--argc) {
		struct fTIFF* pimg = tiff_read(*++argv);
		if (!pimg) {
			wprintf_s(L"TIFF read error.\n");
			continue;
		}

		static png_color Pal8[256];
		if (pimg->Format == PHOTOMETRIC_PALETTE) {
			for (size_t i = 0; i < 256; i++) {
				struct fPal16 Pal16;
				Pal16.R = pimg->Pal.R[i];
				Pal16.G = pimg->Pal.G[i];
				Pal16.B = pimg->Pal.B[i];
				color_65536to256(&Pal8[i], &Pal16);
			}
		}
		else if (pimg->Format == PHOTOMETRIC_MINISBLACK) {
			for (size_t i = 0; i < 256; i++) {
				union {
					unsigned __int8 a;
					struct color8 c;
				} u;
				u.a = i;
				struct fPal8 iPal8;
				iPal8.R = table3to8[u.c.R];
				iPal8.G = table3to8[u.c.G];
				iPal8.B = table2to8[u.c.B];
				color_256to256(&Pal8[i], &iPal8);
			}

		}
		else {
			wprintf_s(L"Unexpected format.\n");
			continue;
		}

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = pimg->Cols;
		imgw.Rows = pimg->Rows;
		imgw.Pal = Pal8;
		imgw.Trans = NULL;
		imgw.nPal = 1LL << pimg->depth;
		imgw.pXY = 1;

		imgw.image = malloc(imgw.Rows * sizeof(png_bytep));
		if (imgw.image == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			free(pimg->image);
			free(pimg);
			exit(-2);
		}

		for (size_t j = 0; j < imgw.Rows; j++)
			imgw.image[j] = (png_bytep)&pimg->image[j * imgw.Cols];

		void* res = png_create(&imgw);
		if (res == NULL) {
			wprintf_s(L"File %s create/write error\n", path);
		}

		free(imgw.image);
		free(pimg->image);
		free(pimg);
	}
}