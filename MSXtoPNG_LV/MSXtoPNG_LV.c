// MSXtoPNG Little Vampire��p��

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"

#define MSX_ROWS 212

#pragma pack(1)
// MSX�̃t�H�[�}�b�g�̃p���b�g���
struct MSX_Palette {
	unsigned __int16 C0 : 4;
	unsigned __int16 C1 : 4;
	unsigned __int16 C2 : 4;
	unsigned __int16 u0 : 4;
} Pal[8] = { { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
				{ 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 } };

// MSX�̃t�H�[�}�b�g�̃w�b�_
struct MSXo_header {
	unsigned __int8 M1;
	unsigned __int8 NegCols; // ~(Cols - 1)
	unsigned __int8 Rows;
} hMSXo;
#pragma pack()

// �C���[�W���ۊǗp�\����
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
	FILE* pFi;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		enum fmt_cg g_fmt = NONE;

		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		size_t rcount = fread_s(&hMSXo, sizeof(hMSXo), sizeof(hMSXo), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		size_t msx_len = fs.st_size - sizeof(hMSXo);
		unsigned __int8* msx_data = malloc(msx_len);
		if (msx_data == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		rcount = fread_s(msx_data, msx_len, 1, msx_len, pFi);
		if (rcount != msx_len) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			free(msx_data);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		size_t msx_start = 0;
		size_t msx_len_x = hMSXo.NegCols == 0x01 ? 256 : (unsigned __int8)(~(hMSXo.NegCols)) + 1LL;
		size_t msx_len_y = hMSXo.Rows;
		size_t msx_len_decoded = msx_len_y * msx_len_x;
		unsigned __int8* msx_data_decoded = calloc(msx_len_decoded, sizeof(unsigned __int8));
		if (msx_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(msx_data);
			exit(-2);
		}
		wprintf_s(L"%s: %3I64u/%3zd MSX size %zu => %zu.\n", *argv, msx_len_x, msx_len_y, msx_len, msx_len_decoded);
		__int64 count = msx_len, cp_len;
		size_t rows_real = 0;
		unsigned __int8* src = msx_data, * dst = msx_data_decoded, * cp_src, prev = ~*src, repeat = 0;

		while (count-- > 0 && (dst - msx_data_decoded) < msx_len_decoded) {
			if ((src - msx_data) % 0x100 == (0x100 - sizeof(struct MSXo_header))) {
				repeat = 0;
				prev = ~*src;
			}
			if (*src == 0xFF && *(src + 1) == 0x1A) {
				break;
			}
			else if (*src == 0xF3 && *(src + 1) == 0xFF) {
				src += 2;
				count--;
			}
			else if (repeat) {
				repeat = 0;
				cp_len = *src - 2;
				if (cp_len > 0) {
					memset(dst, prev, cp_len);
				}
				src++;
				dst += cp_len;
				prev = ~*src;
			}
			else if (*src == 0x88) {
				//				dst += (msx_len_x - ((dst - msx_data_decoded) % msx_len_x)) % msx_len_x;
				prev = *src;
				src++;
				rows_real++;
			}
			else if (*src == prev) {
				repeat = 1;
				prev = *src;
				*dst++ = *src++;
			}
			else {
				repeat = 0;
				prev = *src;
				*dst++ = *src++;
			}
		}
		//		wprintf_s(L"Length %6zd -> %6zd. y = %3zd.\n", src - msx_data, dst - msx_data_decoded, rows_real);
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
		iInfo.start_x = 0;
		iInfo.start_y = 0;
		iInfo.len_x = msx_len_x * 2;
		iInfo.len_y = msx_len_y;
		iInfo.colors = 8;

		size_t canvas_x = iInfo.len_x;
		size_t canvas_y = iInfo.len_y;
		unsigned __int8 t_color = 8;

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
			memcpy_s(&canvas[iy * canvas_x], iInfo.len_x, &iInfo.image[iy * iInfo.len_x], iInfo.len_x);
		}
		free(iInfo.image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		png_color pal[9] = { {0,0,0} };

		for (size_t ci = 0; ci < iInfo.colors; ci++) {
			color_8to256(&pal[ci], Pal[ci].C0, Pal[ci].C1, Pal[ci].C2);
		}
		color_8to256(&pal[iInfo.colors], 0, 0, 0);


		png_byte trans[9] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pal;
		imgw.Trans = trans;
		imgw.nPal = iInfo.colors + 1;
		imgw.nTrans = iInfo.colors + 1;
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
}