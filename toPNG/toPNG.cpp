#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <bitset>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "zlib.h"
#include "png.h"

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

static inline png_byte d16tod8(png_uint_16 a)
{
	//	png_byte r = ((double) a) * 255.0L / 65535.0L + 0.5L;
	png_byte r = ((unsigned)a * 255L + 32895L) >> 16;
	return  r;
}


#pragma pack(push)
#pragma pack(1)

constexpr size_t VGA_V = 480;
constexpr size_t VGA_H = 640;
constexpr size_t PC9801_V = 400;
constexpr size_t PC9801_H = 640;
constexpr size_t PC8801_V = 200;
constexpr size_t PC8801_H = 640;
constexpr size_t RES = 40000;

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

struct Palette_depth5 {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
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

	void set_size_and_change_resolution(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		res_x = ((RES * in_x * 2) / PC9801_H + 1) >> 1;
		res_y = ((RES * in_y * 2) / PC9801_V + 1) >> 1;
	}

	void change_resolution_halfy(void)
	{
		res_y >>= 1;
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
				png_set_PLTE(png_ptr, info_ptr, &this->palette.at(0), this->palette.size());

				if (!this->trans.empty()) {
					png_set_tRNS(png_ptr, info_ptr, &this->trans.at(0), this->trans.size(), NULL);
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

#include "DRS003.hpp"
#include "TIFF_TOWNS.hpp"
#include "X68K.hpp"
#include "GL.hpp"
#include "GL3.hpp"

class GL_X68K {
	std::vector<unsigned __int8> I;

	struct format_GL_X68K {
		unsigned __int8 Sig;
		unsigned __int8 unk0;
		struct Palette_depth4 Pal4[16];
		unsigned __int16 start_hx; // divided by 2
		unsigned __int16 start_y;
		unsigned __int16 len_hx; // divided by 2
		unsigned __int16 len_y;
		unsigned __int16 unk1;
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC9801_H;
	png_uint_32 len_y = PC9801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	unsigned transparent = 16;
	png_uint_32 disp_y = PC9801_V;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_GL_X68K)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		unsigned __int8 t = 0;
		for (int i = 2; i < 0x32; i++) {
			t |= buffer.at(i);
		}

		if (t >= 0x10) {
			std::wcerr << "Wrong palette." << std::endl;
			return true;
		}

		this->buf = (format_GL_X68K*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (this->buf->Sig != 0x11) {
			std::wcerr << "Wrong Signature." << std::endl;
			return true;

		}

		if (this->buf->len_hx == 0 || this->buf->len_y == 0) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		this->len_col = this->buf->len_hx;
		this->len_x = 2 * this->buf->len_hx;
		this->len_y = this->buf->len_y;
		this->offset_x = 2 * this->buf->start_hx;
		this->offset_y = this->buf->start_y;

		if (((size_t)this->len_x + this->offset_x > PC9801_H) || ((size_t)this->len_y + this->offset_y) > PC9801_V) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		std::wcout << L"From " << std::setw(4) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" Size " << std::setw(4) << len_x << L"," << std::setw(3) << len_y << std::endl;
		return false;
	}


};



class VSP { // VSPのVSはVertical Scanの事か? 各プレーンの8ドット(1バイト)を縦にスキャンしてデータが構成されている
	std::vector<unsigned __int8> I;
};

enum class decode_mode {
	NONE = 0, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS8, PMS16, QNT, X68R, X68T, X68V, TIFF_TOWNS, DRS_CG003, DRS_CG003_TOWNS, DRS_OPENING_TOWNS, SPRITE_X68K, MASK_X68K
};

#pragma pack(pop)

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	enum class decode_mode dm = decode_mode::NONE;

	while (--argc) {
		if (**++argv == L'-') {
			// already used: sSOYPMghv

			if (*(*argv + 1) == L's') { // Dr.STOP! CG003
				dm = decode_mode::DRS_CG003;
			}
			else if (*(*argv + 1) == L'S') { // Dr.STOP! FM TOWNS CG003
				dm = decode_mode::DRS_CG003_TOWNS;
			}
			else if (*(*argv + 1) == L'O') { // Dr.STOP! FM TOWNS OPENING
				dm = decode_mode::DRS_OPENING_TOWNS;
			}
			else if (*(*argv + 1) == L'Y') { // ALICEの館CD他TIFF FM TOWNS
				dm = decode_mode::TIFF_TOWNS;
			}
			else if (*(*argv + 1) == L'P') { // 闘神都市 X68000 PCG
				dm = decode_mode::SPRITE_X68K;
			}
			else if (*(*argv + 1) == L'M') { // 闘神都市 X68000 Attack effect mask
				dm = decode_mode::MASK_X68K;
			}
			else if (*(*argv + 1) == L'g') { // GL 
				dm = decode_mode::GL;
			}
			else if (*(*argv + 1) == L'h') { // GL3, GM3
				dm = decode_mode::GL3;
			}
			else if (*(*argv + 1) == L'R') { // GL_X68K RanceII
				dm = decode_mode::X68R;
			}
			else if (*(*argv + 1) == L'v') { // VSP
				dm = decode_mode::VSP;
			}
			continue;
		}

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		toPNG out;
		DRS003 drs;
		DRS003T drst;
		DRSOPNT drsot;
		TIFFT tifft;
		SPRITE spr;
		MASK mask;
		GL gl;
		GL3 gl3;
		GL_X68K glx68k;

		switch (dm) {
		case decode_mode::DRS_CG003:
			if (drs.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			drs.decode_palette(out.palette);
			drs.decode_body(out.body);
			out.set_size(PC9801_V, PC9801_H);
			break;

		case decode_mode::DRS_CG003_TOWNS:
			if (drst.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			drst.decode_palette(out.palette);
			drst.decode_body(out.body);
			out.set_size(PC9801_V, PC9801_H);
			break;

		case decode_mode::DRS_OPENING_TOWNS:
			if (drsot.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			drsot.decode_palette(out.palette);
			drsot.decode_body(out.body);
			out.set_size(VGA_H, VGA_V);
			break;

		case decode_mode::TIFF_TOWNS:
			if (tifft.init(*argv)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			tifft.decode_palette(out.palette);
			if (tifft.decode_body(out.body)) {
				out.set_directcolor();
			}
			out.set_depth(tifft.depth);
			out.set_size(tifft.Cols, tifft.Rows);
			break;

		case decode_mode::SPRITE_X68K:
			if (spr.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			spr.decode_palette(out.palette, out.trans);
			spr.decode_body(out.body);
			out.set_size(16 * 16, 16 * 128 / 16);
			break;

		case decode_mode::MASK_X68K:
			if (mask.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			mask.decode_palette(out.palette, out.trans);
			mask.decode_body(out.body);
			out.set_size(5 * 8 * 14, 24 * 70 / 14);
			break;

		case decode_mode::GL:
			if (gl.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			gl.decode_palette(out.palette, out.trans);
			gl.decode_body(out.body);
			out.set_size_and_change_resolution(PC8801_H, PC8801_V);
			break;

		case decode_mode::GL3:
			if (gl3.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			gl3.decode_palette(out.palette, out.trans);
			if (gl3.decode_body(out.body)) {
				out.change_resolution_halfy();
			}
			out.set_size(PC9801_H, gl3.disp_y);
			break;

		case decode_mode::X68R:
			if (glx68k.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
//			glx68k.decode_palette(out.palette, out.trans);
//			glx68k.decode_body(out.body);
//			out.set_size_and_change_resolution(PC8801_H, PC8801_V);
			break;

		default:
			break;
		}

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		int result = out.create(path);
		if (result) {
			std::wcerr << L"output failed." << std::endl;
		}
	}

	return 0;
}
