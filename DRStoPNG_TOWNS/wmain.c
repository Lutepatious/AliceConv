#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "../aclib/pngio.h"
#include "../aclib/accore.h"
#include "../aclib/acinternal.h"

#pragma pack (1)
struct DRS003T {
	unsigned __int8 body[640][200];
};

struct DRSOPENT {
	unsigned __int8 body[480][320];
};

#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi;

	errno_t ecode = _wfopen_s(&pFi, L"CG003.BMP", L"rb");
	if (ecode) {
		wprintf_s(L"File open error.\n");
		exit(-2);
	}

	struct DRS003T* cg003 = GC_malloc(sizeof(struct DRS003T));
	if (cg003 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	size_t rcount = fread_s(cg003, sizeof(struct DRS003T), sizeof(struct DRS003T), 1, pFi);
	if (rcount != 1) {
		wprintf_s(L"File read error.\n");
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	unsigned __int8* decode_buffer = convert_index4_to_index8_BE(cg003->body, 640 * 400 / 2);

	const struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16] = { {0x0, 0x0, 0x0}, {0x0, 0x0, 0x0}, {0xF, 0xF, 0xF}, {0xA, 0xA, 0xA},
		{0x6, 0x6, 0x6}, {0x1, 0xD, 0x0}, {0x4, 0x0, 0x7}, {0x2, 0x0, 0x3},
		{0xD, 0xD, 0xD}, {0xC, 0xF, 0xD}, {0x8, 0xF, 0xA}, {0x5, 0xD, 0x6},
		{0x1, 0x6, 0x2}, {0xD, 0xF, 0xB}, {0xA, 0xF, 0x7}, {0xE, 0xE, 0xE} };

	png_colorp Pal8 = GC_malloc(sizeof(png_color) * 16);
	for (size_t ci = 0; ci < 16; ci++) {
		struct fPal8 inPal4;
		inPal4.R = Pal4[ci].R;
		inPal4.G = Pal4[ci].G;
		inPal4.B = Pal4[ci].B;
		color_16to256(Pal8 + ci, &inPal4);
	}

	struct fPNGw imgw;
	imgw.outfile = L"003.png";
	imgw.depth = 8;
	imgw.Cols = 400;
	imgw.Rows = 640;
	imgw.Pal = Pal8;
	imgw.Trans = NULL;
	imgw.nPal = 16;
	imgw.pXY = 1;

	imgw.image = GC_malloc(imgw.Rows * sizeof(png_bytep));
	if (imgw.image == NULL) {
		fprintf_s(stderr, "Memory allocation error.\n");
		exit(-2);
	}

	for (size_t j = 0; j < imgw.Rows; j++)
		imgw.image[j] = (png_bytep)&decode_buffer[j * imgw.Cols];

	void* res = png_create(&imgw);
	if (res == NULL) {
		wprintf_s(L"File %s create/write error\n", imgw.outfile);
	}

	ecode = _wfopen_s(&pFi, L"OPENNING.BMP", L"rb");
	if (ecode) {
		wprintf_s(L"File open error.\n");
		exit(-2);
	}

	struct DRSOPENT* cgOPEN = GC_malloc(sizeof(struct DRSOPENT));
	if (cgOPEN == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	rcount = fread_s(cgOPEN, sizeof(struct DRSOPENT), sizeof(struct DRSOPENT), 1, pFi);
	if (rcount != 1) {
		wprintf_s(L"File read error.\n");
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	decode_buffer = convert_index4_to_index8_BE(cgOPEN->body, 640 * 480 / 2);
	const struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4O[3] = { { 0x0, 0x0, 0x0 }, { 0x0, 0x0, 0x0 }, { 0xF, 0xF, 0xF }};

	Pal8 = GC_malloc(sizeof(png_color) * 3);
	for (size_t ci = 0; ci < 3; ci++) {
		struct fPal8 inPal4;
		inPal4.R = Pal4O[ci].R;
		inPal4.G = Pal4O[ci].G;
		inPal4.B = Pal4O[ci].B;
		color_16to256(Pal8 + ci, &inPal4);
	}
	struct fPNGw imgwo;
	imgwo.outfile = L"OPENNING.png";
	imgwo.depth = 8;
	imgwo.Cols = 640;
	imgwo.Rows = 480;
	imgwo.Pal = Pal8;
	imgwo.Trans = NULL;
	imgwo.nPal = 3;
	imgwo.pXY = 1;
	
	imgwo.image = GC_malloc(imgwo.Rows * sizeof(png_bytep));
	if (imgwo.image == NULL) {
		fprintf_s(stderr, "Memory allocation error.\n");
		exit(-2);
	}

	for (size_t j = 0; j < imgwo.Rows; j++)
		imgwo.image[j] = (png_bytep)&decode_buffer[j * imgwo.Cols];

	res = png_create(&imgwo);
	if (res == NULL) {
		wprintf_s(L"File %s create/write error\n", imgwo.outfile);
	}

}