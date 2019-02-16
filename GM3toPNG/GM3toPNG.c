#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "png.h"

#define PLANE 4
#define BPP 8
#define ROW 201

#pragma pack (1)
struct GL3_header {
	unsigned __int8 Palette[16][3];
	unsigned __int16 Start;
	unsigned __int16 Columns; // divided by 8
	unsigned __int16 Rows;
} hGL3;

unsigned __int8 decode2bpp(unsigned __int64 *dst, const unsigned __int8 *src, size_t col, size_t row)
{
	unsigned __int8 max_ccode = 0;
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			union {
				__int64 a;
				__int8 a8[8];
			} u;
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0) | ((*(src + col * 3) & (1 << x)) ? 8 : 0);
				if (max_ccode < u.a8[x]) {
					max_ccode = u.a8[x];
				}
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * (PLANE - 1);
	}
	return max_ccode + 1;
}
#pragma pack()

unsigned __int8 screen[ROW][640];

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

		size_t rcount = fread_s(&hGL3, sizeof(hGL3), sizeof(hGL3), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		size_t gl3_len = fs.st_size - sizeof(hGL3);
		unsigned __int8 *gl3_data = malloc(gl3_len);
		if (gl3_data == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		rcount = fread_s(gl3_data, gl3_len, 1, gl3_len, pFi);
		if (rcount != gl3_len) {
			wprintf_s(L"File read error %s %d.\n", *argv, rcount);
			free(gl3_data);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		size_t gl3_start = hGL3.Start & 0x7FFF;
		size_t gl3_start_x = gl3_start % 80 * 8;
		size_t gl3_start_y = gl3_start / 80;
		size_t gl3_len_decoded = hGL3.Rows * hGL3.Columns * PLANE;
		unsigned __int8 *gl3_data_decoded = malloc(gl3_len_decoded);
		if (gl3_data_decoded == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(gl3_data);
			exit(-2);
		}
		wprintf_s(L"Start %zu/%zu %zu*%zu GM3 size %zu => %zu.\n", gl3_start_x, gl3_start_y, hGL3.Columns * 8, hGL3.Rows, gl3_len, gl3_len_decoded);

		size_t count = gl3_len, cp_len, cur_plane;
		unsigned __int8 *src = gl3_data, *dst = gl3_data_decoded, *cp_src;
		while (count-- && (dst - gl3_data_decoded) < gl3_len_decoded) {
			//			wprintf_s(L"%04X.\n", src - gl3_data + sizeof(hGL3));
			switch (*src) {
			case 0x00:
				cp_len = *(src + 1) & 0x7F;
				if (*(src + 1) & 0x80) {
					cp_src = dst - hGL3.Columns * PLANE * 2 + 1;
					memset(dst, *cp_src, cp_len);
					dst += cp_len;
					src += 2;
					count--;
				}
				else {
					memset(dst, *(src + 2), cp_len);
					dst += cp_len;
					src += 3;
					count -= 2;
				}
				break;
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				cp_len = *src;
				memset(dst, *(src + 1), cp_len);
				src += 2;
				dst += cp_len;
				count--;
				break;
			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0D:
				cp_len = *src - 6;
				cp_src = dst - hGL3.Columns * PLANE * 2 + 1;
				memset(dst, *cp_src, cp_len);
				dst += cp_len;
				src++;
				break;
			case 0x0E:
				cur_plane = ((dst - gl3_data_decoded) / hGL3.Columns) % PLANE;
				cp_len = *(src + 1) & 0x7F;
				cp_src = dst - hGL3.Columns * (cur_plane - 2);
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0x0F:
				cur_plane = ((dst - gl3_data_decoded) / hGL3.Columns) % PLANE;
				cp_len = *(src + 1) & 0x7F;
				cp_src = dst - hGL3.Columns * ((*(src + 1) & 0x80) ? cur_plane - 1 : cur_plane);
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;

			default:
				*dst++ = *src++;
			}
		}

		free(gl3_data);

		size_t decode_len = hGL3.Rows * hGL3.Columns * BPP;
		unsigned __int8 *decode_buffer = malloc(decode_len);
		if (decode_buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(gl3_data_decoded);
			exit(-2);
		}

		decode2bpp(decode_buffer, gl3_data_decoded, hGL3.Columns, hGL3.Rows);
		free(gl3_data_decoded);

		memset(screen, 0, sizeof(screen));

		for (size_t iy = 0; iy < hGL3.Rows; iy++) {
			for (size_t ix = 0; ix < hGL3.Columns * 8; ix++) {
				screen[gl3_start_y + iy][gl3_start_x + ix] = decode_buffer[iy*hGL3.Columns * 8 + ix];
			}
		}
		free(decode_buffer);

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
		png_color pal[16];

		unsigned __int8 Palette[16][3] = {{ 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
										  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
										  { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
										  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF }};

		for (size_t ci = 0; ci < 16; ci++) {
			pal[ci].blue = Palette[ci][0] * 0x11;
			pal[ci].red = Palette[ci][1] * 0x11;
			pal[ci].green = Palette[ci][2] * 0x11;
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
		image = (png_bytepp)malloc(ROW * sizeof(png_bytep));
		if (image == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			fclose(pFo);
			exit(-2);
		}
		for (size_t j = 0; j < ROW; j++)
			image[j] = (png_bytep)&screen[j];

		png_init_io(png_ptr, pFo);
		png_set_IHDR(png_ptr, info_ptr, 640, ROW,
			BPP, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_PLTE(png_ptr, info_ptr, pal, 16);
		png_byte trans[16] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		png_set_tRNS(png_ptr, info_ptr, trans, 16, NULL);
		png_set_pHYs(png_ptr, info_ptr, 2, 1, PNG_RESOLUTION_UNKNOWN);

		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, image);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		free(image);
		fclose(pFo);
	}
}
