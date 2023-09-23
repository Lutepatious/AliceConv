#ifndef TOPNG_GL_X68K
#define TOPNG_GL_X68K
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class GL_X68K {
	std::vector<unsigned __int8> I;

	struct format_GL_X68K {
		unsigned __int8 Sig;
		unsigned __int8 unk0;
		struct Palette_depth4RGB Pal4[16];
		unsigned __int16 start_hx; // divided by 2
		unsigned __int16 start_y;
		unsigned __int16 len_hx; // divided by 2
		unsigned __int16 len_y;
		unsigned __int16 Sig2;
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

		if (this->buf->Sig != 0x11 || this->buf->Sig2 != 0xFEFF) {
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

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;

		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->buf->Pal4[i].R);
			c.green = d4tod8(this->buf->Pal4[i].G);
			c.blue = d4tod8(this->buf->Pal4[i].B);
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
		const size_t decode_size = (size_t)this->len_col * this->len_y;
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL_X68K);


		union {
			unsigned __int8 B;
			struct {
				unsigned __int8	L : 4;
				unsigned __int8 H : 4;
			} S;
		} u;

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < image_size) {
			switch (*src) {
			case 0xFE:
				for (size_t len = 0; len < *(src + 1); len++) {
					u.B = *(src + 2);
					D.push_back(u.S.L);
					D.push_back(u.S.H);
					u.B = *(src + 3);
					D.push_back(u.S.L);
					D.push_back(u.S.H);
				}
				src += 4;
				count -= 3;
				break;

			case 0xFF:
				for (size_t len = 0; len < *(src + 1); len++) {
					u.B = *(src + 2);
					D.push_back(u.S.L);
					D.push_back(u.S.H);
				}
				src += 3;
				count -= 2;
				break;

			default:
				u.B = *src;
				D.push_back(u.S.L);
				D.push_back(u.S.H);
				src++;
			}
		}

		this->I.insert(this->I.end(), PC9801_H * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), D.begin() + this->len_x * y, D.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), PC9801_H - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), PC9801_H * ((size_t)PC9801_V - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < PC9801_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_H));
		}
	}
};
#pragma pack(pop)
#endif
