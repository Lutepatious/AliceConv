#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "png.h"
#include "zlib.h"

#define MSX_COLS 512
#define MSX_ROWS 212

struct fPNG {
	FILE* pFi;
	png_structp ppng;
	png_infop pinfo;
	png_bytepp image;
	png_colorp Pal;
	png_bytep Trans;
	png_uint_32 Rows;
	png_uint_32 Cols;
	int nPal;
	int nTrans;
	int depth;
};

struct fPNG* png_open(wchar_t* infile)
{
	FILE* pFi;
	errno_t ecode = _wfopen_s(&pFi, infile, L"rb");
	if (ecode) {
		wprintf_s(L"File open error %s.\n", infile);
		return NULL;
	}

	png_byte pngsig[8];
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	size_t rcount = fread_s(pngsig, sizeof(pngsig), sizeof(pngsig), 1, pFi);
	if (rcount != 1) {
		wprintf_s(L"File read error %s.\n", infile);
		fclose(pFi);
		return NULL;
	}

	if (png_sig_cmp(pngsig, 0, sizeof(pngsig))) {
		fclose(pFi);
		return NULL;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(pFi);
		return NULL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		fclose(pFi);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		fclose(pFi);
		return NULL;
	}

	png_init_io(png_ptr, pFi);
	png_set_sig_bytes(png_ptr, sizeof(pngsig));
	png_set_compression_buffer_size(png_ptr, 0x1000LL);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_PALETTE) {
		wprintf_s(L"Sorry Index-Colour image only %s %04X.\n", infile, png_get_color_type(png_ptr, info_ptr));
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		fclose(pFi);
		return NULL;
	}

	struct fPNG* pimg = malloc(sizeof(struct fPNG));
	if (pimg == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		fclose(pFi);
		return NULL;
	}
	pimg->pFi = pFi;
	pimg->ppng = png_ptr;
	pimg->pinfo = info_ptr;
	pimg->Cols = png_get_image_width(png_ptr, info_ptr);
	pimg->Rows = png_get_image_height(png_ptr, info_ptr);
	pimg->image = png_get_rows(png_ptr, info_ptr);
	pimg->depth = png_get_bit_depth(png_ptr, info_ptr);
	png_get_PLTE(png_ptr, info_ptr, &pimg->Pal, &pimg->nPal);
	png_get_tRNS(png_ptr, info_ptr, &pimg->Trans, &pimg->nTrans, NULL);

	return pimg;
}

void png_close(struct fPNG* pimg)
{
	png_destroy_read_struct(&pimg->ppng, &pimg->pinfo, (png_infopp)NULL);
	fclose(pimg->pFi);
	free(pimg);
}

struct fPNGw {
	wchar_t* outfile;
	png_bytepp image;
	png_colorp Pal;
	png_bytep Trans;
	png_uint_32 Rows;
	png_uint_32 Cols;
	int nPal;
	int nTrans;
	int depth;
};

void* png_create(struct fPNGw* pngw)
{
	FILE* pFo;
	errno_t ecode = _wfopen_s(&pFo, pngw->outfile, L"wb");
	if (ecode) {
		wprintf_s(L"File open error %s.\n", pngw->outfile);
		return NULL;
	}
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fclose(pFo);
		return NULL;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		fclose(pFo);
		return NULL;
	}
	png_init_io(png_ptr, pFo);
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_IHDR(png_ptr, info_ptr, pngw->Cols, pngw->Rows, pngw->depth, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_tRNS(png_ptr, info_ptr, pngw->Trans, pngw->nTrans, NULL);
	png_set_PLTE(png_ptr, info_ptr, pngw->Pal, pngw->nPal);
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, pngw->image);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(pFo);

	return pngw;
}

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
		wchar_t fname_odd[_MAX_FNAME];
		wchar_t fname_even[_MAX_FNAME];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*++argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		if (wcslen(fname) != 7) {
			wprintf_s(L"Unexpected filename %s.\n", *argv);
			continue;
		}
		if (!iswdigit(fname[0]) || !iswdigit(fname[1]) || !iswdigit(fname[2]) || !iswalpha(fname[3]) || !iswdigit(fname[4]) || !iswdigit(fname[5]) || !iswdigit(fname[6])) {
			wprintf_s(L"Not suitable format %s.\n", *argv);
			continue;
		}

		wchar_t path_num1[4];
		wchar_t path_num2[4];
		wchar_t path_a = fname[3], * stopscan;
		unsigned long num1, num2;
		unsigned long num1_odd, num2_odd;
		unsigned long num1_even, num2_even;

		wcsncpy_s(path_num1, 4, fname, 3);
		wcsncpy_s(path_num2, 4, &fname[4], 3);

		num1 = wcstoul(path_num1, &stopscan, 10);
		num2 = wcstoul(path_num2, &stopscan, 10);

		if (num1 & 1) {
			num1_odd = num1;
			num2_odd = num2;
			num1_even = num1 + 1;
			num2_even = num2 + 1;
			swprintf(fname, _MAX_FNAME, L"%03ld-%03ld", num1_odd, num1_even);
		}
		else {
			num1_odd = num1 - 1;
			num2_odd = num2 - 1;
			num1_even = num1;
			num2_even = num2;
			swprintf(fname, _MAX_FNAME, L"%03ld-%03ld", num1_even, num1_odd);
		}

		swprintf(fname_odd, _MAX_FNAME, L"%03ld%c%03ld", num1_odd, path_a, num2_odd);
		swprintf(fname_even, _MAX_FNAME, L"%03ld%c%03ld", num1_even, path_a, num2_even);
		_wmakepath_s(path_odd, _MAX_PATH, drive, dir, fname_odd, L".png");
		_wmakepath_s(path_even, _MAX_PATH, drive, dir, fname_even, L".png");
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

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
		imgw.outfile = path;
		imgw.depth = pimg_odd->depth;
		imgw.image = malloc(sizeof(png_bytep) * pimg_odd->Rows * 2);
		imgw.Cols = pimg_odd->Cols;
		imgw.Rows = pimg_odd->Rows * 2;
		imgw.Pal = pimg_odd->Pal;
		imgw.Trans = pimg_odd->Trans;
		imgw.nPal = pimg_odd->nPal;
		imgw.nTrans = pimg_odd->nTrans;

		for (size_t i = 0 ; i < pimg_odd->Rows ; i++) {
			*(imgw.image + i * 2) = *(pimg_odd->image + i);
			*(imgw.image + i * 2 + 1) = *(pimg_even->image + i);
		}
		void *res = png_create(&imgw);

		free(imgw.image);
		png_close(pimg_odd);
		png_close(pimg_even);
		if (res == NULL) {
			wprintf_s(L"File %s create/write error\n", path);
		}
	}
}