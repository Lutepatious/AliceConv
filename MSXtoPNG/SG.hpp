#pragma pack(push)
#pragma pack(1)
class Dual_GL {
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
	} *bufo = nullptr, *bufe = nullptr;

	size_t len_bufo = 0;
	size_t len_bufe = 0;

public:
	int transparent = -1;
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffero ,std::vector<__int8>& buffere)
	{
		this->bufo = (format_GL*)&buffero.at(0);
		this->bufe = (format_GL*)&buffere.at(0);
		this->len_bufo = buffero.size();
		this->len_bufe = buffere.size();

		if (this->bufo->trans != this->bufe->trans ||
			this->bufo->len_hx != this->bufe->len_hx ||
			this->bufo->offs_hx != this->bufe->offs_hx ||
			this->bufo->len_y != this->bufe->len_y ||
			this->bufo->offs_y != this->bufe->offs_y ||
			this->bufo->unk0 != 0xFF ||
			this->bufe->unk0 != 0xFF) {
			return true;
		}

		for (size_t i = 0; i < 16; i++) {
			if (this->bufo->palette[i].B != this->bufe->palette[i].B ||
				this->bufo->palette[i].R != this->bufe->palette[i].R ||
				this->bufo->palette[i].G != this->bufe->palette[i].G)
				return true;
		}

		this->transparent = this->bufo->trans;

		this->len_x = this->bufo->len_hx ? (size_t)this->bufo->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->bufo->len_y;

		this->offset_x = this->bufo->offs_hx * 2;
		this->offset_y = this->bufo->offs_y;

		if (this->len_y > MSX_SCREEN7_V) {
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
			c.red = d3tod8(this->bufo->palette[i].R);
			c.green = d3tod8(this->bufo->palette[i].G);
			c.blue = d3tod8(this->bufo->palette[i].B);
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
		unsigned __int8* src = this->bufo->body;
		size_t count = this->len_bufo - sizeof(format_GL);
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

		src = this->bufe->body;
		count = this->len_bufe - sizeof(format_GL);

		while (count-- && I.size() < image_size * 2) {
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

		FI.assign(MSX_SCREEN7_H * MSX_SCREEN7_V * 2ULL, this->transparent);

		for (size_t j = 0; j < this->len_y; j++) {
			memcpy_s(&FI[(this->offset_y + j) * 2 * MSX_SCREEN7_H + this->offset_x], this->len_x, &I[j * this->len_x], this->len_x);
			memcpy_s(&FI[((this->offset_y + j) * 2 + 1) * MSX_SCREEN7_H + this->offset_x], this->len_x, &I[(j + this->len_y) * this->len_x], this->len_x);
		}

		for (size_t j = 0; j < MSX_SCREEN7_V * 2; j++) {
			out_body.push_back((png_bytep)&FI.at(j * MSX_SCREEN7_H));
		}
		this->len_y *= 2;
	}
};
#pragma pack(pop)
