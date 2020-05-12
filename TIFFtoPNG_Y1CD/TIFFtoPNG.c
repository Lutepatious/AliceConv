#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tiffio.h"

#include "../aclib/pngio.h"
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
		TIFF* pTi = TIFFOpenW(*++argv, "r");

		if (pTi == NULL) {
			wprintf_s(L"File open error. %s\n", *argv);
			TIFFClose(pTi);
			exit(-2);
		}

		unsigned __int32 len_x, len_y, Rows_per_Strip;
		unsigned __int16 Bits_per_Sample, metric;
		TIFFGetField(pTi, TIFFTAG_IMAGEWIDTH, &len_x);
		TIFFGetField(pTi, TIFFTAG_IMAGELENGTH, &len_y);
		TIFFGetFieldDefaulted(pTi, TIFFTAG_ROWSPERSTRIP, &Rows_per_Strip);
		TIFFGetField(pTi, TIFFTAG_BITSPERSAMPLE, &Bits_per_Sample);
		TIFFGetField(pTi, TIFFTAG_PHOTOMETRIC, &metric);

		size_t colours = 1LL << Bits_per_Sample;
		static png_color Pal8[256];
		if (metric == PHOTOMETRIC_PALETTE) {
			unsigned __int16* PalR, * PalG, * PalB;
			TIFFGetField(pTi, TIFFTAG_COLORMAP, &PalR, &PalG, &PalB);
			unsigned __int32 PalDepth = 8;
			for (size_t i = 0; i < colours; i++) {
				if (PalR[i] >= 256 || PalG[i] >= 256 || PalB[i] >= 256) {
					PalDepth = 16;
					break;
				}
			}
			if (PalDepth == 16) {
				for (size_t i = 0; i < colours; i++) {
					color_65536to256(&Pal8[i], PalB[i], PalR[i], PalG[i]);
				}
			}
			else {
				for (size_t i = 0; i < colours; i++) {
					color_256to256(&Pal8[i], PalB[i], PalR[i], PalG[i]);
				}
			}
		}
		else if (metric == PHOTOMETRIC_MINISBLACK) {
			for (size_t i = 0; i < colours; i++) {
				union {
					unsigned __int8 a;
					struct color8 c;
				} u;
				u.a = i;
				color_256to256(&Pal8[i], table2to8[u.c.B], table3to8[u.c.R], table3to8[u.c.G]);
			}
		}
		size_t canvas_x = len_x;
		size_t canvas_y = len_y;

		png_bytep canvas = malloc(canvas_y * canvas_x);
		if (canvas == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			exit(-2);
		}

		for (size_t l = 0; l < len_y; l += Rows_per_Strip) {
			size_t read_rows = (l + Rows_per_Strip > len_y) ? len_y - l : Rows_per_Strip;
			if (-1 == TIFFReadEncodedStrip(pTi, TIFFComputeStrip(pTi, l, 0), &canvas[len_x * l], (size_t)len_x * Rows_per_Strip * Bits_per_Sample)) {
				fprintf_s(stderr, "File read error.\n");
				TIFFClose(pTi);
				free(canvas);
				exit(-2);
			}
		}

		TIFFClose(pTi);

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
		imgw.Pal = Pal8;
		imgw.Trans = NULL;
		imgw.nPal = colours;
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