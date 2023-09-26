#ifndef TOPNG_X68K_TT
#define TOPNG_X68K_TT
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class X68K_TT { // 闘神都市X68000版のGRAPHICSモードで使われる256色用フォーマット。
	std::vector<unsigned __int8> I;

	struct format_X68K_TT {
		unsigned __int32 Sig0BE; // 0x00000011 固定
		unsigned __int32 Sig1BE; // 0x00000000 固定
		unsigned __int32 Sig2BE; // 0x00010007 固定
		unsigned __int16 Pal5BE[0xC0]; // Palette No. 0x40 to 0xFF 0x00から0x3FはテキストモードやPCGのパレットと共用だが利用例なし。
		unsigned __int16 Overlay;
		unsigned __int16 StartBE;
		unsigned __int16 len_x_BE;
		unsigned __int16 len_y_BE;
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;
	unsigned transparent = 0;

public:
	png_uint_32 len_x = X68000_G;
	png_uint_32 len_y = X68000_G;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_X68K_TT)) {
			wouterr(L"File too short.");
			return true;
		}

		this->buf = (format_X68K_TT*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (_byteswap_ulong(this->buf->Sig0BE) != 0x11 || _byteswap_ulong(this->buf->Sig1BE) != 0 || _byteswap_ulong(this->buf->Sig2BE) != 0x00010007) {
			wouterr(L"Wrong signature.");
			return true;
		}

		unsigned __int16 start = _byteswap_ushort(this->buf->StartBE) / 2;
		this->offset_y = start / X68000_G;
		this->offset_x = start % X68000_G;
		this->len_x = _byteswap_ushort(this->buf->len_x_BE);
		this->len_y = _byteswap_ushort(this->buf->len_y_BE);

		if (this->len_x > X68000_G || this->len_y > X68000_G) {
			wouterr(L"Wrong size.");
			return true;
		}

		out_image_info(this->offset_x, this->offset_y, this->len_x, this->len_y, L"X68K_TT");
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		union X68Pal_conv {
			struct Palette_depth5 Pal5;
			unsigned __int16 Pin;
		} P;

		png_color c;

		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.insert(pal.end(), 0x40, c);
		trans.insert(trans.end(), 0x40, 0xFF);
		for (size_t i = 0; i < 0xC0; i++) {
			P.Pin = _byteswap_ushort(buf->Pal5BE[i]);

			c.red = d5tod8(P.Pal5.R);
			c.green = d5tod8(P.Pal5.G);
			c.blue = d5tod8(P.Pal5.B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}

		trans.at(this->transparent) = 0;
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_X68K_TT);

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < image_size) {
			switch (*src) {
			case 0x00:
				for (size_t len = 0; len < *(src + 2); len++) {
					D.push_back(*(src + 1));
				}
				src += 3;
				count -= 2;
				break;
			case 0x01:
				for (size_t len = 0; len < *(src + 3); len++) {
					D.push_back(*(src + 1));
					D.push_back(*(src + 2));
				}
				src += 4;
				count -= 3;
				break;
			default:
				D.push_back(*src);
				src++;
				break;
			}
		}

		this->I.insert(this->I.end(), X68000_G * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), D.begin() + this->len_x * y, D.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), X68000_G - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), X68000_G * ((size_t)X68000_G - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < X68000_G; j++) {
			out_body.push_back((png_bytep)&I.at(j * X68000_G));
		}
	}
};
#pragma pack(pop)
#endif
