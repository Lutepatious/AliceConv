#ifndef TOPNG_TOPNG
#define TOPNG_TOPNG
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <bitset>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "zlib.h"
#include "png.h"

#pragma pack(push)
#pragma pack(1)

static bool silent = false;

static inline void wouterr(const wchar_t* msg)
{
	if (!silent) {
		std::wcerr << msg << std::endl;
	}
}

static inline void out_image_info(const size_t offset_x, const size_t offset_y, const size_t len_x, const size_t len_y, const wchar_t* type, const size_t extra0 = 0, const size_t extra1 = 0)
{
	std::wcout << L"From " << std::setw(4) <<offset_x << L"," << std::setw(3) << offset_y << L" Size " << std::setw(4) << len_x << L"*" << std::setw(3) << len_y << L" Type:" << type << L" " << std::setw(3) << extra0 << L"," << std::setw(3) << extra1 << std::endl;
}

static inline png_byte d3tod8(png_byte a)
{
	//	png_byte r = (double) (a) * 255.0L / 7.0L + 0.5L;
	png_byte r = ((unsigned)a * 146 + 1) >> 2;
	return r;
}

static inline png_byte d4tod8(png_byte a)
{
	//	png_byte r = (double) (a) * 255.0L / 15.0L + 0.5L;
	png_byte r = (unsigned)a * 17;
	return r;
}

static inline png_byte d5tod8(png_byte a)
{
	//	png_byte r = (double) (a) * 255.0L / 31.0L + 0.5L;
	png_byte r = ((unsigned)a * 527 + 23) >> 6;
	return r;
}

static inline unsigned __int8 d6tod8(unsigned __int8 a)
{
	//	unsigned __int8 r = (double) (a) * 255.0L / 63.0L + 0.5L;
	unsigned __int8 r = ((unsigned)a * 259 + 33) >> 6;
	return r;
}

static inline png_byte d16tod8(png_uint_16 a)
{
	//	png_byte r = ((double) a) * 255.0L / 65535.0L + 0.5L;
	png_byte r = ((unsigned)a * 255L + 32895L) >> 16;
	return  r;
}

constexpr size_t SVGA_V = 600;
constexpr size_t SVGA_H = 800;
constexpr size_t VGA_V = 480;
constexpr size_t VGA_H = 640;
constexpr size_t PC9801_V = 400;
constexpr size_t PC9801_H = 640;
constexpr size_t PC8801_V = 200;
constexpr size_t PC8801_H = 640;
constexpr size_t X68000_G = 512;
constexpr size_t X68000_GX = 768;
constexpr size_t RES = 40000;
constexpr size_t MSX_SCREEN7_V = 212;
constexpr size_t MSX_SCREEN7_H = 512;
constexpr size_t MSX_RES_Y = 20000;
constexpr size_t MSX_RES_X = MSX_RES_Y * 7 / 4;
constexpr size_t LEN_SECTOR = 256;

struct Palette_depth3 {
	unsigned __int16 B : 3;
	unsigned __int16 R : 3;
	unsigned __int16 : 2;
	unsigned __int16 G : 3;
	unsigned __int16 : 5;
};

struct Palette_depth4 {
	unsigned __int8 B : 4;
	unsigned __int8 : 4;
	unsigned __int8 R : 4;
	unsigned __int8 : 4;
	unsigned __int8 G : 4;
	unsigned __int8 : 4;
};

struct Palette_depth4RGB {
	unsigned __int8 R : 4;
	unsigned __int8 : 4;
	unsigned __int8 G : 4;
	unsigned __int8 : 4;
	unsigned __int8 B : 4;
	unsigned __int8 : 4;
};

struct Palette_depth5 {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
};

struct Palette_depth8 {
	unsigned __int8 R;
	unsigned __int8 G;
	unsigned __int8 B;
};

struct MSX_Pal {
	unsigned __int16 B : 3;
	unsigned __int16 : 1;
	unsigned __int16 R : 3;
	unsigned __int16 : 1;
	unsigned __int16 G : 3;
	unsigned __int16 : 5;
};

struct PackedPixel4 {
	unsigned __int8	L : 4;
	unsigned __int8 H : 4;
};

union uPackedPixel4 {
	unsigned __int8 B;
	PackedPixel4 S;
};

struct toPNG {
	std::vector<png_color> palette;
	std::vector<png_byte> trans;
	std::vector<png_bytep> body;

	png_uint_32 pixels_V = PC9801_V;
	png_uint_32 pixels_H = PC9801_H;
	int depth = 8;
	bool indexed = true;
	int res_x = RES;
	int res_y = RES;
	unsigned background = 0;

	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	bool enable_offset = false;

	void set_size_and_change_resolution_MSX(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		res_x = ((MSX_RES_X * in_x * 2) / MSX_SCREEN7_H + 1) >> 1;
		res_y = ((MSX_RES_Y * in_y * 2) / MSX_SCREEN7_V + 1) >> 1;
	}

	void set_size_and_change_resolution_MSX_default(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		res_x = MSX_RES_X;
		res_y = MSX_RES_Y;
	}
	void set_size_and_change_resolution(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		this->res_x = ((RES * in_x * 2) / PC9801_H + 1) >> 1;
		this->res_y = ((RES * in_y * 2) / PC9801_V + 1) >> 1;
	}

	void change_resolution_halfy(void)
	{
		this->res_y >>= 1;
	}

	void change_resolution_x68k_g(void)
	{
		this->res_x = this->res_x * 4 / 5;
		this->res_y = this->res_y * 6 / 5;
	}

	void set_size(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;
	}

	void set_offset(png_uint_32 offs_x, png_uint_32 offs_y)
	{
		this->offset_x = offs_x;
		this->offset_y = offs_y;
		this->enable_offset = true;
	}

	void set_depth(int in_depth)
	{
		this->depth = in_depth;
	}

	void set_directcolor(void)
	{
		this->indexed = false;
	}

	int create(wchar_t* outfile)
	{
		if (body.size()) {

			FILE* pFo;
			errno_t ecode = _wfopen_s(&pFo, outfile, L"wb");
			if (ecode || !pFo) {
				std::wcerr << L"File open error." << outfile << std::endl;
				return -1;
			}
			png_structp png_ptr = NULL;
			png_infop info_ptr = NULL;

			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (png_ptr == NULL) {
				fclose(pFo);
				return -1;
			}

			info_ptr = png_create_info_struct(png_ptr);
			if (info_ptr == NULL) {
				png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
				fclose(pFo);
				return -1;
			}

			png_init_io(png_ptr, pFo);
			png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
			png_set_IHDR(png_ptr, info_ptr, this->pixels_H, this->pixels_V, this->depth, !this->indexed ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

			png_set_pHYs(png_ptr, info_ptr, this->res_x, this->res_y, PNG_RESOLUTION_METER);

			if (this->indexed) {
				png_set_PLTE(png_ptr, info_ptr, &this->palette.at(0), (int)this->palette.size());

				if (!this->trans.empty()) {
					png_set_tRNS(png_ptr, info_ptr, &this->trans.at(0), (int)this->trans.size(), NULL);
				}
			}

			if (this->enable_offset) {
				png_color_16 t;
				t.index = background;
				png_set_bKGD(png_ptr, info_ptr, &t);
				png_set_oFFs(png_ptr, info_ptr, this->offset_x, this->offset_y, PNG_OFFSET_PIXEL);
			}

			png_write_info(png_ptr, info_ptr);
			png_write_image(png_ptr, &this->body.at(0));
			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(pFo);

			return 0;
		}
		else {
			return -1;
		}
	}
};
#pragma pack(pop)
#endif

#include "DAT.hpp"

#include "GL.hpp"
#include "GL3.hpp"
#include "GL_X68K.hpp"

#include "VSP200l.hpp"
#include "VSP.hpp"
#include "X68K_TT.hpp"
#include "X68K_ABZ.hpp"

#include "PMS.hpp"
#include "PMS16.hpp"

#include "DRS003.hpp"
#include "TIFF_TOWNS.hpp"
#include "X68K.hpp"

#include "ICN.hpp"
#include "GAIJI.hpp"

#include "MSX_GE7.hpp"
#include "MSX_LP.hpp"
#include "MSX_LV.hpp"
#include "MSX_GS.hpp"
#include "MSX_GL.hpp"
#include "MSX_Intruder.hpp"
#include "MSX_SG.hpp"
#include "MSX_TT_DRS.hpp"
