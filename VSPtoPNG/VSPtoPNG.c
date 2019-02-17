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
#define ROWS 480


#pragma pack (1)
struct VSP_header {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int16 Unknown;
	unsigned __int8 Palette[16][3];
} hVSP;

struct VSP256_header {
	unsigned __int16 Column_in;
	unsigned __int16 Row_in;
	unsigned __int16 Column_out;
	unsigned __int16 Row_out;
	unsigned __int16 Unknown[12];
	unsigned __int8 Palette[256][3];
} hVSP256;

struct VSP200l_header {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int16 Unknown[9];
} hVSP200l;

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

unsigned __int8 decode2bpp_3(unsigned __int64 *dst, const unsigned __int8 *src, size_t col, size_t row)
{
	unsigned __int8 max_ccode = 0;
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			union {
				__int64 a;
				__int8 a8[8];
			} u;
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0);
				if (max_ccode < u.a8[x]) {
					max_ccode = u.a8[x];
				}
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * (PLANE - 2);
	}
	return max_ccode + 1;
}
#pragma pack()

unsigned __int8 screen[ROWS][640];

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		unsigned is256 = 0, is200l = 0;
		size_t rows = 400;
		size_t color = 17;

		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		unsigned __int8 hbuf[0x40];
		size_t rcount = fread_s(hbuf, sizeof(hbuf), sizeof(hbuf), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		fseek(pFi, 0, SEEK_SET);

		if (hbuf[1] >= 0x10 || hbuf[8] >= 2) {
			wprintf_s(L"Wrong data exist. %s is not VSP.\n", *argv);
			continue;
		}

		size_t i = 0xA;
		do {
			if (hbuf[i] != 0)
				break;
		} while (++i < 0x3A);

		if (hbuf[0x8] == 1 || (i >= 0x20 && i < 0x3A)) {
			is256 = 1;
		}

		if (hbuf[0x10] == 0x3F) {
			is200l = 1;
		}

		if (is256) {
			color = 256;
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

			if (vsp_out_y > 400) {
				rows = ROWS;
			}

			unsigned __int8 *vsp_data_decoded = malloc(vsp_len_decoded);
			if (vsp_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			wprintf_s(L"%zu/%zu - %zu/%zu VSP256 size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, vsp_len, vsp_len_decoded);

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

			memset(screen, 0x0, sizeof(screen));

			for (size_t iy = 0; iy < vsp_len_y; iy++) {
				for (size_t ix = 0; ix < vsp_len_x; ix++) {
					screen[vsp_in_y + iy][vsp_in_x + ix] = vsp_data_decoded[iy*vsp_len_x + ix];
				}
			}
			free(vsp_data_decoded);
		}
		else if (is200l) {
			rows = 200;
			rcount = fread_s(&hVSP200l, sizeof(hVSP200l), sizeof(hVSP200l), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}

			size_t vsp_len = fs.st_size - sizeof(hVSP200l);
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

			const size_t vsp_in_col = hVSP200l.Column_in;
			const size_t vsp_in_x = vsp_in_col * 8;
			const size_t vsp_in_y = hVSP200l.Row_in;
			const size_t vsp_out_col = hVSP200l.Column_out;
			const size_t vsp_out_x = vsp_out_col * 8;
			const size_t vsp_out_y = hVSP200l.Row_out;
			const size_t vsp_len_col = hVSP200l.Column_out - hVSP200l.Column_in;
			const size_t vsp_len_x = vsp_len_col * 8;
			const size_t vsp_len_y = hVSP200l.Row_out - hVSP200l.Row_in;
			const size_t vsp_len_decoded = vsp_len_y * vsp_len_col * (PLANE-1);

			unsigned __int8 *vsp_data_decoded = malloc(vsp_len_decoded);
			if (vsp_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			wprintf_s(L"%zu/%zu - %zu/%zu VSP200l size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, vsp_len, vsp_len_decoded);

			size_t count = vsp_len, cp_len, cur_plane;
			unsigned __int8 *src = vsp_data, *dst = vsp_data_decoded, *cp_src, negate = 0;

			while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
				switch (*src) {
				case 0x00:
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * (PLANE-1);
					memcpy_s(dst, cp_len, cp_src, cp_len);
					dst += cp_len;
					src += 2;
					count--;
					break;
				case 0x01:
					cp_len = *(src + 1) + 1;
					memset(dst, *(src + 2), cp_len);
					dst += cp_len;
					src += 3;
					count -= 2;
					break;
				case 0x02:
					cp_len = *(src + 1) + 1;
					for (size_t len = 0; len < cp_len; len++) {
						memcpy_s(dst, 2, src + 2, 2);
						dst += 2;
					}
					src += 4;
					count -= 3;
					break;
				case 0x03:
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % (PLANE-1);
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * cur_plane;
					if (negate) {
						for (size_t len = 0; len < cp_len; len++) {
							*dst++ = ~*cp_src++;
						}
					}
					else {
						memcpy_s(dst, cp_len, cp_src, cp_len);
						dst += cp_len;
					}
					src += 2;
					count--;
					negate = 0;
					break;
				case 0x04:
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % (PLANE-1);
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * (cur_plane - 1);
					if (negate) {
						for (size_t len = 0; len < cp_len; len++) {
							*dst++ = ~*cp_src++;
						}
					}
					else {
						memcpy_s(dst, cp_len, cp_src, cp_len);
						dst += cp_len;
					}
					src += 2;
					count--;
					negate = 0;
					break;
				case 0x05:
					wprintf_s(L"No! You must NOT.\n");
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % (PLANE-1);
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * (cur_plane - 2);
					if (negate) {
						for (size_t len = 0; len < cp_len; len++) {
							*dst++ = ~*cp_src++;
						}
					}
					else {
						memcpy_s(dst, cp_len, cp_src, cp_len);
						dst += cp_len;
					}
					src += 2;
					count--;
					negate = 0;
					break;
				case 0x06:
					src++;
					negate = 1;
					break;
				case 0x07:
					src++;
					*dst++ = *src++;
					break;
				default:
					*dst++ = *src++;
				}
			}

			free(vsp_data);

			unsigned __int8 *vsp_data_decoded2 = malloc(vsp_len_decoded);
			if (vsp_data_decoded2 == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			for (size_t ix = 0; ix < vsp_len_col; ix++) {
				for (size_t ip = 0; ip < (PLANE-1); ip++) {
					for (size_t iy = 0; iy < vsp_len_y; iy++) {
						vsp_data_decoded2[iy*vsp_len_col*(PLANE-1) + ip * vsp_len_col + ix] = vsp_data_decoded[ix*vsp_len_y*(PLANE-1) + ip * vsp_len_y + iy];
					}
				}
			}
			free(vsp_data_decoded);

			size_t decode_len = vsp_len_y * vsp_len_col * BPP;
			unsigned __int8 *decode_buffer = malloc(decode_len);
			if (decode_buffer == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data_decoded2);
				exit(-2);
			}

			decode2bpp_3(decode_buffer, vsp_data_decoded2, vsp_len_col, vsp_len_y);
			free(vsp_data_decoded2);

			memset(screen, 0x10, sizeof(screen));

			for (size_t iy = 0; iy < vsp_len_y; iy++) {
				for (size_t ix = 0; ix < vsp_len_x; ix++) {
					screen[vsp_in_y + iy][vsp_in_x + ix] = decode_buffer[iy*vsp_len_x + ix];
				}
			}
			free(decode_buffer);
		}
		else {
			rcount = fread_s(&hVSP, sizeof(hVSP), sizeof(hVSP), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}

			size_t vsp_len = fs.st_size - sizeof(hVSP);
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

			const size_t vsp_in_col = hVSP.Column_in;
			const size_t vsp_in_x = vsp_in_col * 8;
			const size_t vsp_in_y = hVSP.Row_in;
			const size_t vsp_out_col = hVSP.Column_out;
			const size_t vsp_out_x = vsp_out_col * 8;
			const size_t vsp_out_y = hVSP.Row_out;
			const size_t vsp_len_col = hVSP.Column_out - hVSP.Column_in;
			const size_t vsp_len_x = vsp_len_col * 8;
			const size_t vsp_len_y = hVSP.Row_out - hVSP.Row_in;
			const size_t vsp_len_decoded = vsp_len_y * vsp_len_col * PLANE;

			unsigned __int8 *vsp_data_decoded = malloc(vsp_len_decoded);
			if (vsp_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			wprintf_s(L"%zu/%zu - %zu/%zu VSP size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, vsp_len, vsp_len_decoded);

			size_t count = vsp_len, cp_len, cur_plane;
			unsigned __int8 *src = vsp_data, *dst = vsp_data_decoded, *cp_src, negate = 0;

			while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
				//			wprintf_s(L"%04X.\n", src - vsp_data + sizeof(hGL3));
				switch (*src) {
				case 0x00:
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * PLANE;
					memcpy_s(dst, cp_len, cp_src, cp_len);
					dst += cp_len;
					src += 2;
					count--;
					break;
				case 0x01:
					cp_len = *(src + 1) + 1;
					memset(dst, *(src + 2), cp_len);
					dst += cp_len;
					src += 3;
					count -= 2;
					break;
				case 0x02:
					cp_len = *(src + 1) + 1;
					for (size_t len = 0; len < cp_len; len++) {
						memcpy_s(dst, 2, src + 2, 2);
						dst += 2;
					}
					src += 4;
					count -= 3;
					break;
				case 0x03:
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % PLANE;
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * cur_plane;
					if (negate) {
						for (size_t len = 0; len < cp_len; len++) {
							*dst++ = ~*cp_src++;
						}
					}
					else {
						memcpy_s(dst, cp_len, cp_src, cp_len);
						dst += cp_len;
					}
					src += 2;
					count--;
					negate = 0;
					break;
				case 0x04:
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % PLANE;
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * (cur_plane - 1);
					if (negate) {
						for (size_t len = 0; len < cp_len; len++) {
							*dst++ = ~*cp_src++;
						}
					}
					else {
						memcpy_s(dst, cp_len, cp_src, cp_len);
						dst += cp_len;
					}
					src += 2;
					count--;
					negate = 0;
					break;
				case 0x05:
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % PLANE;
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * (cur_plane - 2);
					if (negate) {
						for (size_t len = 0; len < cp_len; len++) {
							*dst++ = ~*cp_src++;
						}
					}
					else {
						memcpy_s(dst, cp_len, cp_src, cp_len);
						dst += cp_len;
					}
					src += 2;
					count--;
					negate = 0;
					break;
				case 0x06:
					src++;
					negate = 1;
					break;
				case 0x07:
					src++;
					*dst++ = *src++;
					break;
				default:
					*dst++ = *src++;
				}
			}

			free(vsp_data);

			unsigned __int8 *vsp_data_decoded2 = malloc(vsp_len_decoded);
			if (vsp_data_decoded2 == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			for (size_t ix = 0; ix < vsp_len_col; ix++) {
				for (size_t ip = 0; ip < PLANE; ip++) {
					for (size_t iy = 0; iy < vsp_len_y; iy++) {
						vsp_data_decoded2[iy*vsp_len_col*PLANE + ip * vsp_len_col + ix] = vsp_data_decoded[ix*vsp_len_y*PLANE + ip * vsp_len_y + iy];
					}
				}
			}
			free(vsp_data_decoded);

			size_t decode_len = vsp_len_y * vsp_len_col * BPP;
			unsigned __int8 *decode_buffer = malloc(decode_len);
			if (decode_buffer == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data_decoded2);
				exit(-2);
			}

			decode2bpp(decode_buffer, vsp_data_decoded2, vsp_len_col, vsp_len_y);
			free(vsp_data_decoded2);

			memset(screen, 0x10, sizeof(screen));


			for (size_t iy = 0; iy < vsp_len_y; iy++) {
				for (size_t ix = 0; ix < vsp_len_x; ix++) {
					screen[vsp_in_y + iy][vsp_in_x + ix] = decode_buffer[iy*vsp_len_x + ix];
				}
			}
			free(decode_buffer);
		}

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
		unsigned __int8 Palette[16][3] = { { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
										  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
										  { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
										  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF } };


		if (is256) {
			for (size_t ci = 0; ci < 256; ci++) {
				pal[ci].blue = hVSP256.Palette[ci][2];
				pal[ci].red = hVSP256.Palette[ci][0];
				pal[ci].green = hVSP256.Palette[ci][1];
			}
		}
		else if (is200l) {
			for (size_t ci = 0; ci < 16; ci++) {
				pal[ci].blue = Palette[ci][0] * 0x11;
				pal[ci].red = Palette[ci][1] * 0x11;
				pal[ci].green = Palette[ci][2] * 0x11;
			}
		}
		else {
			for (size_t ci = 0; ci < 16; ci++) {
				pal[ci].blue = hVSP.Palette[ci][0] * 0x11;
				pal[ci].red = hVSP.Palette[ci][1] * 0x11;
				pal[ci].green = hVSP.Palette[ci][2] * 0x11;
			}
			pal[16].blue = pal[16].red = pal[16].green = 0;
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
		image = (png_bytepp)malloc(rows * sizeof(png_bytep));
		if (image == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			fclose(pFo);
			exit(-2);
		}
		for (size_t j = 0; j < rows; j++)
			image[j] = (png_bytep)&screen[j];

		png_init_io(png_ptr, pFo);
		png_set_IHDR(png_ptr, info_ptr, 640, rows,
			BPP, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_PLTE(png_ptr, info_ptr, pal, color);

		if (!is256) {
			png_byte trans[17] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 00 };
			png_set_tRNS(png_ptr, info_ptr, trans, color, NULL);
		}
		if (is200l) {
			png_set_pHYs(png_ptr, info_ptr, 2, 1, PNG_RESOLUTION_UNKNOWN);
		}
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, image);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		free(image);
		fclose(pFo);
	}
}
