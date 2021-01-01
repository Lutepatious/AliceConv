#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "../aclib/pngio.h"

#define MSX_ROWS 212

#pragma pack(1)
// MSXのフォーマットのパレット情報
struct MSX_Palette {
	unsigned __int16 C0 : 4;
	unsigned __int16 C1 : 4;
	unsigned __int16 C2 : 4;
	unsigned __int16 u0 : 4;
}  Pal[8] = { { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
				{ 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 } };

// MSXのフォーマットのヘッダ
struct MSXt_header {
	unsigned __int8 u0;
	unsigned __int16 u1;
	unsigned __int16 len;
	unsigned __int16 u2;
} hMSXt;
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

enum fmt_cg { NONE, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS, PMS16, QNT, MSX };

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi, *pFo;

	enum fmt_cg g_fmt = NONE;

	errno_t ecode = _wfopen_s(&pFi, L"TITLE", L"rb");
	if (ecode) {
		wprintf_s(L"File open error\n");
		exit(ecode);
	}

	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	unsigned __int8 hbuf[0x40];
	size_t rcount = fread_s(hbuf, sizeof(hbuf), sizeof(hbuf), 1, pFi);
	if (rcount != 1) {
		wprintf_s(L"File read error\n");
		fclose(pFi);
		exit(-2);
	}

	fseek(pFi, 0, SEEK_SET);

	rcount = fread_s(&hMSXt, sizeof(hMSXt), sizeof(hMSXt), 1, pFi);
	if (rcount != 1) {
		wprintf_s(L"File read error\n");
		fclose(pFi);
		exit(-2);
	}

	size_t msx_len = fs.st_size - sizeof(hMSXt);
	unsigned __int8* msx_data = GC_malloc(msx_len);
	if (msx_data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	rcount = fread_s(msx_data, msx_len, 1, msx_len, pFi);
	if (rcount != msx_len) {
		wprintf_s(L"File read error %zd.\n", rcount);
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	// Palette Out 0xF800 to 0xF860
	ecode = _wfopen_s(&pFo, L"palette", L"wb");
	if (ecode) {
		wprintf_s(L"File open error\n");
		exit(ecode);
	}

	rcount = fwrite(msx_data+0xF800LL, 1, 0x60, pFo);
	if (rcount != 0x60) {
		wprintf_s(L"File read error %zd.\n", rcount);
		fclose(pFo);
		exit(-2);
	}
	fclose(pFo);

	size_t msx_start_x = 0;
	size_t msx_start_y = 0;
	size_t msx_len_x = 256LL;
	size_t msx_len_y = 0xD4LL;
	size_t msx_len_decoded = 0xD400LL;
	wprintf_s(L"%3zd/%3zd MSX_RAW size %zu => %zu.\n",  msx_len_x * 2, msx_len_y, msx_len, msx_len_decoded);

	size_t decode_len = msx_len_y * msx_len_x * 2;
	unsigned __int8* decode_buffer = GC_malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	for (size_t i = 0; i < msx_len_decoded; i++) {
		decode_buffer[i * 2] = (msx_data[i] & 0xF0) >> 4;
		decode_buffer[i * 2 + 1] = msx_data[i] & 0xF;
	}

	iInfo.image = decode_buffer;
	iInfo.start_x = msx_start_x;
	iInfo.start_y = msx_start_y;
	iInfo.len_x = msx_len_x * 2;
	iInfo.len_y = msx_len_y;
	iInfo.colors = 8;

	size_t canvas_x = 512;
	size_t canvas_y = 212;

	unsigned __int8* canvas = decode_buffer;

	wchar_t path[_MAX_PATH];
	wchar_t fname[_MAX_FNAME];
	wchar_t dir[_MAX_DIR];
	wchar_t drive[_MAX_DRIVE];

	_wsplitpath_s(L"TITLE", drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
	_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

	png_color pal[128] = { {0,0,0} };

	for (size_t ci = 0; ci < iInfo.colors; ci++) {
		struct fPal8 Pal3;
		Pal3.R = Pal[ci].C1;
		Pal3.G = Pal[ci].C2;
		Pal3.B = Pal[ci].C0;
		color_8to256(&pal[ci], &Pal3);
	}
	png_byte trans[128] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	struct MSX_Palette* hp = msx_data + 0xF800LL;

	for (size_t ci = 0; ci < 0x30; ci++) {
		struct fPal8 Pal3;
		Pal3.R = hp[ci].C1;
		Pal3.G = hp[ci].C2;
		Pal3.B = hp[ci].C0;
		color_8to256(&pal[ci + 0x20], &Pal3);
	}

	unsigned __int8 (*psel)[16] = msx_data + 0xD400LL;











	struct fPNGw imgw;
	imgw.outfile = path;
	imgw.depth = 8;
	imgw.Cols = canvas_x;
	imgw.Rows = canvas_y;
	imgw.Pal = pal;
	imgw.Trans = NULL;
	imgw.nPal = 128;
	imgw.nTrans = 128;
	imgw.pXY = 2;

	imgw.image = GC_malloc(canvas_y * sizeof(png_bytep));
	if (imgw.image == NULL) {
		fprintf_s(stderr, "Memory allocation error.\n");
		exit(-2);
	}
	for (size_t j = 0; j < canvas_y; j++)
		imgw.image[j] = (png_bytep)&canvas[j * canvas_x];

	void* res = png_create(&imgw);
	if (res == NULL) {
		wprintf_s(L"File %s create/write error\n", path);
	}

}
