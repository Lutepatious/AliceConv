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

struct PackedPixel4 {
	unsigned __int8	L : 4;
	unsigned __int8 H : 4;
};

union uPackedPixel4 {
	unsigned __int8 B;
	PackedPixel4 S;
};

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
};

class GE7 {
	std::vector<unsigned __int8> I;
	format_GE7* buf = nullptr;

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

struct format_LP {
	unsigned __int8 body[][LEN_SECTOR];
};

class LP {
	std::vector<unsigned __int8> I;
	format_LP* buf = nullptr;
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

// MSX版Little Vampireのフォーマット
struct format_LV {
	unsigned __int8 Id;
	unsigned __int8 NegCols; // ~(Cols - 1)
	unsigned __int8 len_y;
	unsigned __int8 body[];
};

class LV {
	std::vector<unsigned __int8> I;
	format_LV* buf = nullptr;

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

struct format_GS {
	unsigned __int8 len_hx; // divided by 2
	unsigned __int8 len_y;
	unsigned __int8 body[];
};

class GS {
	std::vector<unsigned __int8> I;
	struct palette_GS {
		struct sec_palette_GS {
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
	} *pal2 = nullptr;
	format_GS* buf = nullptr;
	size_t len_buf = 0;

	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} palette[16] =
	{ { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
	  { 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 },
	  { 0x4, 0x7, 0x6 }, { 0x6, 0x7, 0x6 }, { 0x4, 0x4, 0x4 }, { 0x6, 0x6, 0x6 },
	  { 0x1, 0x1, 0x4 }, { 0x5, 0x6, 0x2 }, { 0x5, 0x5, 0x5 }, { 0x7, 0x7, 0x7 } };

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;

	bool init(std::vector<__int8>& buffer, std::vector<__int8>& palette_buffer)
	{
		this->buf = (format_GS*)&buffer.at(0);
		this->len_buf = buffer.size();
		this->pal2 = (palette_GS*)&palette_buffer.at(0);

		this->len_x = this->buf->len_hx ? this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		//		std::wcout << this->len_x << L"," << this->len_y << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, size_t n)
	{
		png_color c;
		for (size_t i = 0; i < 10; i++) {
			c.red = d3tod8(this->palette[i].R);
			c.green = d3tod8(this->palette[i].G);
			c.blue = d3tod8(this->palette[i].B);
			pal.push_back(c);
		}

		switch (n) {
		case 43: // verified
			n = 51;
			break;
		case 46: // verified
			n = 52;
			break;
		case 57: // verified
			n = 9;
			break;
		case 58: // verified
			n = 13;
			break;
		case 72: // verified
			n = 38;
			break;
		case 73: // verified
			n = 233;
			break;
		case 74: // verified
			n = 59;
			break;
		case 76: // verified
			n = 75;
			break;
		case 84: // verified
			n = 55;
			break;
		case 97:
			n = 96;
			break;
		case 99:
			n = 98;
			break;
		case 109: // verified
		case 110: // verified
			n = 14;
			break;
		case 120: // verified
			n = 20;
			break;
		default:
			break;
		}

		n--;
		size_t sector = n / 6;
		size_t entry = n % 6;

		unsigned __int8 max_pal = *std::max_element(I.begin(), I.end());
		if (max_pal >= 10) {
			if (this->pal2->Sector[sector].Entry[entry].F == 0x30) {
				if (this->pal2->Sector[sector].Entry[entry].P[0].R != 0x30 || this->pal2->Sector[sector].Entry[entry].P[0].G != 0x30 || this->pal2->Sector[sector].Entry[entry].P[0].B != 0x30) {
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->pal2->Sector[sector].Entry[entry].P[i].R - 0x30);
						c.green = d3tod8(this->pal2->Sector[sector].Entry[entry].P[i].G - 0x30);
						c.blue = d3tod8(this->pal2->Sector[sector].Entry[entry].P[i].B - 0x30);
						pal.push_back(c);
					}
				}
				else {
					std::cout << n + 1 << "/" << this->pal2->Sector[sector].Entry[entry].F << " " << (int)max_pal << std::endl;
					for (size_t i = 0; i < 6; i++) {
						c.red = d3tod8(this->palette[0].R);
						c.green = d3tod8(this->palette[0].G);
						c.blue = d3tod8(this->palette[0].B);
						pal.push_back(c);
					}
				}
			}
			else {
				std::cout << n + 1 << "/" << this->pal2->Sector[sector].Entry[entry].F << " " << (int)max_pal << std::endl;
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->pal2->Sector[sector].Entry[entry].P[i].R - 0x30);
					c.green = d3tod8(this->pal2->Sector[sector].Entry[entry].P[i].G - 0x30);
					c.blue = d3tod8(this->pal2->Sector[sector].Entry[entry].P[i].B - 0x30);
					pal.push_back(c);
				}
			}
		}
	}

	void view_palette(void)
	{
		for (size_t i = 0; i < 50; i++) {
			for (size_t j = 0; j < 6; j++) {
				std::cout << std::setw(3) << i * 6 + j + 1 << "/";
				std::cout << this->pal2->Sector[i].Entry[j].F << " ";
				for (size_t c = 0; c < 3; c++) {
					std::cout << this->pal2->Sector[i].Entry[j].X[c] << " ";
				}
				for (size_t c = 0; c < 3; c++) {
					std::cout << this->pal2->Sector[i].Entry[j].Y[c] << " ";
				}
				std::cout << "/";
				for (size_t c = 0; c < 6; c++) {
					std::cout << this->pal2->Sector[i].Entry[j].P[c].B << this->pal2->Sector[i].Entry[j].P[c].R << this->pal2->Sector[i].Entry[j].P[c].G << " ";
				}
				for (size_t c = 0; c < 4; c++) {
					for (size_t d = 0; d < 3; d++) {
						std::cout << this->pal2->Sector[i].Entry[j].O[c][d];
					}
					std::cout << " ";
				}
				std::cout << std::endl;
			}
		}

	}

	bool is_empty(unsigned __int8* pos) {
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

	void decode_body(std::vector<png_bytep>& out_body)
	{
		unsigned __int8* src = this->buf->body, prev = ~*src;
		bool repeat = false;

		while (!this->is_empty(src)) {
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

//		std::wcerr << I.size() << L"/" << this->len_x * this->len_y << std::endl;
		I.resize(this->len_x * this->len_y);

		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
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
			else if (*(*argv + 1) == L'g') { // Little Vampire
				dm = decode_mode::GS;
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

			if (gs.init(inbuf, palbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}

			gs.decode_body(out.body);
			gs.decode_palette(out.palette, n);
			out.set_size(gs.len_x, gs.len_y);
			break;
		}
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