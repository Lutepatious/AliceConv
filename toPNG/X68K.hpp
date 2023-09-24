#ifndef TOPNG_X68K
#define TOPNG_X68K
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class SPRITE {
	std::vector<unsigned __int8> I;

	struct format_SPRITE {
		unsigned __int16 Pal5BE[16]; // X68000 big endian、バイトスワップの上で色分解する事。
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
			P.Pin = _byteswap_ushort(buf->Pal5BE[i]);

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
#pragma pack(pop)
#endif
