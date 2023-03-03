#include "png.h"
#include "zlib.h"

#pragma pack(push)
#pragma pack(1)
constexpr size_t MSX_SCREEN7_V = 212;
constexpr size_t MSX_SCREEN7_H = 512;
constexpr size_t RES_X = 40000;
constexpr size_t RES_Y = 25000;
constexpr size_t LEN_SECTOR = 256;

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
	unsigned background = 0;

	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	bool enable_offset = false;

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

	void set_offset(png_uint_32 offs_x, png_uint_32 offs_y)
	{
		this->offset_x = offs_x;
		this->offset_y = offs_y;
		this->enable_offset = true;
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

			png_set_pHYs(png_ptr, info_ptr, this->res_x, this->res_y, PNG_RESOLUTION_METER);

			if (this->palette.size() <= 256) {
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

struct PackedPixel4 {
	unsigned __int8	L : 4;
	unsigned __int8 H : 4;
};

union uPackedPixel4 {
	unsigned __int8 B;
	PackedPixel4 S;
};

enum class decode_mode {
	NONE = 0, GE7, LP, LV, GS, GL, Intr, TT, DRS
};
#pragma pack(pop)
