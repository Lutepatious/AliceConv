#pragma pack(push)
#pragma pack(1)
class GL {
	std::vector<unsigned __int8> I;
	std::vector<unsigned __int8> FI;

	struct format_GL {
		unsigned __int8 offs_hx; // divided by 2
		unsigned __int8 offs_y;
		unsigned __int8 len_hx; // divided by 2
		unsigned __int8 len_y;
		unsigned __int8 unk0;
		__int8 trans; // Transparent color
		unsigned __int8 Unknown[10];
		MSX_Pal palette[16];
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

public:
	int transparent = -1;
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer)
	{
		this->buf = (format_GL*)&buffer.at(0);
		this->len_buf = buffer.size();
		this->transparent = this->buf->trans;

		this->len_x = this->buf->len_hx ? (size_t)this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		this->offset_x = this->buf->offs_hx * 2;
		this->offset_y = this->buf->offs_y;

		if (this->len_y > MSX_SCREEN7_V || this->buf->unk0 != 0xFF) {
			return true;
		}

		std::wcout << std::setw(3) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" ("
			<< std::setw(3) << this->len_x << L"*" << std::setw(3) << this->len_y << L") transparent " << std::setw(2) << this->transparent << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->buf->palette[i].R);
			c.green = d3tod8(this->buf->palette[i].G);
			c.blue = d3tod8(this->buf->palette[i].B);
			pal.push_back(c);

			trans.push_back(0xFF);
		}

		if (this->transparent == -1) {
			c.red = 0;
			c.green = 0;
			c.blue = 0;
			pal.push_back(c);
			this->transparent = trans.size();
			trans.push_back(0);
		}
		else {
			trans.at(this->transparent) = 0;
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL);
		size_t cp_len;
		uPackedPixel4 u;

		while (count-- && I.size() < image_size) {
			switch (*src) {
			case 0x00:
				cp_len = *(src + 1) ? *(src + 1) : 256;
				u.B = *(src + 2);

				for (size_t k = 0; k < cp_len; k++) {
					I.push_back(u.S.H);
					I.push_back(u.S.L);
				}

				src += 3;
				count -= 2;
				break;

			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				cp_len = *src;
				u.B = *(src + 1);

				for (size_t k = 0; k < cp_len; k++) {
					I.push_back(u.S.H);
					I.push_back(u.S.L);
				}

				src += 2;
				count--;
				break;
			default:
				u.B = *src;

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
			}
		}

#if 0
		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}

#else
		FI.assign(MSX_SCREEN7_H * MSX_SCREEN7_V, this->transparent);
		for (size_t j = 0; j < this->len_y; j++) {
			// Almost all viewer not support png offset feature
			// memcpy_s(&FI[(j) * MSX_SCREEN7_H], this->len_x, &I[j * this->len_x], this->len_x);
			memcpy_s(&FI[(this->offset_y + j) * MSX_SCREEN7_H + this->offset_x], this->len_x, &I[j * this->len_x], this->len_x);
		}

		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			out_body.push_back((png_bytep)&FI.at(j * MSX_SCREEN7_H));
		}
#endif
	}
};
#pragma pack(pop)
