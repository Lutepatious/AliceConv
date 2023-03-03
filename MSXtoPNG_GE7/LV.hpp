#pragma pack(push)
#pragma pack(1)
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
#pragma pack(pop)
