#pragma pack(push)
#pragma pack(1)
class Intr {
	std::vector<unsigned __int8> I;
	struct format_Intr {
		unsigned __int8 len_hx; // divided by 2
		unsigned __int8 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;
	size_t len_buf = 0;
	size_t image_num = 0;

	MSX_Pal(*pal)[16];

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer, std::vector<__int8>& palette_buffer, size_t num)
	{
		if (palette_buffer.size() != 3200) {
			return true;
		}

		this->pal = (MSX_Pal(*)[16]) & palette_buffer.at(0);

		this->buf = (format_Intr*)&buffer.at(0);
		this->len_buf = buffer.size();
		this->image_num = num;

		this->len_x = this->buf->len_hx ? (size_t)this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		if (this->len_y > MSX_SCREEN7_V) {
			return true;
		}

		std::wcout << std::setw(2) << image_num << L":" << std::setw(3) << this->len_x << L"*" << std::setw(3) << this->len_y << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->pal[image_num][i].R);
			c.green = d3tod8(this->pal[image_num][i].G);
			c.blue = d3tod8(this->pal[image_num][i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->buf->body, prev = ~*src;
		size_t count = this->len_buf - sizeof(format_Intr);
		size_t cp_len;
		uPackedPixel4 u;

		while (count-- && I.size() < image_size) {
			if (*src == prev) {
				u.B = *src;
				cp_len = *(src + 1) + 1LL;

				for (size_t k = 0; k < cp_len; k++) {
					I.push_back(u.S.H);
					I.push_back(u.S.L);
				}

				src += 2;
				count--;
			}
			else {
				prev = u.B = *src;

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
			}
		}

		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}
	}
};
#pragma pack(pop)
