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
struct DRS003 {
	struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16];
	unsigned __int8 body[640][4][50];
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi;

	errno_t ecode = _wfopen_s(&pFi, L"CG003.DAT", L"rb");
	if (ecode || !pFi) {
		wprintf_s(L"File open error.\n");
		exit(-2);
	}

	struct DRS003* cg003 = GC_malloc(sizeof(struct DRS003));
	if (cg003 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	const size_t rcount = fread_s(cg003, sizeof(struct DRS003), sizeof(struct DRS003), 1, pFi);
	if (rcount != 1) {
		wprintf_s(L"File read error.\n");
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);


	struct plane4_dot8* buffer_dot8 = convert_YPC_to_YCP(cg003->body, 640, 50, 4);
	unsigned __int8* decode_buffer = convert_plane4_dot8_to_index8(buffer_dot8, 640 * 50);

	png_colorp Pal8 = GC_malloc(sizeof(png_color) * 16);
	for (size_t ci = 0; ci < 16; ci++) {
		struct fPal8 inPal4;
		inPal4.R = cg003->Pal4[ci].R;
		inPal4.G = cg003->Pal4[ci].G;
		inPal4.B = cg003->Pal4[ci].B;
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
}