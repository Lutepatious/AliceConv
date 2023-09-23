#ifndef TOPNG_DRS003
#define TOPNG_DRS003
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class DRS003 {
	std::vector<unsigned __int8> I;

	struct format_DRS003 {
		struct Palette_depth4 Pal4[16];
		unsigned __int8 body[PC9801_H][4][PC9801_V / 8];
	} *buf = nullptr;

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_DRS003)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_DRS003*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->buf->Pal4[i].R);
			c.green = d4tod8(this->buf->Pal4[i].G);
			c.blue = d4tod8(this->buf->Pal4[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		for (size_t y = 0; y < PC9801_H; y++) {
			for (size_t x = 0; x < PC9801_V / 8; x++) {
				std::bitset<8> b[4] = { this->buf->body[y][0][x], this->buf->body[y][1][x], this->buf->body[y][2][x], this->buf->body[y][3][x] };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0) | (b[3][7 - i] ? 8 : 0);
					I.push_back(a);
				}
			}
		}

		for (size_t j = 0; j < PC9801_H; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_V));
		}
	}
};

class DRS003T {
	std::vector<unsigned __int8> I;

	struct format_DRS003T {
		unsigned __int8 body[PC9801_H][PC9801_V / 2];
	} *buf = nullptr;

	const struct Palette_depth4 Pal4[16] = { {0x0, 0x0, 0x0}, {0x0, 0x0, 0x0}, {0xF, 0xF, 0xF}, {0xA, 0xA, 0xA},
											{0x6, 0x6, 0x6}, {0x1, 0xD, 0x0}, {0x4, 0x0, 0x7}, {0x2, 0x0, 0x3},
											{0xD, 0xD, 0xD}, {0xC, 0xF, 0xD}, {0x8, 0xF, 0xA}, {0x5, 0xD, 0x6},
											{0x1, 0x6, 0x2}, {0xD, 0xF, 0xB}, {0xA, 0xF, 0x7}, {0xE, 0xE, 0xE} };

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_DRS003T)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_DRS003T*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->Pal4[i].R);
			c.green = d4tod8(this->Pal4[i].G);
			c.blue = d4tod8(this->Pal4[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		for (size_t y = 0; y < PC9801_H; y++) {
			for (size_t x = 0; x < PC9801_V / 2; x++) {
				union {
					unsigned __int8 A;
					struct {
						unsigned __int8 L : 4;
						unsigned __int8 H : 4;
					} B;
				} u;
				u.A = this->buf->body[y][x];

				I.push_back(u.B.L);
				I.push_back(u.B.H);
			}
		}

		for (size_t j = 0; j < PC9801_H; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_V));
		}
	}
};

class DRSOPNT {
	std::vector<unsigned __int8> I;

	struct format_DRSOPNT {
		unsigned __int8 body[VGA_V][VGA_H / 2];
	} *buf = nullptr;

	const struct Palette_depth4 Pal4[16] = { {0x0, 0x0, 0x0}, {0x0, 0x0, 0x0}, {0xF, 0xF, 0xF}, {0xA, 0xA, 0xA},
											{0x6, 0x6, 0x6}, {0x1, 0xD, 0x0}, {0x4, 0x0, 0x7}, {0x2, 0x0, 0x3},
											{0xD, 0xD, 0xD}, {0xC, 0xF, 0xD}, {0x8, 0xF, 0xA}, {0x5, 0xD, 0x6},
											{0x1, 0x6, 0x2}, {0xD, 0xF, 0xB}, {0xA, 0xF, 0x7}, {0xE, 0xE, 0xE} };

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_DRSOPNT)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_DRSOPNT*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->Pal4[i].R);
			c.green = d4tod8(this->Pal4[i].G);
			c.blue = d4tod8(this->Pal4[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		for (size_t y = 0; y < VGA_V; y++) {
			for (size_t x = 0; x < VGA_H / 2; x++) {
				union {
					unsigned __int8 A;
					struct {
						unsigned __int8 L : 4;
						unsigned __int8 H : 4;
					} B;
				} u;
				u.A = this->buf->body[y][x];

				I.push_back(u.B.L);
				I.push_back(u.B.H);
			}
		}

		for (size_t j = 0; j < VGA_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * VGA_H));
		}
	}
};
#pragma pack(pop)
#endif
