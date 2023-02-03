#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>

#pragma pack(push)
#pragma pack(1)
constexpr size_t MSX_SCREEN7_V = 212;
constexpr size_t MSX_SCREEN7_H = 512;


#include "png.h"
#include "zlib.h"

png_byte d3tod8(unsigned __int8 a)
{
	//	return (png_byte)((double) (a) * 255.0L / 7.0L + 0.5L);
	return (png_byte)(((unsigned)a * 146 + 1) >> 2);
}

struct MSXtoPNG {
	std::vector<png_color> pal;
	std::vector<png_byte> trans;
	std::vector<png_bytep> body;

	png_uint_32 pixels_V = MSX_SCREEN7_V;
	png_uint_32 pixels_H = MSX_SCREEN7_H;
	int depth = 8;
	int res_x = 40000;
	int res_y = 25000;


	void set_size(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		res_x = (((size_t)40000ULL * in_x * 2) / MSX_SCREEN7_H + 1) >> 1;
		res_y = (((size_t)25000ULL * in_y * 2) / MSX_SCREEN7_V + 1) >> 1;
	}

	void init(std::vector<unsigned __int8>& in_body)
	{
		for (size_t i = 0; i < this->pixels_V; i++) {
			this->body.push_back((png_bytep)&in_body.at(i * this->pixels_H));
		}
	}

	int create(wchar_t * outfile)
	{
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
		png_set_IHDR(png_ptr, info_ptr, this->pixels_H, this->pixels_V, this->depth, (this->pal.size() > 256) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		if (!this->trans.empty() && (this->pal.size() <= 256)) {
			png_set_tRNS(png_ptr, info_ptr, &this->trans.at(0), this->trans.size(), NULL);
		}
		png_set_pHYs(png_ptr, info_ptr, this->res_x, this->res_y, PNG_RESOLUTION_METER);
		if (this->pal.size() <= 256) {
			png_set_PLTE(png_ptr, info_ptr, &this->pal.at(0), this->pal.size());
		}

		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, &this->body.at(0));
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(pFo);

		return 0;
	}
};

class GE7 {
	unsigned __int8 Id;
	unsigned __int16 start;
	unsigned __int16 length;
	unsigned __int16 unknown;
	struct PackedPixel4 {
		unsigned __int8	L : 4;
		unsigned __int8 pixels_H : 4;
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

public:
	std::vector<unsigned __int8> decode_body(void)
	{
		std::vector<unsigned __int8> I;
		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			for (size_t i = 0; i < MSX_SCREEN7_H / 2; i++) {
				I.push_back(this->body[j][i].pixels_H);
				I.push_back(this->body[j][i].L);
			}
		}
		return I;
	}

	std::vector<png_color> decode_palette(std::vector<png_color> &pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->palette[i].R);
			c.green = d3tod8(this->palette[i].G);
			c.blue = d3tod8(this->palette[i].B);
			pal.push_back(c);
		}
		return pal;
	}
};

#pragma pack(pop)

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	while (--argc) {
		std::ifstream infile(*++argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		class GE7* buf = (class GE7*)&inbuf.at(0);

		if (inbuf.size() < sizeof(class GE7)) {
			std::wcerr << "File too short. " << *argv << std::endl;
			continue;
		}

		MSXtoPNG out;

		std::vector<unsigned __int8> out_body = buf->decode_body();
		buf->decode_palette(out.pal);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		out.init(out_body);
		out.create(path);
	}
}