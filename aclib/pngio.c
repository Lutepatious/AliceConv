#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pngio.h"

#define DEFAULT_PpM 4000

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
	if (pngw->nPal != 256) {
		png_set_tRNS(png_ptr, info_ptr, pngw->Trans, pngw->nTrans, NULL);
	}
	png_set_pHYs(png_ptr, info_ptr, DEFAULT_PpM, pngw->pXY == 2 ? DEFAULT_PpM / 2 : DEFAULT_PpM, PNG_RESOLUTION_METER);
	png_set_PLTE(png_ptr, info_ptr, pngw->Pal, pngw->nPal);
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, pngw->image);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(pFo);

	return pngw;
}

#define STEP_3_8(x) ((double) (x) * 255.0L / 7.0L + 0.5L) 

static inline png_byte d3tod8(png_byte a)
{
	const static png_byte table[8] = { 0, STEP_3_8(1), STEP_3_8(2), STEP_3_8(3), STEP_3_8(4), STEP_3_8(5), STEP_3_8(6), 255 };
	return table[a];
}

void color_8to256(png_colorp pcolor, png_byte blue, png_byte red, png_byte green)
{
	pcolor->blue = d3tod8(blue);
	pcolor->red = d3tod8(red);
	pcolor->green = d3tod8(green);
}

#define STEP_4_8(y) ((double) (y) * 255.0L / 15.0L + 0.5L) 

static inline png_byte d4tod8(png_byte a)
{
	const static png_byte table[16] = { 0, STEP_4_8(1), STEP_4_8(2), STEP_4_8(3), STEP_4_8(4), STEP_4_8(5), STEP_4_8(6), STEP_4_8(7),
											STEP_4_8(8), STEP_4_8(9), STEP_4_8(10), STEP_4_8(11), STEP_4_8(12), STEP_4_8(13), STEP_4_8(14), 255 };
	return table[a];
}

void color_16to256(png_colorp pcolor, png_byte blue, png_byte red, png_byte green)
{
	pcolor->blue = d4tod8(blue);
	pcolor->red = d4tod8(red);
	pcolor->green = d4tod8(green);
}

void color_256to256(png_colorp pcolor, png_byte blue, png_byte red, png_byte green)
{
	pcolor->blue = blue;
	pcolor->red = red;
	pcolor->green = green;
}
