#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "png.h"

#define BPP 8

#pragma pack (1)
struct VSP_header {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int16 Format;
	unsigned __int16 Unknown[11];
	unsigned __int8 Palette[256][3];
} hVSP256;

#pragma pack()

unsigned __int8 screen[400][640];

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		size_t rcount = fread_s(&hVSP256, sizeof(hVSP256), sizeof(hVSP256), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		size_t vsp_len = fs.st_size - sizeof(hVSP256);
		unsigned __int8 *vsp_data = malloc(vsp_len);
		if (vsp_data == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		rcount = fread_s(vsp_data, vsp_len, 1, vsp_len, pFi);
		if (rcount != vsp_len) {
			wprintf_s(L"File read error %s %d.\n", *argv, rcount);
			free(vsp_data);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		const size_t vsp_in_x = hVSP256.Column_in;
		const size_t vsp_in_y = hVSP256.Row_in;
		const size_t vsp_out_x = hVSP256.Column_out + 1;
		const size_t vsp_out_y = hVSP256.Row_out + 1;
		const size_t vsp_len_x = hVSP256.Column_out - hVSP256.Column_in + 1;
		const size_t vsp_len_y = hVSP256.Row_out - hVSP256.Row_in + 1;
		const size_t vsp_len_decoded = vsp_len_y * vsp_len_x;

		unsigned __int8 *vsp_data_decoded = malloc(vsp_len_decoded);
		if (vsp_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(vsp_data);
			exit(-2);
		}

		wprintf_s(L"%zu/%zu - %zu/%zu VSP size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, vsp_len, vsp_len_decoded);

		size_t count = vsp_len, cp_len;
		unsigned __int8 *src = vsp_data, *dst = vsp_data_decoded, *cp_src;

		while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
			//			wprintf_s(L"%04X.\n", src - vsp_data + sizeof(hGL3));
			switch (*src) {
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
				src++;
				*dst++ = *src++;
				break;
			case 0xFC:
				cp_len = *(src + 1) + 3;
				for (size_t len = 0; len < cp_len; len++) {
					memcpy_s(dst, 2, src + 2, 2);
					dst += 2;
				}
				src += 4;
				count -= 3;
				break;
			case 0xFD:
				cp_len = *(src + 1) + 4;
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
				break;
			case 0xFE:
				cp_len = *(src + 1) + 3;
				cp_src = dst - vsp_len_x * 2;
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0xFF:
				cp_len = *(src + 1) + 3;
				cp_src = dst - vsp_len_x;
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			default:
				*dst++ = *src++;
			}
		}

		free(vsp_data);


		memset(screen, 0, sizeof(screen));


		for (size_t iy = 0; iy < vsp_len_y; iy++) {
			for (size_t ix = 0; ix < vsp_len_x; ix++) {
				screen[vsp_in_y + iy][vsp_in_x + ix] = vsp_data_decoded[iy*vsp_len_x + ix];
			}
		}
		free(vsp_data_decoded);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		ecode = _wfopen_s(&pFo, path, L"wb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		png_structp png_ptr;
		png_infop info_ptr;
		png_color pal[256];

		for (size_t ci = 0; ci < 256; ci++) {
			pal[ci].blue = hVSP256.Palette[ci][2];
			pal[ci].red = hVSP256.Palette[ci][0];
			pal[ci].green = hVSP256.Palette[ci][1];
		}

		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == NULL) {
			fclose(pFo);
			return;
		}
		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL) {
			png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
			fclose(pFo);
			return;
		}

		unsigned char **image;
		image = (png_bytepp)malloc(400 * sizeof(png_bytep));
		if (image == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			fclose(pFo);
			exit(-2);
		}
		for (size_t j = 0; j < 400; j++)
			image[j] = (png_bytep)&screen[j];

		png_init_io(png_ptr, pFo);
		png_set_IHDR(png_ptr, info_ptr, 640, 400,
			BPP, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_PLTE(png_ptr, info_ptr, pal, 256);
#if 0
		png_byte trans[16] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		png_set_tRNS(png_ptr, info_ptr, trans, 16, NULL);
#endif
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, image);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		free(image);
		fclose(pFo);
	}
}
