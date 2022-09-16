#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "tiffrw.h"

struct fTIFF* tiff_read(const wchar_t* infile)
{
	TIFF* pTi = TIFFOpenW(infile, "r");

	if (pTi == NULL) {
		wprintf_s(L"File open error. %s\n", infile);
		TIFFClose(pTi);
		return NULL;
	}

	struct fTIFF* pimg = GC_malloc(sizeof(struct fTIFF));
	if (pimg == NULL) {
		fwprintf_s(stderr, L"Memory allocation error.\n");
		TIFFClose(pTi);
		return NULL;
	}
	
	unsigned __int16 Samples_per_Pixel;
	unsigned __int32 Rows_per_Strip;
	TIFFGetField(pTi, TIFFTAG_IMAGEWIDTH, &pimg->Cols);
	TIFFGetField(pTi, TIFFTAG_IMAGELENGTH, &pimg->Rows);
	TIFFGetField(pTi, TIFFTAG_BITSPERSAMPLE, &pimg->depth);
	TIFFGetField(pTi, TIFFTAG_SAMPLESPERPIXEL, &Samples_per_Pixel);
	TIFFGetFieldDefaulted(pTi, TIFFTAG_ROWSPERSTRIP, &Rows_per_Strip);
	TIFFGetField(pTi, TIFFTAG_PHOTOMETRIC, &pimg->Format);

	pimg->image = GC_malloc((size_t) pimg->Rows * pimg->Cols * Samples_per_Pixel);
	if (pimg->image == NULL) {
		fwprintf_s(stderr, L"Memory allocation error.\n");
		TIFFClose(pTi);
		return NULL;
	}

	for (unsigned __int32 l = 0; l < pimg->Rows; l += Rows_per_Strip) {
		size_t read_rows = (l + Rows_per_Strip > pimg->Rows) ? pimg->Rows - l : Rows_per_Strip;
		if (-1 == TIFFReadEncodedStrip(pTi, TIFFComputeStrip(pTi, l, 0), &pimg->image[(size_t) pimg->Cols * Samples_per_Pixel * l], (size_t) pimg->Cols * read_rows * Samples_per_Pixel)) {
			fwprintf_s(stderr, L"File read error.\n");
			TIFFClose(pTi);
			return NULL;
		}
	}

	if (pimg->Format == PHOTOMETRIC_PALETTE) {
		unsigned __int16* PalR, * PalG, * PalB;
		TIFFGetField(pTi, TIFFTAG_COLORMAP, &PalR, &PalG, &PalB);
		memcpy_s(pimg->Pal.R, sizeof(pimg->Pal.R), PalR, sizeof(unsigned __int16) * (1LL << pimg->depth));
		memcpy_s(pimg->Pal.G, sizeof(pimg->Pal.G), PalG, sizeof(unsigned __int16) * (1LL << pimg->depth));
		memcpy_s(pimg->Pal.B, sizeof(pimg->Pal.B), PalB, sizeof(unsigned __int16) * (1LL << pimg->depth));
	}

	TIFFClose(pTi);
	return pimg;
}