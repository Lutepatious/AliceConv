#ifndef TOPNG_PMS
#define TOPNG_PMS

#pragma pack(push)
#pragma pack(1)
class PMS {
	std::vector<unsigned __int8> I;

	struct format_PMS { // Super D.P.Sからの旧PMS(VSP256)
		unsigned __int16 start_x;
		unsigned __int16 start_y;
		unsigned __int16 end_x;
		unsigned __int16 end_y;
		unsigned __int8 unk0;
		unsigned __int8 unk1;
		unsigned __int16 unk2;
		unsigned __int64 Sig0;
		unsigned __int64 Sig1;
		unsigned __int32 Sig2;
		struct Palette_depth8 Pal8[256];
		unsigned __int8 body[];
	} *buf = nullptr;

	struct format_PMS8 { // 鬼畜王ランスからのPMS(PMS8)
		unsigned __int16 Sig; // 'P','M'
		unsigned __int16 Ver;
		unsigned __int16 len_hdr; // 0x30?
		unsigned __int8 depth; // 8
		unsigned __int8 bits;
		unsigned __int16 unk0;
		unsigned __int16 unk1;
		unsigned __int32 unk2;
		unsigned __int32 start_x;
		unsigned __int32 start_y;
		unsigned __int32 len_x;
		unsigned __int32 len_y;
		unsigned __int32 offset_body;
		unsigned __int32 offset_Pal;
		unsigned __int64 unk3;
		unsigned __int8 body[];
	} *buf8 = nullptr;

	size_t len_buf = 0;
	unsigned transparent = 0; // デフォルトの透過色が不明
	bool is_PMS8 = false;

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC9801_H;
	png_uint_32 len_y = PC9801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	png_uint_32 disp_x = PC9801_H;
	png_uint_32 disp_y = PC9801_V;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_PMS)) {
			wouterr(L"File too short.");
			return true;
		}

		this->buf = (format_PMS*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (this->buf->Sig0 != 0 || this->buf->Sig1 != 0 || this->buf->Sig2 != 0) {
			wouterr(L"Wrong signature.");
			return true;
		}

		if (this->buf->end_x < this->buf->start_x || this->buf->end_y < this->buf->start_y) {
			wouterr(L"Wrong size.");
			return true;
		}

		this->len_x = this->buf->end_x - this->buf->start_x + 1;
		this->len_y = this->buf->end_y - this->buf->start_y + 1;
		this->offset_x = this->buf->start_x;
		this->offset_y = this->buf->start_y;

		if (this->buf->end_x > VGA_H || this->buf->end_y > VGA_V) {
			wouterr(L"Wrong size.");
			return true;
		}

		out_image_info(this->offset_x, this->offset_y, this->len_x, this->len_y, L"PMS", this->buf->unk0, this->buf->unk1);
		return false;
	}

	bool init8(std::vector<__int8>& buffer)
	{
		this->is_PMS8 = true;

		if (buffer.size() < sizeof(format_PMS8)) {
			wouterr(L"File too short.");
			return true;
		}

		this->buf8 = (format_PMS8*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (this->buf8->Sig != 0x4D50) {
			wouterr(L"Wrong signature.");
			return true;
		}

		this->len_x = this->buf8->len_x;
		this->len_y = this->buf8->len_y;
		this->offset_x = this->buf8->start_x;
		this->offset_y = this->buf8->start_y;

		if ((size_t)this->buf8->start_x + this->buf8->len_x > VGA_H || (size_t)this->buf8->start_y + this->buf8->len_y > VGA_V) {
			wouterr(L"Wrong size.");
			return true;
		}

		out_image_info(this->offset_x, this->offset_y, this->len_x, this->len_y, L"PMS8", this->buf8->unk0, this->buf8->unk1);
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;

		struct Palette_depth8* Pal8 = is_PMS8 ? (Palette_depth8*)((unsigned __int8*)this->buf8 + this->buf8->offset_Pal) : this->buf->Pal8;
		for (size_t i = 0; i < 0x100; i++) {
			c.red = (Pal8 + i)->R;
			c.green = (Pal8 + i)->G;
			c.blue = (Pal8 + i)->B;
			pal.push_back(c);
			trans.push_back(0xFF);
		}
		//		trans.at(this->transparent) = 0; // 透過色が判明するまではとりあえずコメントアウト
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->is_PMS8 ? ((unsigned __int8*)this->buf8 + this->buf8->offset_body) : this->buf->body;
		size_t count = this->is_PMS8 ? this->len_buf - this->buf8->offset_body : this->len_buf - sizeof(format_PMS);

//		std::wcout << count << L"," << image_size << std::endl;

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < image_size) {
			switch (*src) {
			case 0xF8: // 何もしない、エスケープ用
			case 0xF9:
			case 0xFA:
			case 0xFB:
				D.push_back(*(src + 1));
				src += 2;
				count--;
				break;

			case 0xFC: // 2,3バイト目の2ピクセルの値を1バイト目で指定した回数繰り返す。
				for (size_t len = 0; len < 3ULL + *(src + 1); len++) {
					D.push_back(*(src + 2));
					D.push_back(*(src + 3));
				}
				src += 4;
				count -= 3;
				break;

			case 0xFD: // 2バイト目の1ピクセルの値を1バイト目で指定した回数繰り返す。
				for (size_t len = 0; len < 4ULL + *(src + 1); len++) {
					D.push_back(*(src + 2));
				}
				src += 3;
				count -= 2;
				break;

			case 0xFE: // 2行前の値を1バイト目で指定した長さでコピー
				D.insert(D.end(), D.end() - 2ULL * len_x, D.end() - 2ULL * len_x + *(src + 1) + 3LL);
				src += 2;
				count--;
				break;

			case 0xFF: // 1行前の値を1バイト目で指定した長さでコピー
				D.insert(D.end(), D.end() - len_x, D.end() - len_x + *(src + 1) + 3LL);
				src += 2;
				count--;
				break;

			default:
				D.push_back(*src);
				src++;
			}
		}

		if (!this->is_PMS8) {
			if (buf->end_y > PC9801_V) {
				disp_y = VGA_V;
			}
		}

		this->I.insert(this->I.end(), (size_t)this->disp_x * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), D.begin() + this->len_x * y, D.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), (size_t)this->disp_x - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), this->disp_x * ((size_t)this->disp_y - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < this->disp_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->disp_x));
		}
	}

};
#pragma pack(pop)
#endif
