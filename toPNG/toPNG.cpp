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

class SPRITE {
	std::vector<unsigned __int8> I;

	struct format_SPRITE {
		unsigned __int16 Pal16BE[16]; // X68000 big endian、バイトスワップの上で色分解する事。
		struct {
			unsigned __int8 H : 4;
			unsigned __int8 L : 4;
		} S[128][2][16][4];
	}  *buf = nullptr;

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_SPRITE)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_SPRITE*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		union X68Pal_conv {
			struct Palette_depth5 Pal5;
			unsigned __int16 Pin;
		} P;

		png_color c;
		for (size_t i = 0; i < 16; i++) {
			P.Pin = _byteswap_ushort(buf->Pal16BE[i]);

			c.red = d5tod8(P.Pal5.R);
			c.green = d5tod8(P.Pal5.G);
			c.blue = d5tod8(P.Pal5.B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}

		trans.at(0) = 0;
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t spr_x = 16;
		const size_t spr_y = 16;
		const size_t patterns = 128;
		const size_t blocks = 16;


		for (size_t p = 0; p < patterns; p += blocks) {
			for (size_t Rows = 0; Rows < spr_y; Rows++) {
				for (size_t b = 0; b < blocks; b++) {
					for (size_t h = 0; h < 2; h++) {
						for (size_t c = 0; c < 4; c++) {
							I.push_back(this->buf->S[p + b][h][Rows][c].L);
							I.push_back(this->buf->S[p + b][h][Rows][c].H);
						}
					}
				}
			}
		}


		for (size_t j = 0; j < spr_y * patterns / blocks; j++) {
			out_body.push_back((png_bytep)&I.at(j * spr_x * blocks));
		}
	}

};

class MASK {
	std::vector<unsigned __int8> I;

	struct format_MASK {
		unsigned __int8 M[70][24][5];
	} *buf = NULL;

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_MASK)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_MASK*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		c.red = 0xFF;
		c.green = 0xFF;
		c.blue = 0xFF;
		pal.push_back(c);

		trans.push_back(0);
		trans.push_back(0xFF);
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t spr_x = 5;
		const size_t spr_y = 24;
		const size_t patterns = 70;
		const size_t blocks = 14;

		for (size_t p = 0; p < patterns; p += blocks) {
			for (size_t y = 0; y < spr_y; y++) {
				for (size_t b = 0; b < blocks; b++) {
					for (size_t x = 0; x < spr_x; x++) {
						for (size_t i = 0; i < 8; i++) {
							std::bitset<8> t = this->buf->M[p + b][y][x];
							unsigned a = t[7 - i] ? 1 : 0;
							I.push_back(a);
						}
					}
				}
			}
		}

		for (size_t j = 0; j < spr_y * patterns / blocks; j++) {
			out_body.push_back((png_bytep)&I.at(j * spr_x * blocks * 8));
		}
	}

};

class GL {
	std::vector<unsigned __int8> I;

	struct format_GL {
		unsigned __int16 Start; // (big endian表現) PC-8801ベースでのLoad address
		unsigned __int8 len_x8; // divided by 8
		unsigned __int8 len_y;
		unsigned __int32 unk;
		struct Palette_depth3 Pal3[8];
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC8801_H;
	png_uint_32 len_y = PC8801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	unsigned transparent = 8;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_GL)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_GL*)&buffer.at(0);
		this->len_buf = buffer.size();

		unsigned start = _byteswap_ushort(this->buf->Start);
		if (start < 0xC000) {
			std::wcerr << "Wrong start address." << std::endl;
			return true;
		}

		start -= 0xC000;

		this->len_col = this->buf->len_x8;
		this->len_x = 8 * this->buf->len_x8;
		this->len_y = this->buf->len_y;
		this->offset_x = start % 80 * 8;
		this->offset_y = start / 80;

		if (((size_t)this->len_x + this->offset_x > PC8801_H) || ((size_t)this->len_y + this->offset_y) > PC8801_V) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		std::wcout << L"From " << std::setw(4) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" Size " << std::setw(4) << len_x << L"," << std::setw(3) << len_y << std::endl;
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		for (size_t i = 0; i < 8; i++) {
			c.red = d3tod8(this->buf->Pal3[i].R);
			c.green = d3tod8(this->buf->Pal3[i].G);
			c.blue = d3tod8(this->buf->Pal3[i].B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}
		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		trans.push_back(0);
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const unsigned planes = 3;
		const size_t decode_size = (size_t)this->len_col * this->len_y * planes;
		const size_t image_size = (size_t)this->len_x * this->len_y;
		const size_t Row1_step = this->len_col * planes;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL);

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < decode_size) {
			switch (*src) {
			case 0x00:
				if (*(src + 1) & 0x80) { // (4行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*(src + 1) & 0x7F)回コピー
					D.insert(D.end(), *(src + 1) & 0x7F, *(D.end() - Row1_step * 4 + 1));
					src += 2;
					count--;
				}
				else { // *(src + 2)の8ドットデータを末尾に*(src + 1)回コピー
					D.insert(D.end(), *(src + 1), *(src + 2));
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
				// *(src + 1)の8ドットデータを末尾に*src回コピー
				D.insert(D.end(), *src, *(src + 1));
				src += 2;
				count--;
				break;

			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0D:
			case 0x0E:
				// (4行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*src - 6)回コピー
				D.insert(D.end(), *src - 6, *(D.end() - Row1_step * 4 + 1));
				src++;
				break;

			case 0x0F:
			{
				// 別プレーンのデータを(*(src + 1) & 0x7F)*8ドット分末尾にコピー
				size_t target_plane = ((D.size() / len_col) % planes) - ((*(src + 1) & 0x80) ? 1 : 0);
				auto cp_src = D.end() - len_col * target_plane;
				D.insert(D.end(), cp_src, cp_src + (*(src + 1) & 0x7F));
				src += 2;
				count--;
				break;
			}

			default:
				D.push_back(*src);
				src++;
			}
		}

		// 3プレーン表現をインデックスカラーに変換 std::bitsetでより簡潔になった。
		std::vector<unsigned __int8> P;
		for (size_t l = 0; l < D.size(); l += Row1_step) {
			for (size_t c = 0; c < this->len_col; c++) {
				std::bitset<8> b[3] = { D.at(l + c), D.at(l + c + this->len_col), D.at(l + c + this->len_col * 2) };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0);
					P.push_back(a);
				}
			}
		}

		this->I.insert(this->I.end(), PC8801_H * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), P.begin() + this->len_x * y, P.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), PC8801_H - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), PC8801_H * (PC8801_V - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < PC8801_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC8801_H));
		}
	}
};

class GL3 {
	std::vector<unsigned __int8> I;

	struct format_GL3 {
		struct Palette_depth4 Pal4[16];	// もしここが空っぽならFM TOWNS版ALICEの館CDに収録されたIntruderだと思われる
		unsigned __int16 Start;
		unsigned __int16 len_x8; // divided by 8
		unsigned __int16 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;

	struct Palette_depth4 Pal4_GM3[16] =
	{ { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
	  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
	  { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
	  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF } };

	size_t len_buf = 0;
	bool is_GM3 = false;

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
		if (buffer.size() < sizeof(format_GL3)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		unsigned __int8 t = 0;
		for (int i = 0; i < 0x30; i++) {
			t |= buffer.at(i);
		}

		if (t >= 0x10) {
			std::wcerr << "Wrong palette." << std::endl;
			return false;
		}

		if (t == 0) {
			this->is_GM3 = true;
		}

		this->buf = (format_GL3*)&buffer.at(0);
		this->len_buf = buffer.size();

		unsigned start = this->buf->Start;
		if (start < 0x8000) {
			std::wcerr << "Wrong start address." << std::endl;
			return true;
		}

		start -= 0x8000;

		this->len_col = this->buf->len_x8;
		this->len_x = 8 * this->buf->len_x8;
		this->len_y = this->buf->len_y;
		this->offset_x = start % 80 * 8;
		this->offset_y = start / 80;

		if (((size_t)this->len_x + this->offset_x > PC9801_H) || ((size_t)this->len_y + this->offset_y) > PC9801_V) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		std::wcout << L"From " << std::setw(4) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" Size " << std::setw(4) << len_x << L"," << std::setw(3) << len_y << std::endl;
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;

		for (size_t i = 0; i < 16; i++) {
			struct Palette_depth4 *p;
			if (this->is_GM3) {
				p = &this->Pal4_GM3[i];
			}
			else {
				p = &this->buf->Pal4[i];
			}
			c.red = d4tod8(p->R);
			c.green = d4tod8(p->G);
			c.blue = d4tod8(p->B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}
		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		trans.push_back(0);
	}

	bool decode_body(std::vector<png_bytep>& out_body)
	{
		const unsigned planes = 4;
		const size_t decode_size = (size_t)this->len_col * this->len_y * planes;
		const size_t image_size = (size_t)this->len_x * this->len_y;
		const size_t Row1_step = this->len_col * planes;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL3);

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < decode_size) {
			switch (*src) {
			case 0x00:
				if (*(src + 1) & 0x80) { // (2行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*(src + 1) & 0x7F)回コピー
					D.insert(D.end(), *(src + 1) & 0x7F, *(D.end() - Row1_step * 2 + 1));
					src += 2;
					count--;
				}
				else { // *(src + 2)の8ドットデータを末尾に*(src + 1)回コピー
					D.insert(D.end(), *(src + 1), *(src + 2));
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
				// *(src + 1)の8ドットデータを末尾に*src回コピー
				D.insert(D.end(), *src, *(src + 1));
				src += 2;
				count--;
				break;

			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0D:
				// (2行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*src - 6)回コピー
				D.insert(D.end(), *src - 6, *(D.end() - Row1_step * 2 + 1));
				src++;
				break;

			case 0x0E:
			{
				// 別プレーンのデータを(*(src + 1) & 0x7F)*8ドット分末尾にコピー
				size_t target_plane = ((D.size() / len_col) % planes) - 2;
				auto cp_src = D.end() - len_col * target_plane;
				size_t cp_len = *(src + 1) & 0x7F;

				D.insert(D.end(), cp_src, cp_src + cp_len);
				src += 2;
				count--;
				break;
			}

			case 0x0F:
			{
				// 別プレーンのデータを(*(src + 1) & 0x7F)*8ドット分末尾にコピー
				size_t target_plane = ((D.size() / len_col) % planes) - ((*(src + 1) & 0x80) ? 1 : 0);
				auto cp_src = D.end() - len_col * target_plane;
				size_t cp_len = *(src + 1) & 0x7F;

				D.insert(D.end(), cp_src, cp_src + cp_len);
				src += 2;
				count--;
				break;
			}

			default:
				D.push_back(*src);
				src++;
			}
		}

		// 4プレーン表現をインデックスカラーに変換 std::bitsetでより簡潔になった。
		std::vector<unsigned __int8> P;
		for (size_t l = 0; l < D.size(); l += Row1_step) {
			for (size_t c = 0; c < this->len_col; c++) {
				std::bitset<8> b[4] = { D.at(l + c), D.at(l + c + this->len_col), D.at(l + c + this->len_col * 2), D.at(l + c + this->len_col * 3) };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0) | (b[3][7 - i] ? 8 : 0);
					P.push_back(a);
				}
			}
		}

		if (this->is_GM3) {
			this->disp_y = 201;
		}

		this->I.insert(this->I.end(), PC9801_H * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), P.begin() + this->len_x * y, P.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), PC9801_H - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), PC9801_H * ((size_t) this->disp_y - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < this->disp_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_H));
		}

		return this->is_GM3;
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
			else if (*(*argv + 1) == L'v') { // GL3, GM3
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
