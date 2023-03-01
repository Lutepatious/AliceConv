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

class GE7 {
	std::vector<unsigned __int8> I;
	struct format_GE7 {
		unsigned __int8 Id;
		unsigned __int16 start;
		unsigned __int16 length;
		unsigned __int16 unknown;
		PackedPixel4 body[MSX_SCREEN7_V][MSX_SCREEN7_H / 2];
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
	} *buf = nullptr;

public:

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_GE7)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_GE7*)&buffer.at(0);

		if (this->buf->Id != 0xFE || this->buf->start != 0 || this->buf->length != 0xFAA0 || this->buf->unknown != 0) {
			std::wcerr << "File type not match." << std::endl;
			return true;
		}

		return false;
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
};

class LP {
	std::vector<unsigned __int8> I;
	struct format_LP {
		unsigned __int8 body[][LEN_SECTOR];
	} *buf = nullptr;
	size_t sectors = 0;

	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} palette[8] =
	{ { 0x0, 0x0, 0x0 }, { 0x0, 0x0, 0x7 }, { 0x0, 0x7, 0x0 }, { 0x0, 0x7, 0x7 },
	  { 0x7, 0x0, 0x0 }, { 0x7, 0x0, 0x7 }, { 0x7, 0x7, 0x0 }, { 0x7, 0x7, 0x7 } };

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() % LEN_SECTOR) {
			std::wcerr << L"File size mismatch. " << std::endl;
			return true;
		}

		this->buf = (format_LP*)&buffer.at(0);
		this->sectors = buffer.size() / LEN_SECTOR;

		return false;
	}

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

	void decode_body(std::vector<png_bytep>& out_body)
	{
		//		std::wcout << this->sectors << L" sectors." << std::endl;

		for (size_t i = 0; i < this->sectors; i++) {
			if (*((size_t*)&this->buf->body[i][0]) == 0x0101010101010101ULL) {
				//				std::wcout << L"decode end." << std::endl;

				break;
			}

			bool repeat = false;
			unsigned __int8 prev = ~this->buf->body[i][0];

			for (size_t j = 0; j < LEN_SECTOR; j++) {
				if (repeat) {
					repeat = false;
					int cp_len = (int)this->buf->body[i][j] - 2; // range -2 to 253. Minus means cancell previous data. 
					if (cp_len > 0)
					{
						uPackedPixel4 u = { prev };

						for (size_t k = 0; k < cp_len; k++) {
							I.push_back(u.S.H);
							I.push_back(u.S.L);
						}
					}
					else if (cp_len < 0) {
						I.erase(I.end() + (__int64)cp_len * 2, I.end());
					}
					prev = ~this->buf->body[i][j + 1];
				}
				else {
					if (this->buf->body[i][j] == prev) {
						repeat = true;
					}
					else {
						repeat = false;
					}
					prev = this->buf->body[i][j];
					uPackedPixel4 u = { prev };

					I.push_back(u.S.H);
					I.push_back(u.S.L);
				}
			}
		}

		this->len_y = I.size() / MSX_SCREEN7_H;

		//		std::wcout << I.size() << L" bytes. " << this->len_y << L" lines." << std::endl;

		size_t offset_x = 0;
		if (this->len_y == 139 || this->len_y == 140) {
			offset_x = 8;
			this->len_x = 352;
		}
		if (this->len_y == 141) {
			this->len_y = 140;
			this->len_x = 352;
		}

		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * MSX_SCREEN7_H + offset_x));
		}
	}
};

class LV {
	std::vector<unsigned __int8> I;
	struct format_LV {
		unsigned __int8 Id;
		unsigned __int8 NegCols; // ~(Cols - 1)
		unsigned __int8 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;

	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} palette[8] =
	{ { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
	  { 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 } };

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;

	bool init(std::vector<__int8>& buffer)
	{
		this->buf = (format_LV*)&buffer.at(0);

		if (this->buf->Id != 0xEE) {
			std::wcerr << "File type not match." << std::endl;
			return true;
		}
		if (this->buf->NegCols == 1) {
			this->len_x = MSX_SCREEN7_H;
		}
		else {
			this->len_x = (256 - this->buf->NegCols) * 2;
		}
		this->len_y = this->buf->len_y;

		std::wcout << this->len_x << L"," << this->len_y << std::endl;

		return false;
	}

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

	void decode_body(std::vector<png_bytep>& out_body)
	{
		unsigned __int8* src = this->buf->body, prev = ~*src;
		bool repeat = false;

		while (*(unsigned __int16*)src != 0x1AFF) {
			if ((src - &this->buf->Id) % 0x100 == 0) {
				repeat = false;
				prev = ~*src;
			}

			if (*(unsigned __int16*)src == 0xFFF3) {
				src += 2;
			}
			else if (repeat) {
				repeat = false;
				int cp_len = *src - 2; // range -2 to 253. Minus means cancell previous data. 
				if (cp_len > 0) {
					uPackedPixel4 u = { prev };

					for (size_t k = 0; k < cp_len; k++) {
						I.push_back(u.S.H);
						I.push_back(u.S.L);
					}
				}
				else if (cp_len < 0) {
					I.erase(I.end() + (__int64)cp_len * 2, I.end());
				}
				src++;
				prev = ~*src;
			}
			else if (*src == 0x88) {
				prev = *src;
				src++;
			}
			else {
				if (*src == prev) {
					repeat = true;
				}
				else {
					repeat = false;
				}
				prev = *src;
				uPackedPixel4 u = { prev };

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
			}
		}
		//		std::wcout << I.size() << std::endl;
		if (this->len_x == MSX_SCREEN7_H) {
			this->len_y = I.size() / MSX_SCREEN7_H;
		}

		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}
	}
};

class GS {
	std::vector<unsigned __int8> I;
	struct format_GS {
		unsigned __int8 len_hx; // divided by 2
		unsigned __int8 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;
	size_t len_buf = 0;
	size_t color_num = 0;

	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} palette[16] =
	{ { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
	  { 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 },
	  { 0x4, 0x7, 0x6 }, { 0x6, 0x7, 0x6 }, { 0x4, 0x4, 0x4 }, { 0x6, 0x6, 0x6 },
	  { 0x1, 0x1, 0x4 }, { 0x5, 0x6, 0x2 }, { 0x5, 0x5, 0x5 }, { 0x7, 0x7, 0x7 } };

	struct RXx {
		struct {
			struct {
				unsigned __int8 F;
				unsigned __int8 X[3];
				unsigned __int8 Y[3];
				struct {
					unsigned __int8 B;
					unsigned __int8 R;
					unsigned __int8 G;
				} P[6];
				unsigned __int8 O[4][3];
			} Entry[6];
			unsigned __int8 F[34];
		} Sector[50];
	};

	struct RXx_decoded {
		size_t offset_X;
		size_t offset_Y;
		size_t begin_X;
		size_t begin_Y;
		size_t end_X;
		size_t end_Y;
		Pal palette[6];
		unsigned __int8 flag;
	};

	std::vector<RXx_decoded> R;

	bool is_end(unsigned __int8* pos) {
		if (*pos == 0xFF) {
			size_t remain = this->len_buf - (pos - &this->buf->len_hx);

			while (--remain) {
				if (*++pos) {
					return false;
				}
			}
			return true;
		}

		return false;
	}

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer, std::vector<__int8>& palette_buffer, size_t num)
	{
		this->color_num = num;
		this->buf = (format_GS*)&buffer.at(0);
		this->len_buf = buffer.size();
		RXx* rxx = (RXx*)&palette_buffer.at(0);

		this->len_x = this->buf->len_hx ? (size_t)this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		if (this->len_y > 212) {
			return true;
		}
		unsigned __int8 xC[4] = { 0, 0, 0, 0 };
		unsigned __int8 yC[4] = { 0, 0, 0, 0 };
		unsigned __int8 C[2] = { 0, 0 };

		RXx_decoded d;
		for (size_t i = 0; i < 50; i++) {
			for (size_t j = 0; j < 6; j++) {
				char* t;
				C[0] = rxx->Sector[i].Entry[j].F;
				d.flag = strtoull((const char*)C, &t, 10);
				xC[0] = rxx->Sector[i].Entry[j].X[0];
				xC[1] = rxx->Sector[i].Entry[j].X[1];
				xC[2] = rxx->Sector[i].Entry[j].X[2];
				yC[0] = rxx->Sector[i].Entry[j].Y[0];
				yC[1] = rxx->Sector[i].Entry[j].Y[1];
				yC[2] = rxx->Sector[i].Entry[j].Y[2];
				d.offset_X = strtoull((const char*)xC, &t, 10);
				d.offset_Y = strtoull((const char*)yC, &t, 10);
				xC[0] = rxx->Sector[i].Entry[j].O[0][0];
				xC[1] = rxx->Sector[i].Entry[j].O[0][1];
				xC[2] = rxx->Sector[i].Entry[j].O[0][2];
				yC[0] = rxx->Sector[i].Entry[j].O[1][0];
				yC[1] = rxx->Sector[i].Entry[j].O[1][1];
				yC[2] = rxx->Sector[i].Entry[j].O[1][2];
				d.begin_X = strtoull((const char*)xC, &t, 10);
				d.begin_Y = strtoull((const char*)yC, &t, 10);
				xC[0] = rxx->Sector[i].Entry[j].O[2][0];
				xC[1] = rxx->Sector[i].Entry[j].O[2][1];
				xC[2] = rxx->Sector[i].Entry[j].O[2][2];
				yC[0] = rxx->Sector[i].Entry[j].O[3][0];
				yC[1] = rxx->Sector[i].Entry[j].O[3][1];
				yC[2] = rxx->Sector[i].Entry[j].O[3][2];
				d.end_X = strtoull((const char*)xC, &t, 10) + 1;
				d.end_Y = strtoull((const char*)yC, &t, 10) + 1;
				for (size_t k = 0; k < 6; k++) {
					C[0] = rxx->Sector[i].Entry[j].P[k].B;
					d.palette[k].B = strtoull((const char*)C, &t, 10);
					C[0] = rxx->Sector[i].Entry[j].P[k].R;
					d.palette[k].R = strtoull((const char*)C, &t, 10);
					C[0] = rxx->Sector[i].Entry[j].P[k].G;
					d.palette[k].G = strtoull((const char*)C, &t, 10);
				}
				R.push_back(d);
			}
		}
		//		std::wcout << this->len_x << L"," << this->len_y << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 10; i++) {
			c.red = d3tod8(this->palette[i].R);
			c.green = d3tod8(this->palette[i].G);
			c.blue = d3tod8(this->palette[i].B);
			pal.push_back(c);
		}

		switch (this->color_num) {
		case 43:
			this->color_num = 51;
			break;
		case 46:
			this->color_num = 52;
			break;
		case 57:
			this->color_num = 9;
			break;
		case 58:
			this->color_num = 13;
			break;
		case 72:
			this->color_num = 38;
			break;
		case 73:
			this->color_num = 233;
			break;
		case 74:
			this->color_num = 59;
			break;
		case 76:
			this->color_num = 75;
			break;
		case 84:
			this->color_num = 55;
			break;
		case 97:
			this->color_num = 229;
			break;
		case 99:
			this->color_num = 98;
			break;
		case 109:
		case 110:
			this->color_num = 14;
			break;
		case 120:
			this->color_num = 20;
			break;
		case 124:
			this->color_num = 231;
			break;
		case 148:
			this->color_num = 138;
			break;
		case 154:
			this->color_num = 17;
			break;
		case 157:
			this->color_num = 156;
			break;
		case 164:
			this->color_num = 138;
			break;
		case 166:
			this->color_num = 218;
			break;
		case 197:
			this->color_num = 156;
			break;
		case 256:
		case 257:
		case 258:
		case 259:
			this->color_num = 207;
			break;
		case 281:
			this->color_num = 223;
			break;
		default:
			break;
		}

		unsigned __int8 max_pal = *std::max_element(I.begin(), I.end());
		if (max_pal >= 10) {
			if (this->R[this->color_num - 1].flag == 0) {
				if (this->R[this->color_num - 1].palette[0].R || this->R[this->color_num - 1].palette[0].G || this->R[this->color_num - 1].palette[0].B) {
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->R[this->color_num - 1].palette[i].R);
						c.green = d3tod8(this->R[this->color_num - 1].palette[i].G);
						c.blue = d3tod8(this->R[this->color_num - 1].palette[i].B);
						pal.push_back(c);
					}
				}
				else if (this->color_num == 297) {
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->palette[10 + i].R);
						c.green = d3tod8(this->palette[10 + i].G);
						c.blue = d3tod8(this->palette[10 + i].B);
						pal.push_back(c);
					}
				}
				else {
					std::cout << this->color_num << "/" << this->R[this->color_num - 1].flag << " " << (int)max_pal << std::endl;
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->palette[0].R);
						c.green = d3tod8(this->palette[0].G);
						c.blue = d3tod8(this->palette[0].B);
						pal.push_back(c);
					}
				}
			}
			else {
				if (this->color_num == 298) {

					this->color_num = 20;
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->R[this->color_num - 1].palette[i].R);
						c.green = d3tod8(this->R[this->color_num - 1].palette[i].G);
						c.blue = d3tod8(this->R[this->color_num - 1].palette[i].B);
						pal.push_back(c);
					}
					this->color_num = 26;
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->R[this->color_num - 1].palette[i].R);
						c.green = d3tod8(this->R[this->color_num - 1].palette[i].G);
						c.blue = d3tod8(this->R[this->color_num - 1].palette[i].B);
						pal.push_back(c);
					}

					for (size_t s = 0; s < I.size(); s++) {
						if (s % 512 >= 256 && I.at(s) >= 10) {
							I.at(s) += 6;
						}
					}
				}
				else {
					//					std::cout << this->color_num << "/" << this->R[this->color_num - 1].flag << " " << (int)max_pal << std::endl;
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->R[this->color_num - 1].palette[i].R);
						c.green = d3tod8(this->R[this->color_num - 1].palette[i].G);
						c.blue = d3tod8(this->R[this->color_num - 1].palette[i].B);
						pal.push_back(c);
					}
				}
			}
		}

		for (size_t i = 0; i < 16; i++) {
			std::wcout << (int)(pal[i].red >> 5) << (int)(pal[i].green >> 5) << (int)(pal[i].blue >> 5) << " ";
		}
		std::cout << std::endl;
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		std::cout << std::setw(3) << this->color_num << ":" << std::setw(3) << this->R[this->color_num - 1].offset_X << "," << std::setw(3) << this->R[this->color_num - 1].offset_Y << " ";

		unsigned __int8* src = this->buf->body, prev = ~*src;
		bool repeat = false;

		while (!this->is_end(src)) {
			if (repeat) {
				repeat = false;
				int cp_len = *src;
				if (cp_len > 0) {
					uPackedPixel4 u = { prev };

					for (size_t k = 0; k < cp_len; k++) {
						I.push_back(u.S.H);
						I.push_back(u.S.L);
					}
				}
				src++;
			}
			else {
				if (*src == prev) {
					repeat = true;
				}
				else {
					repeat = false;
				}
				prev = *src;
				uPackedPixel4 u = { prev };

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
			}
		}

		std::wcerr << std::setw(6) << I.size() << L"/" << std::setw(6) << this->len_x * this->len_y << L" ";
		I.resize((size_t)this->len_x * this->len_y);

		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}
	}
};

class GL {
	std::vector<unsigned __int8> I;
	struct Pal_GL {
		unsigned __int16 B : 4;
		unsigned __int16 R : 4;
		unsigned __int16 G : 4;
		unsigned __int16 : 4;
	};

	struct format_GL {
		unsigned __int8 offs_hx; // divided by 2
		unsigned __int8 offs_y;
		unsigned __int8 len_hx; // divided by 2
		unsigned __int8 len_y;
		unsigned __int8 unk0;
		__int8 trans; // Transparent color
		unsigned __int8 Unknown[10];
		Pal_GL palette[16];
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

public:
	int transparent = -1;
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer, std::vector<__int8>* palette_buffer)
	{
		this->buf = (format_GL*)&buffer.at(0);
		this->len_buf = buffer.size();
		this->transparent = this->buf->trans;

		if (palette_buffer != nullptr) {
			*this->buf->palette = *(Pal_GL*)palette_buffer->at(0);
		}

		this->len_x = this->buf->len_hx ? (size_t)this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		if (this->len_y > 212 || this->buf->unk0 != 0xFF) {
			return true;
		}

		this->offset_x = this->buf->offs_hx * 2;
		this->offset_y = this->buf->offs_y;

		std::wcout << std::setw(3) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" ("
			<< std::setw(3) << this->len_x << L"*" << std::setw(3) << this->len_y << L") transparent " << std::setw(2) << this->transparent << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->buf->palette[i].R);
			c.green = d3tod8(this->buf->palette[i].G);
			c.blue = d3tod8(this->buf->palette[i].B);
			pal.push_back(c);

			trans.push_back(0xFF);
		}

		if (this->transparent == -1) {
			c.red = 0;
			c.green = 0;
			c.blue = 0;
			pal.push_back(c);
			trans.push_back(0);
		}
		else {
			trans.at(this->transparent) = 0;
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = len_x * len_y;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL);
		size_t cp_len;
		uPackedPixel4 u;

		while (count-- && I.size() < image_size) {
			switch (*src) {
			case 0x00:
				cp_len = *(src + 1) ? *(src + 1) : 256;
				u.B = *(src + 2);

				for (size_t k = 0; k < cp_len; k++) {
					I.push_back(u.S.H);
					I.push_back(u.S.L);
				}

				src += 3;
				count -= 2;
				break;

			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				cp_len = *src;
				u.B = *(src + 1);

				for (size_t k = 0; k < cp_len; k++) {
					I.push_back(u.S.H);
					I.push_back(u.S.L);
				}

				src += 2;
				count--;
				break;
			default:
				u.B = *src;

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
			}
		}

		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}
	}
};

enum class decode_mode {
	NONE = 0, GE7, LP, LV, GS, GL, I, TT, DRS
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
			else if (*(*argv + 1) == L'g') { // Gakuen Senki
				dm = decode_mode::GS;
			}
			else if (*(*argv + 1) == L'r') { // Rance and other GL
				dm = decode_mode::GL;
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
		LP lp;
		LV lv;
		GS gs;
		GL gl;

		switch (dm) {
		case decode_mode::GE7:
			if (ge7.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			ge7.decode_palette(out.palette);
			ge7.decode_body(out.body);
			break;

		case decode_mode::LP:
			if (lp.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			lp.decode_palette(out.palette);
			lp.decode_body(out.body);
			out.set_size(lp.len_x, lp.len_y);
			break;

		case decode_mode::LV:
			if (lv.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			lv.decode_palette(out.palette);
			lv.decode_body(out.body);
			out.set_size(lv.len_x, lv.len_y);
			break;

		case decode_mode::GS:
		{
			wchar_t* t;
			size_t n = wcstoull(*argv, &t, 10);
			if (n == 0 || n > 300) {
				std::wcerr << L"Out of range." << std::endl;

				continue;
			}

			std::ifstream palettefile("RXX", std::ios::binary);
			if (!palettefile) {
				std::wcerr << L"File " << "RXX" << L" open error." << std::endl;

				continue;
			}

			std::vector<__int8> palbuf{ std::istreambuf_iterator<__int8>(palettefile), std::istreambuf_iterator<__int8>() };

			palettefile.close();

			if (gs.init(inbuf, palbuf, n)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}

			gs.decode_body(out.body);
			gs.decode_palette(out.palette);
			out.set_size(gs.len_x, gs.len_y);
			break;
		}
		case decode_mode::GL:
			if (gl.init(inbuf, nullptr)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			gl.decode_palette(out.palette, out.trans);
			gl.decode_body(out.body);
			out.set_size(gl.len_x, gl.len_y);
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