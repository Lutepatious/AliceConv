#pragma pack(push)
#pragma pack(1)
class GS {
	std::vector<unsigned __int8> I;
	std::vector<unsigned __int8> FI;
	struct format_GS {
		unsigned __int8 len_hx; // divided by 2
		unsigned __int8 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;
	size_t len_buf = 0;
	size_t color_num = 0;

	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} palette[16] =
	{ { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
	  { 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 },
	  { 0x4, 0x7, 0x6 }, { 0x6, 0x7, 0x6 }, { 0x4, 0x4, 0x4 }, { 0x6, 0x6, 0x6 },
	  { 0x1, 0x1, 0x4 }, { 0x5, 0x6, 0x2 }, { 0x5, 0x5, 0x5 }, { 0x7, 0x7, 0x7 } };

	struct RXx {
		struct {
			struct {
				unsigned __int8 F;
				unsigned __int8 X[3];
				unsigned __int8 Y[3];
				struct {
					unsigned __int8 B;
					unsigned __int8 R;
					unsigned __int8 G;
				} P[6];
				unsigned __int8 O[4][3];
			} Entry[6];
			unsigned __int8 F[34];
		} Sector[50];
	};

	struct RXx_decoded {
		size_t offset_X;
		size_t offset_Y;
		size_t begin_X;
		size_t begin_Y;
		size_t end_X;
		size_t end_Y;
		Pal palette[6];
		unsigned __int8 flag;
	};

	std::vector<RXx_decoded> R;

	bool is_end(unsigned __int8* pos) {
		if (*pos == 0xFF) {
			size_t remain = this->len_buf - (pos - &this->buf->len_hx);

			while (--remain) {
				if (*++pos) {
					return false;
				}
			}
			return true;
		}

		return false;
	}

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	int transparent = -1;

	bool init(std::vector<__int8>& buffer, std::vector<__int8>& palette_buffer, size_t num)
	{
		if (palette_buffer.size() != 0x3200) {
			return true;
		}

		this->color_num = num;
		this->buf = (format_GS*)&buffer.at(0);
		this->len_buf = buffer.size();
		RXx* rxx = (RXx*)&palette_buffer.at(0);

		this->len_x = this->buf->len_hx ? (size_t)this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		if (this->len_y > 212) {
			return true;
		}
		unsigned __int8 xC[4] = { 0, 0, 0, 0 };
		unsigned __int8 yC[4] = { 0, 0, 0, 0 };
		unsigned __int8 C[2] = { 0, 0 };

		RXx_decoded d;
		for (size_t i = 0; i < 50; i++) {
			for (size_t j = 0; j < 6; j++) {
				char* t;
				C[0] = rxx->Sector[i].Entry[j].F;
				d.flag = strtoull((const char*)C, &t, 10);
				xC[0] = rxx->Sector[i].Entry[j].X[0];
				xC[1] = rxx->Sector[i].Entry[j].X[1];
				xC[2] = rxx->Sector[i].Entry[j].X[2];
				yC[0] = rxx->Sector[i].Entry[j].Y[0];
				yC[1] = rxx->Sector[i].Entry[j].Y[1];
				yC[2] = rxx->Sector[i].Entry[j].Y[2];
				d.offset_X = strtoull((const char*)xC, &t, 10);
				d.offset_Y = strtoull((const char*)yC, &t, 10);
				xC[0] = rxx->Sector[i].Entry[j].O[0][0];
				xC[1] = rxx->Sector[i].Entry[j].O[0][1];
				xC[2] = rxx->Sector[i].Entry[j].O[0][2];
				yC[0] = rxx->Sector[i].Entry[j].O[1][0];
				yC[1] = rxx->Sector[i].Entry[j].O[1][1];
				yC[2] = rxx->Sector[i].Entry[j].O[1][2];
				d.begin_X = strtoull((const char*)xC, &t, 10);
				d.begin_Y = strtoull((const char*)yC, &t, 10);
				xC[0] = rxx->Sector[i].Entry[j].O[2][0];
				xC[1] = rxx->Sector[i].Entry[j].O[2][1];
				xC[2] = rxx->Sector[i].Entry[j].O[2][2];
				yC[0] = rxx->Sector[i].Entry[j].O[3][0];
				yC[1] = rxx->Sector[i].Entry[j].O[3][1];
				yC[2] = rxx->Sector[i].Entry[j].O[3][2];
				d.end_X = strtoull((const char*)xC, &t, 10) + 1;
				d.end_Y = strtoull((const char*)yC, &t, 10) + 1;
				for (size_t k = 0; k < 6; k++) {
					C[0] = rxx->Sector[i].Entry[j].P[k].B;
					d.palette[k].B = strtoull((const char*)C, &t, 10);
					C[0] = rxx->Sector[i].Entry[j].P[k].R;
					d.palette[k].R = strtoull((const char*)C, &t, 10);
					C[0] = rxx->Sector[i].Entry[j].P[k].G;
					d.palette[k].G = strtoull((const char*)C, &t, 10);
				}
				R.push_back(d);
			}
		}
		//		std::wcout << this->len_x << L"," << this->len_y << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		for (size_t i = 0; i < 10; i++) {
			c.red = d3tod8(this->palette[i].R);
			c.green = d3tod8(this->palette[i].G);
			c.blue = d3tod8(this->palette[i].B);
			pal.push_back(c);
		}

		size_t tnum = this->color_num;
		switch (tnum) {
		case 43:
			tnum = 51;
			break;
		case 46:
			tnum = 52;
			break;
		case 57:
			tnum = 9;
			break;
		case 58:
			tnum = 13;
			break;
		case 72:
			tnum = 38;
			break;
		case 73:
			tnum = 233;
			break;
		case 74:
			tnum = 59;
			break;
		case 76:
			tnum = 75;
			break;
		case 84:
			tnum = 55;
			break;
		case 97:
			tnum = 229;
			break;
		case 99:
			tnum = 98;
			break;
		case 109:
		case 110:
			tnum = 14;
			break;
		case 120:
			tnum = 20;
			break;
		case 124:
			tnum = 231;
			break;
		case 148:
			tnum = 138;
			break;
		case 154:
			tnum = 17;
			break;
		case 157:
			tnum = 156;
			break;
		case 164:
			tnum = 138;
			break;
		case 166:
			tnum = 218;
			break;
		case 197:
			tnum = 156;
			break;
		case 256:
		case 257:
		case 258:
		case 259:
			tnum = 207;
			break;
		case 281:
			tnum = 223;
			break;
		default:
			break;
		}

		if (this->R[tnum - 1].flag == 0) {
			if (this->R[tnum - 1].palette[0].R || this->R[tnum - 1].palette[0].G || this->R[tnum - 1].palette[0].B) {
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->R[tnum - 1].palette[i].R);
					c.green = d3tod8(this->R[tnum - 1].palette[i].G);
					c.blue = d3tod8(this->R[tnum - 1].palette[i].B);
					pal.push_back(c);
				}
			}
			else if (tnum == 297) {
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->palette[10 + i].R);
					c.green = d3tod8(this->palette[10 + i].G);
					c.blue = d3tod8(this->palette[10 + i].B);
					pal.push_back(c);
				}
			}
			else {
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->palette[0].R);
					c.green = d3tod8(this->palette[0].G);
					c.blue = d3tod8(this->palette[0].B);
					pal.push_back(c);
				}
			}
		}
		else {
			if (tnum == 298) {

				tnum = 20;
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->R[tnum - 1].palette[i].R);
					c.green = d3tod8(this->R[tnum - 1].palette[i].G);
					c.blue = d3tod8(this->R[tnum - 1].palette[i].B);
					pal.push_back(c);
				}
				tnum = 26;
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->R[tnum - 1].palette[i].R);
					c.green = d3tod8(this->R[tnum - 1].palette[i].G);
					c.blue = d3tod8(this->R[tnum - 1].palette[i].B);
					pal.push_back(c);
				}

				for (size_t s = 0; s < I.size(); s++) {
					if (s % 512 >= 256 && I.at(s) >= 10) {
						I.at(s) += 6;
					}
				}
			}
			else {
				for (size_t i = 0; i < 6; i++) {
					c.red = d3tod8(this->R[tnum - 1].palette[i].R);
					c.green = d3tod8(this->R[tnum - 1].palette[i].G);
					c.blue = d3tod8(this->R[tnum - 1].palette[i].B);
					pal.push_back(c);
				}
			}
		}
		std::cout << std::endl;

		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);

		trans.assign(pal.size() - 1, 0xFF);
		trans.push_back(0);
		this->transparent = trans.size() - 1;
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		this->offset_x = this->R[this->color_num - 1].offset_X;
		this->offset_y = this->R[this->color_num - 1].offset_Y;
		std::cout << std::setw(3) << this->color_num << ":" << std::setw(3) << this->offset_x << "," << std::setw(3) << this->offset_y << " ";

		unsigned __int8* src = this->buf->body, prev = ~*src;
		bool repeat = false;

		while (!this->is_end(src)) {
			if (repeat) {
				repeat = false;
				int cp_len = *src;
				if (cp_len > 0) {
					uPackedPixel4 u = { prev };

					for (size_t k = 0; k < cp_len; k++) {
						I.push_back(u.S.H);
						I.push_back(u.S.L);
					}
				}
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

		std::wcerr << std::setw(6) << I.size() << L"/" << std::setw(6) << this->len_x * this->len_y << L" ";
		I.resize((size_t)this->len_x * this->len_y);

#if 0
		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}

#else
		FI.assign(MSX_SCREEN7_H * MSX_SCREEN7_V, this->transparent);
		for (size_t j = 0; j < this->len_y; j++) {
			memcpy_s(&FI[(this->offset_y + j) * MSX_SCREEN7_H + this->offset_x], this->len_x, &I[j * this->len_x], this->len_x);
		}

		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			out_body.push_back((png_bytep)&FI.at(j * MSX_SCREEN7_H));
		}
#endif
	}
};
#pragma pack(pop)
