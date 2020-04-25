#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"

#define PLANE 4
#define BPP 8
#define ROWS 480
#define MSX_ROWS 212


// MSXのフォーマットのパレット情報
struct MSX_Palette {
	unsigned __int16 C0 : 4;
	unsigned __int16 C1 : 4;
	unsigned __int16 C2 : 4;
	unsigned __int16 u0 : 4;
}  Pal[8] = { { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
				{ 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 } };

#pragma pack(1)
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
	unsigned __int8* msx_data = malloc(msx_len);
	if (msx_data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	rcount = fread_s(msx_data, msx_len, 1, msx_len, pFi);
	if (rcount != msx_len) {
		wprintf_s(L"File read error %zd.\n", rcount);
		free(msx_data);
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
		free(msx_data);
		fclose(pFo);
		exit(-2);
	}
	fclose(pFo);

	size_t msx_start = 0;
	size_t msx_start_x = msx_start % 256 * 2;
	size_t msx_start_y = msx_start / 256;
	size_t msx_len_x = 256LL;
	size_t msx_len_y = 0xD4LL;
	size_t msx_len_decoded = 0xD400LL;
	unsigned __int8* msx_data_decoded = malloc(msx_len_decoded);
	if (msx_data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(msx_data);
		exit(-2);
	}
	wprintf_s(L"%s: %3zd/%3zd MSX size %zu => %zu.\n", *argv,  msx_len_x * 2, msx_len_y, msx_len, msx_len_decoded);
	__int64 count = msx_len_decoded;
	size_t cp_len;
	unsigned __int8* src = msx_data, * dst = msx_data_decoded, prev = ~*src, repeat = 0, reset = 1;

	while (count-- > 0 && (dst - msx_data_decoded) < msx_len_decoded) {
		*dst++ = *src++;
	}
	free(msx_data);

	size_t decode_len = msx_len_y * msx_len_x * 2;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(msx_data_decoded);
		exit(-2);
	}

	for (size_t i = 0; i < msx_len_decoded; i++) {
		decode_buffer[i * 2] = (msx_data_decoded[i] & 0xF0) >> 4;
		decode_buffer[i * 2 + 1] = msx_data_decoded[i] & 0xF;
	}

	free(msx_data_decoded);
	iInfo.image = decode_buffer;
	iInfo.start_x = msx_start_x;
	iInfo.start_y = msx_start_y;
	iInfo.len_x = msx_len_x * 2;
	iInfo.len_y = msx_len_y;
	iInfo.colors = 8;

	size_t canvas_x = 512;
	size_t canvas_y = 212;
	unsigned __int8 t_color = 0x10;

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

	_wsplitpath_s(L"TITLE", drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
	_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

	png_color pal[8] = { {0,0,0} };

	for (size_t ci = 0; ci < 8; ci++) {
		pal[ci].blue = (Pal[ci].C0 * 0x24) | (Pal[ci].C0 >> 1);
		pal[ci].red = (Pal[ci].C1 * 0x24) | (Pal[ci].C1 >> 1);
		pal[ci].green = (Pal[ci].C2 * 0x24) | (Pal[ci].C2 >> 1);
	}
	png_byte trans[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	struct fPNGw imgw;
	imgw.outfile = path;
	imgw.depth = 8;
	imgw.Cols = canvas_x;
	imgw.Rows = canvas_y;
	imgw.Pal = pal;
	imgw.Trans = trans;
	imgw.nPal = 8;
	imgw.nTrans = 8;
	imgw.pXY = 2;

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
