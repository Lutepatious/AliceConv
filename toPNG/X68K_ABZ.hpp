#ifndef TOPNG_X68K_ABZ
#define TOPNG_X68K_ABZ

#pragma pack(push)
#pragma pack(1)
class X68K_ABZ { // RanceIII オプションセット あぶない文化祭前夜 X68000 256色
	std::vector<unsigned __int8> I;

	struct format_X68K_ABZ {
		unsigned __int32 SigBE; // 0x00200010
		unsigned __int16 len_x_BE;
		unsigned __int16 len_y_BE;
		unsigned __int16 Pal5BE[0x100]; // Palette No. 0x00 to 0xFE 最後の1個は無効?
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;
	unsigned transparent = 0xFF;

public:
	png_uint_32 len_x = X68000_GX;
	png_uint_32 len_y = X68000_G;
	png_int_32 offset_x = 96; // 開始点がプログラム上で決め打ちになっていた。
	png_int_32 offset_y = 56; // 同上
	png_uint_32 disp_x = X68000_GX;
	png_uint_32 disp_y = X68000_G;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_X68K_ABZ)) {
			wouterr(L"File too short.");
			return true;
		}

		this->buf = (format_X68K_ABZ*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (_byteswap_ulong(this->buf->SigBE) != 0x00200010) {
			wouterr(L"Wrong signature.");
			return true;
		}

		this->len_x = _byteswap_ushort(this->buf->len_x_BE);
		this->len_y = _byteswap_ushort(this->buf->len_y_BE);

		if ((size_t)this->len_x + this->offset_x > X68000_G || (size_t)this->len_y + this->offset_y > X68000_G) {
			wouterr(L"Wrong size.");
			return true;
		}

		out_image_info(this->offset_x, this->offset_y, this->len_x, this->len_y, L"GL_ABZ");
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		union X68Pal_conv {
			struct Palette_depth5 Pal5;
			unsigned __int16 Pin;
		} P;

		png_color c;

		for (size_t i = 0; i < 0x100; i++) {
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
		size_t count = this->len_buf - sizeof(format_X68K_ABZ);

		__int64 cp_len;
		unsigned __int8 prev = ~*src;
		bool repeat = false;

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < image_size) { // 単純なRLEで同値が続いたら次の値は繰り返し回数(それまでの分を含めたもの)。
			if (repeat) {
				repeat = false;
				cp_len = *src - 2; // range -2 to 253. Minus cancells previous data. 
				if (cp_len > 0) {
					D.insert(D.end(), cp_len, prev);
				}
				src++;
				prev = ~*src;
			}
			else if (*src == prev) {
				repeat = true;
				prev = *src;
				D.push_back(*src);
				src++;
			}
			else {
				repeat = false;
				prev = *src;
				D.push_back(*src);
				src++;
			}
		}

		this->I.insert(this->I.end(), disp_x * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), D.begin() + this->len_x * y, D.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), disp_x - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), disp_x * ((size_t)disp_y - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < disp_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * disp_x));
		}
	}
};
#pragma pack(pop)
#endif
