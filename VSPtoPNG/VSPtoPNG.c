#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "png.h"
#include "zlib.h"

#define PLANE 4
#define BPP 8
#define ROWS 480


#pragma pack (1)
struct GL_Palette {
	unsigned __int16 C0 : 3;
	unsigned __int16 C1 : 3;
	unsigned __int16 u0 : 2;
	unsigned __int16 C2 : 3;
	unsigned __int16 u1 : 3;
	unsigned __int16 u2 : 2;
};

struct VSP_header {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	unsigned __int8 Palette[16][3];
} hVSP;

struct VSP256_header {
	unsigned __int16 Column_in;
	unsigned __int16 Row_in;
	unsigned __int16 Column_out;
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[24];
	unsigned __int8 Palette[256][3];
} hVSP256;

struct VSP200l_header {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct GL_Palette Palette[8];
} hVSP200l;

struct GL3_header {
	unsigned __int8 Palette[16][3];
	unsigned __int16 Start;
	unsigned __int16 Columns; // divided by 8
	unsigned __int16 Rows;
} hGL3;

struct GL_header {
	unsigned __int16 Start;
	unsigned __int8 Columns; // divided by 8
	unsigned __int8 Rows;
	unsigned __int16 Unknown[2];
	struct GL_Palette Palette[8];
} hGL;

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

unsigned __int8 Palette_200l[16][3] = { { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
										{ 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
										{ 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
										{ 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF } };

struct image_info {
	unsigned __int8 *image;
	unsigned __int8(*Palette)[256][3];
	size_t start_x;
	size_t start_y;
	size_t len_x;
	size_t len_y;
	unsigned colors;
} iInfo;

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		unsigned is256 = 0, is200l = 0, isGL3 = 0, isGM3 = 0, isGL = 0;

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

		unsigned __int8 t = 0;
		for (int i = 0; i < 0x30; i++) {
			t |= hbuf[i];
		}

		if (t == 0 && hbuf[0x31] & 0x80) {
			wprintf_s(L"Palette not found. Assume 200 Line\n");
			isGM3 = 1;
		}
		else if (t < 0x10 && hbuf[0x31] & 0x80) {
			isGL3 = 1;
		}
		else if ((hbuf[0] & 0xC0) == 0xC0 && hbuf[3] >= 2) {
			isGL = 1;
		}
		else {
			if (hbuf[1] >= 3 || hbuf[5] >= 3 || hbuf[3] >= 2 || hbuf[7] >= 2 || hbuf[8] >= 2) {
				wprintf_s(L"Wrong data exist. %s is not VSP and variants.\n", *argv);
				continue;
			}
			t = 0;
			for (int i = 0; i < 0x10; i++) {
				t |= hbuf[i];
			}
			if (t == 0) {
				wprintf_s(L"Wrong data exist. %s is not VSP and variants.\n", *argv);
				continue;
			}

			size_t i = 0xA;
			do {
				if (hbuf[i] != 0)
					break;
			} while (++i < 0x3A);

			if (hbuf[8] == 1 || (i >= 0x20 && i < 0x37)) {
				is256 = 1;
			}

			unsigned __int16 *p = hbuf;
			if (*p > 0x50)
				is256 = 1;

			if (!is256 && (hbuf[0xA] >= 0x10 || hbuf[0xC] >= 0x10 || hbuf[0xE] >= 0x10 || hbuf[0x10] >= 0x10 ||
				hbuf[0x12] >= 0x10 || hbuf[0x14] >= 0x10 || hbuf[0x16] >= 0x10 || hbuf[0x18] >= 0x10)) {
				is200l = 1;
			}
		}
		if (isGL3 || isGM3) {
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
			wprintf_s(L"%3zu/%3zu GL3 size %zu => %zu.\n", gl3_start_x, gl3_start_y, gl3_len, gl3_len_decoded);
			size_t count = gl3_len, cp_len, cur_plane;
			unsigned __int8 *src = gl3_data, *dst = gl3_data_decoded, *cp_src;
			while (count-- && (dst - gl3_data_decoded) < gl3_len_decoded) {
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

			iInfo.image = decode_buffer;
			iInfo.start_x = gl3_start_x;
			iInfo.start_y = gl3_start_y;
			iInfo.len_x = hGL3.Columns * 8;
			iInfo.len_y = hGL3.Rows;
			iInfo.colors = 16;
			if (isGM3)
				iInfo.Palette = Palette_200l;
			else
				iInfo.Palette = hGL3.Palette;
		}
		else if (isGL) {
			size_t rcount = fread_s(&hGL, sizeof(hGL), sizeof(hGL), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}

			size_t gl_len = fs.st_size - sizeof(hGL);
			unsigned __int8 *gl_data = malloc(gl_len);
			if (gl_data == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				fclose(pFi);
				exit(-2);
			}

			rcount = fread_s(gl_data, gl_len, 1, gl_len, pFi);
			if (rcount != gl_len) {
				wprintf_s(L"File read error %s %d.\n", *argv, rcount);
				free(gl_data);
				fclose(pFi);
				exit(-2);
			}
			fclose(pFi);


			size_t gl_start = _byteswap_ushort(hGL.Start) & 0x3FFF;
			size_t gl_start_x = gl_start % 80 * 8;
			size_t gl_start_y = gl_start / 80;
			size_t gl_len_decoded = hGL.Rows * hGL.Columns * (PLANE - 1);
			unsigned __int8 *gl_data_decoded = malloc(gl_len_decoded);
			if (gl_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(gl_data);
				exit(-2);
			}
			wprintf_s(L"Start %zu/%zu %zu*%zu GL size %zu => %zu.\n", gl_start_x, gl_start_y, hGL.Columns * 8, hGL.Rows, gl_len, gl_len_decoded);

			size_t count = gl_len, cp_len, cur_plane;
			unsigned __int8 *src = gl_data, *dst = gl_data_decoded, *cp_src;
			while (count-- && (dst - gl_data_decoded) < gl_len_decoded) {
				switch (*src) {
				case 0x00:
					cp_len = *(src + 1) & 0x7F;
					if (*(src + 1) & 0x80) {
						cp_src = dst - hGL.Columns * (PLANE - 1) * 4 + 1;
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
				case 0x0E:
					cp_len = *src - 6;
					cp_src = dst - hGL.Columns * (PLANE - 1) * 4 + 1;
					memset(dst, *cp_src, cp_len);
					dst += cp_len;
					src++;
					break;
				case 0x0F:
					cur_plane = ((dst - gl_data_decoded) / hGL.Columns) % (PLANE - 1);
					cp_len = *(src + 1) & 0x7F;
					cp_src = dst - hGL.Columns * ((*(src + 1) & 0x80) ? cur_plane - 1 : cur_plane);
					memcpy_s(dst, cp_len, cp_src, cp_len);
					dst += cp_len;
					src += 2;
					count--;
					break;

				default:
					*dst++ = *src++;
				}
			}

			free(gl_data);

			size_t decode_len = hGL.Rows * hGL.Columns * BPP;
			unsigned __int8 *decode_buffer = malloc(decode_len);
			if (decode_buffer == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(gl_data_decoded);
				exit(-2);
			}

			decode2bpp_3(decode_buffer, gl_data_decoded, hGL.Columns, hGL.Rows);
			free(gl_data_decoded);

			iInfo.image = decode_buffer;
			iInfo.start_x = gl_start_x;
			iInfo.start_y = gl_start_y;
			iInfo.len_x = hGL.Columns * 8;
			iInfo.len_y = hGL.Rows;
			iInfo.colors = 16;
		}
		else if (is256) {
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

			wprintf_s(L"%3zu/%3zu - %3zu/%3zu VSP256 %d:%d size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, hVSP256.Unknown[0], hVSP256.Unknown[1], vsp_len, vsp_len_decoded);

			size_t count = vsp_len, cp_len;
			unsigned __int8 *src = vsp_data, *dst = vsp_data_decoded, *cp_src;

			while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
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

			iInfo.image = vsp_data_decoded;
			iInfo.start_x = vsp_in_x;
			iInfo.start_y = vsp_in_y;
			iInfo.len_x = vsp_len_x;
			iInfo.len_y = vsp_len_y;
			iInfo.colors = 256;
			iInfo.Palette = hVSP256.Palette;
		}
		else if (is200l) {
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
			const size_t vsp_len_decoded = vsp_len_y * vsp_len_col * (PLANE - 1);

			unsigned __int8 *vsp_data_decoded = malloc(vsp_len_decoded);
			if (vsp_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			wprintf_s(L"%3zu/%3zu - %3zu/%3zu VSP200l %d:%d size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, hVSP200l.Unknown[0], hVSP200l.Unknown[1], vsp_len, vsp_len_decoded);

			size_t count = vsp_len, cp_len, cur_plane;
			unsigned __int8 *src = vsp_data, *dst = vsp_data_decoded, *cp_src, negate = 0;

			while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
				switch (*src) {
				case 0x00:
					cp_len = *(src + 1) + 1;
					cp_src = dst - vsp_len_y * (PLANE - 1);
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
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % (PLANE - 1);
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
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % (PLANE - 1);
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
					cur_plane = ((dst - vsp_data_decoded) / vsp_len_y) % (PLANE - 1);
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
				for (size_t ip = 0; ip < (PLANE - 1); ip++) {
					for (size_t iy = 0; iy < vsp_len_y; iy++) {
						vsp_data_decoded2[iy*vsp_len_col*(PLANE - 1) + ip * vsp_len_col + ix] = vsp_data_decoded[ix*vsp_len_y*(PLANE - 1) + ip * vsp_len_y + iy];
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

			iInfo.image = decode_buffer;
			iInfo.start_x = vsp_in_x;
			iInfo.start_y = vsp_in_y;
			iInfo.len_x = vsp_len_x;
			iInfo.len_y = vsp_len_y;
			iInfo.colors = 16;
		}
		else {
			rcount = fread_s(&hVSP, sizeof(hVSP), sizeof(hVSP), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}

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

			if (vsp_len_decoded == 4) {
				wprintf_s(L"Too short. Skip!\n");
				continue;
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

			unsigned __int8 *vsp_data_decoded = malloc(vsp_len_decoded);
			if (vsp_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			wprintf_s(L"%3zu/%3zu - %3zu/%3zu VSP %d:%d size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, hVSP.Unknown[0], hVSP.Unknown[1], vsp_len, vsp_len_decoded);

			size_t count = vsp_len, cp_len, cur_plane;
			unsigned __int8 *src = vsp_data, *dst = vsp_data_decoded, *cp_src, negate = 0;

			while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
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

			iInfo.image = decode_buffer;
			iInfo.start_x = vsp_in_x;
			iInfo.start_y = vsp_in_y;
			iInfo.len_x = vsp_len_x;
			iInfo.len_y = vsp_len_y;
			iInfo.colors = 16;
			iInfo.Palette = hVSP.Palette;
		}

		size_t canvas_x = (iInfo.start_x + iInfo.len_x) > 640 ? (iInfo.start_x + iInfo.len_x) : 640;
		size_t canvas_y = 400;
		unsigned __int8 t_color = 0x10;

		if (isGM3 || isGL3 || is256)
			t_color = 0;
		else if (is200l || isGL)
			t_color = 8;


		if (is200l || isGL)
			canvas_y = 200;
		else if (isGM3)
			canvas_y = 201;

		canvas_y = (iInfo.start_y + iInfo.len_y) > canvas_y ? (iInfo.start_y + iInfo.len_y) : canvas_y;

		unsigned __int8 *canvas;
		canvas = malloc(canvas_y*canvas_x);
		if (canvas == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(iInfo.image);
			exit(-2);
		}

		memset(canvas, t_color, canvas_y*canvas_x);
		for (size_t iy = 0; iy < iInfo.len_y; iy++) {
			memcpy_s(&canvas[(iInfo.start_y + iy)*canvas_x + iInfo.start_x], iInfo.len_x, &iInfo.image[iy*iInfo.len_x], iInfo.len_x);
		}
		free(iInfo.image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		ecode = _wfopen_s(&pFo, path, L"wb");
		if (ecode) {
			free(canvas);
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		png_structp png_ptr;
		png_infop info_ptr;
		png_color pal[256] = { {0,0,0} };

		if (is256) {
			for (size_t ci = 0; ci < 256; ci++) {
				pal[ci].blue = (*iInfo.Palette)[ci][2];
				pal[ci].red = (*iInfo.Palette)[ci][0];
				pal[ci].green = (*iInfo.Palette)[ci][1];
			}
		}
		else if (is200l) {
			for (size_t ci = 0; ci < 8; ci++) {
				pal[ci].blue = (hVSP200l.Palette[ci].C0 * 0x24) | ((hVSP200l.Palette[ci].C0 & 4) ? 1 : 0);
				pal[ci].red = (hVSP200l.Palette[ci].C1 * 0x24) | ((hVSP200l.Palette[ci].C1 & 4) ? 1 : 0);
				pal[ci].green = (hVSP200l.Palette[ci].C2 * 0x24) | ((hVSP200l.Palette[ci].C2 & 4) ? 1 : 0);
			}
			pal[8].blue = pal[8].red = pal[8].green = 0;
		}
		else if (isGL) {
			for (size_t ci = 0; ci < 8; ci++) {
				pal[ci].blue = (hGL.Palette[ci].C0 * 0x24) | ((hGL.Palette[ci].C0 & 4) ? 1 : 0);
				pal[ci].red = (hGL.Palette[ci].C1 * 0x24) | ((hGL.Palette[ci].C1 & 4) ? 1 : 0);
				pal[ci].green = (hGL.Palette[ci].C2 * 0x24) | ((hGL.Palette[ci].C2 & 4) ? 1 : 0);
			}
			pal[8].blue = pal[8].red = pal[8].green = 0;
		}
		else {
			for (size_t ci = 0; ci < 16; ci++) {
				pal[ci].blue = (*iInfo.Palette)[ci][0] * 0x11;
				pal[ci].red = (*iInfo.Palette)[ci][1] * 0x11;
				pal[ci].green = (*iInfo.Palette)[ci][2] * 0x11;
			}
		}

		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == NULL) {
			free(canvas);
			fclose(pFo);
			return;
		}
		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL) {
			png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
			free(canvas);
			fclose(pFo);
			return;
		}

		unsigned __int8 **image;
		image = (png_bytepp)malloc(canvas_y * sizeof(png_bytep));
		if (image == NULL) {
			fprintf_s(stderr, "Memory allocation error.\n");
			free(canvas);
			fclose(pFo);
			exit(-2);
		}
		for (size_t j = 0; j < canvas_y; j++)
			image[j] = (png_bytep)&canvas[j * canvas_x];

		png_init_io(png_ptr, pFo);
		png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
		png_set_IHDR(png_ptr, info_ptr, canvas_x, canvas_y,
			BPP, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		if (isGL3 || isGM3) {
			png_byte trans[16] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
			png_set_tRNS(png_ptr, info_ptr, trans, iInfo.colors, NULL);
		}
		else if (is200l || isGL) {
			png_byte trans[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								   0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
			png_set_tRNS(png_ptr, info_ptr, trans, iInfo.colors, NULL);
		}
		else if (t_color == 0x10) {
			iInfo.colors = 17;
			png_byte trans[17] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
			png_set_tRNS(png_ptr, info_ptr, trans, iInfo.colors, NULL);
		}

		if (is200l || isGM3 || isGL)
			png_set_pHYs(png_ptr, info_ptr, 2, 1, PNG_RESOLUTION_UNKNOWN);

		png_set_PLTE(png_ptr, info_ptr, pal, iInfo.colors);
		//		png_set_oFFs(png_ptr, info_ptr, iInfo.start_x, iInfo.start_y, PNG_OFFSET_PIXEL);
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, image);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		free(image);
		free(canvas);
		fclose(pFo);
	}
}
