#ifndef TOPNG_ICN
#define TOPNG_ICN

#pragma pack(push)
#pragma pack(1)
class ICN {
	std::vector<unsigned __int8> I;

	struct format_ICN {
		unsigned __int32 bodyBE[][32];
	} *buf = nullptr;
	size_t ICN_entries = 0;

	struct format_ICNFILE {

	};

public:
	png_uint_32 disp_x = 32;
	png_uint_32 disp_y = 32;

	bool init(std::vector<__int8>& buffer)
	{
		size_t len_buf = buffer.size();
		if (strlen(&buffer.at(0)) == 0) {
			if (len_buf % (sizeof(unsigned __int32) * 32)) {
				wouterr(L"Size mismatch.");
				return true;
			}
			this->buf = (struct format_ICN*)&buffer.at(0);
			this->ICN_entries = len_buf / (sizeof(unsigned __int32) * 32);
			return false;
		}
		return true;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;

		c.red = 0xFF;
		c.green = 0xFF;
		c.blue = 0xFF;
		pal.push_back(c);
		trans.push_back(0);

		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		trans.push_back(0xFF);
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		size_t blocks = 16;
		for (size_t i = 0; i < this->ICN_entries; i += blocks) {
			for (size_t y = 0; y < 32; y++) {
				for (size_t b = 0; b < blocks; b++) {
					std::bitset<32> t{ _byteswap_ulong(this->buf->bodyBE[i + b][y]) };

					for (size_t i = 0; i < 32; i++) {
						unsigned __int8 a = (t[31 - i] ? 1 : 0);
						this->I.push_back(a);
					}
				}
			}
		}

		this->disp_x = 32 * blocks;
		this->disp_y = 32 * this->ICN_entries / blocks;

		for (size_t j = 0; j < this->disp_y; j++) {
			out_body.push_back((png_bytep)&this->I.at(j * this->disp_x));
		}
	}

};
#pragma pack(pop)
#endif
