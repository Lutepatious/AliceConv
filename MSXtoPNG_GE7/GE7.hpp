#pragma pack(push)
#pragma pack(1)
class GE7 {
	std::vector<unsigned __int8> I;
	struct format_GE7 {
		unsigned __int8 Id;
		unsigned __int16 start;
		unsigned __int16 length;
		unsigned __int16 unknown;
		PackedPixel4 body[MSX_SCREEN7_V][MSX_SCREEN7_H / 2];
		unsigned __int8 unused[0x1C00];
		unsigned __int8 splite_generator[0x800];
		unsigned __int8 splite_color[0x200];
		unsigned __int8 splite_attribute[32][4];
		struct Pal4 {
			unsigned __int16 B : 3;
			unsigned __int16 : 1;
			unsigned __int16 R : 3;
			unsigned __int16 : 1;
			unsigned __int16 G : 3;
			unsigned __int16 : 5;
		} palette[16];
	} *buf = nullptr;

public:

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_GE7)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_GE7*)&buffer.at(0);

		if (this->buf->Id != 0xFE || this->buf->start != 0 || this->buf->length != 0xFAA0 || this->buf->unknown != 0) {
			std::wcerr << "File type not match." << std::endl;
			return true;
		}

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->buf->palette[i].R);
			c.green = d3tod8(this->buf->palette[i].G);
			c.blue = d3tod8(this->buf->palette[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		// 失敗談 各ラインがデコードされ次第PNG出力の構造体にアドレスをプッシュするコードにしていたが
		// std::vectorはpush_backでサイズが変わっていくと再割り当てを行うので最終的にアドレスが変わってしまう。
		// このことを忘れててデータ壊しまくってた
		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			for (size_t i = 0; i < MSX_SCREEN7_H / 2; i++) {
				I.push_back(this->buf->body[j][i].H);
				I.push_back(this->buf->body[j][i].L);
			}
		}
		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * MSX_SCREEN7_H));
		}
	}
};

#pragma pack(pop)
