#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>

#pragma pack(push)
#pragma pack(1)
constexpr size_t MSX_SCREEN7_ROW = 212;
constexpr size_t MSX_SCREEN7_COLUMN = 512;
constexpr size_t DEFAULT_PpM = 4000;


#include "png.h"
#include "zlib.h"

png_byte d3tod8(unsigned __int8 a)
{
	//	return (png_byte) ((double) (a) * 255.0L / 7.0L + 0.5L);
	return (png_byte)(((unsigned)a * 146 + 1) >> 2);
}

struct fPNGw {
	wchar_t* outfile = nullptr;
	png_bytepp image = nullptr;
	png_colorp Pal = nullptr;
	png_bytep Trans = nullptr;
	png_uint_32 Rows = NULL;
	png_uint_32 Cols = NULL;
	int nPal = 0;
	int nTrans = 0;
	int depth = 0;
	int pXY = 0;

	void init(size_t in_x, size_t in_y, int bpp, int in_asp, std::vector<unsigned __int8>& in_body, std::vector<png_color>& in_pal, std::vector<png_byte>& in_trans)
	{
		this->Rows = in_x;
		this->Cols = in_y;
		this->depth = bpp;
		this->pXY = in_asp;
		this->Pal = this->Pal == nullptr ? nullptr : &in_pal.at(0);
		this->Trans = this->Trans == nullptr ? nullptr : &in_trans.at(0);

		image = (png_bytepp)malloc(sizeof(png_bytep) * this->Rows);
		for (size_t i = 0; i < this->Rows; i++) {
			*(image + i) = (png_bytep) &in_body.at(i * this->Cols);
		}
	}

	int create(void)
	{
		FILE* pFo;
		errno_t ecode = _wfopen_s(&pFo, this->outfile, L"wb");
		if (ecode || !pFo) {
			wprintf_s(L"File open error %s.\n", this->outfile);
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
		png_set_IHDR(png_ptr, info_ptr, this->Cols, this->Rows, this->depth, (this->nPal > 256) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		if ((this->Trans != nullptr) && (this->nPal <= 256)) {
			png_set_tRNS(png_ptr, info_ptr, this->Trans, this->nTrans, NULL);
		}
		png_set_pHYs(png_ptr, info_ptr, this->pXY == 3 ? DEFAULT_PpM * 4 / 5 : DEFAULT_PpM, this->pXY == 2 ? DEFAULT_PpM / 2 : DEFAULT_PpM, PNG_RESOLUTION_METER);
		if (this->nPal <= 256) {
			png_set_PLTE(png_ptr, info_ptr, this->Pal, this->nPal);
		}

		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, this->image);
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
		unsigned __int8 H : 4;
	} body[MSX_SCREEN7_ROW][MSX_SCREEN7_COLUMN / 2];
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
		for (size_t j = 0; j < MSX_SCREEN7_ROW; j++) {
			for (size_t i = 0; i < MSX_SCREEN7_COLUMN / 2; i++) {
				I.push_back(this->body[j][i].H);
				I.push_back(this->body[j][i].L);
			}
		}
		return I;
	}

	std::vector<png_color> decode_palette(void)
	{
		std::vector<png_color> pal;
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

		std::vector<unsigned __int8> out_body = buf->decode_body();
		std::vector<png_color> out_palette = buf->decode_palette();
	}
}