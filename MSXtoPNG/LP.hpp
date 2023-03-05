#pragma pack(push)
#pragma pack(1)
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
#pragma pack(pop)
