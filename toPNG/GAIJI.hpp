#ifndef TOPNG_GAIJI
#define TOPNG_GAIJI

#pragma pack(push)
#pragma pack(1)
class GAIJI {
	std::vector<unsigned __int8> I;

	struct format_GAIJI {
		unsigned __int16 entries;
		unsigned __int16 length;
		unsigned __int16 unk[15];

		struct {
			unsigned __int16 CodeJIS;
			unsigned __int16 bodyBE[16];
		} Data[188];;
	} *buf = nullptr;

public:
	png_uint_32 disp_x = 16;
	png_uint_32 disp_y = 16;

	bool init(std::vector<__int8>& buffer)
	{
		this->buf = (struct format_GAIJI*)&buffer.at(0);
		if (this->buf->entries != 188) {
			wouterr(L"Size mismatch.");
			return true;
		}
		if (this->buf->length != 16) {
			wouterr(L"Length mismatch.");
			return true;
		}
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		c.red = 0xFF;
		c.green = 0xFF;
		c.blue = 0xFF;
		pal.push_back(c);
		trans.push_back(0xFF);

		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		trans.push_back(0xFF);
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		size_t blocks = 2;
		for (size_t i = 0; i < this->buf->entries; i += blocks) {
			for (size_t y = 0; y < 16; y++) {
				for (size_t b = 0; b < blocks; b++) {
					std::bitset<16> t{ _byteswap_ushort(this->buf->Data[i + b].bodyBE[y]) };

					for (size_t i = 0; i < 16; i++) {
						unsigned __int8 a = (t[15 - i] ? 1 : 0);
						this->I.push_back(a);
					}
				}
			}
		}

		this->disp_x = 16 * blocks;
		this->disp_y = 16 * this->buf->entries / blocks;

		for (size_t j = 0; j < this->disp_y; j++) {
			out_body.push_back((png_bytep)&this->I.at(j * this->disp_x));
		}
	}

};

#pragma pack(pop)
#endif
