#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#pragma pack(push)
#pragma pack(1)
constexpr size_t MSX_SCREEN7_V = 212;
constexpr size_t MSX_SCREEN7_H = 512;
constexpr size_t RES_X = 40000;
constexpr size_t RES_Y = 25000;
constexpr size_t LEN_SECTOR = 256;

#include "png.h"
#include "zlib.h"

png_byte d3tod8(unsigned __int8 a)
{
	//	return (png_byte)((double) (a) * 255.0L / 7.0L + 0.5L);
	return (png_byte)(((unsigned)a * 146 + 1) >> 2);
}

struct MSXtoPNG {
	std::vector<png_color> palette;
	std::vector<png_byte> trans;
	std::vector<png_bytep> body;

	png_uint_32 pixels_V = MSX_SCREEN7_V;
	png_uint_32 pixels_H = MSX_SCREEN7_H;
	int depth = 8;
	int res_x = RES_X;
	int res_y = RES_Y;

	void set_size_and_change_resolution(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		res_x = ((RES_X * in_x * 2) / MSX_SCREEN7_H + 1) >> 1;
		res_y = ((RES_Y * in_y * 2) / MSX_SCREEN7_V + 1) >> 1;
	}

	void set_size(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;
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
			png_set_IHDR(png_ptr, info_ptr, this->pixels_H, this->pixels_V, this->depth, (this->palette.size() > 256) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			if (!this->trans.empty() && (this->palette.size() <= 256)) {
				png_set_tRNS(png_ptr, info_ptr, &this->trans.at(0), this->trans.size(), NULL);
			}
			png_set_pHYs(png_ptr, info_ptr, this->res_x, this->res_y, PNG_RESOLUTION_METER);
			if (this->palette.size() <= 256) {
				png_set_PLTE(png_ptr, info_ptr, &this->palette.at(0), this->palette.size());
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

struct format_GE7 {
	unsigned __int8 Id;
	unsigned __int16 start;
	unsigned __int16 length;
	unsigned __int16 unknown;
	struct PackedPixel4 {
		unsigned __int8	L : 4;
		unsigned __int8 H : 4;
	} body[MSX_SCREEN7_V][MSX_SCREEN7_H / 2];
	unsigned __int8 unused[0x1C00];
	unsigned __int8 splite_generator[0x800];
	unsigned __int8 splite_color[0x200];
	unsigned __int8 splite_attribute[32][4];
	struct Pal4 {
		unsigned __int16 B : 3;
		unsigned __int16 : 1;
		unsigned __int16 R : 3;
		unsigned __int16 : 1;
		unsigned __int16 G : 3;
		unsigned __int16 : 5;
	} palette[16];
};

class GE7 {
	std::vector<unsigned __int8> I;

public:
	format_GE7* buf = nullptr;

	void decode_body(std::vector<png_bytep>& out_body)
	{
		// 失敗談 各ラインがデコードされ次第PNG出力の構造体にアドレスをプッシュするコードにしていたが
		// std::vectorはpush_backでサイズが変わっていくと再割り当てを行うので最終的にアドレスが変わってしまう。
		// このことを忘れててデータ壊しまくってた
		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			for (size_t i = 0; i < MSX_SCREEN7_H / 2; i++) {
				I.push_back(this->buf->body[j][i].H);
				I.push_back(this->buf->body[j][i].L);
			}
		}
		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * MSX_SCREEN7_H));
		}
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->buf->palette[i].R);
			c.green = d3tod8(this->buf->palette[i].G);
			c.blue = d3tod8(this->buf->palette[i].B);
			pal.push_back(c);
		}
	}
};

struct format_LP {
	unsigned __int8 body[][0x100];
};

class LP {
	std::vector<unsigned __int8> I;
	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} palette[8] =
	{ { 0x0, 0x0, 0x0 }, { 0x0, 0x0, 0x7 }, { 0x0, 0x7, 0x0 }, { 0x0, 0x7, 0x7 },
	  { 0x7, 0x0, 0x0 }, { 0x7, 0x0, 0x7 }, { 0x7, 0x7, 0x0 }, { 0x7, 0x7, 0x7 } };

public:
	format_LP* buf = nullptr;

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 8; i++) {
			c.red = d3tod8(this->palette[i].R);
			c.green = d3tod8(this->palette[i].G);
			c.blue = d3tod8(this->palette[i].B);
			pal.push_back(c);
		}
	}
};

enum class decode_mode {
	NONE = 0, GE7, LP, LV, GS, R1, I, TT, DRS
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
			if (*(*argv + 1) == L'b') { // MSX BSAVE (GE7)
				dm = decode_mode::GE7;
			}
			else if (*(*argv + 1) == L'l') { // Little Princess
				dm = decode_mode::LP;
			}
			else if (*(*argv + 1) == L'v') { // Little Vampire
				dm = decode_mode::LV;
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

		MSXtoPNG out;
		GE7 ge7;

		switch (dm) {
		case decode_mode::GE7:
			if (inbuf.size() < sizeof(format_GE7)) {
				std::wcerr << "File too short. " << *argv << std::endl;
				continue;
			}

			ge7.buf = (format_GE7*)&inbuf.at(0);

			if (ge7.buf->Id != 0xFE || ge7.buf->start != 0 || ge7.buf->length != 0xFAA0 || ge7.buf->unknown != 0) {
				std::wcerr << "File type not match. " << *argv << std::endl;
				continue;
			}

			ge7.decode_palette(out.palette);
			ge7.decode_body(out.body);
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
}