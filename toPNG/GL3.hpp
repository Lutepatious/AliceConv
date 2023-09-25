#ifndef TOPNG_GL3
#define TOPNG_GL3
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class GL3 {
	std::vector<unsigned __int8> I;

	struct format_GL3 {
		struct Palette_depth4 Pal4[16];	// もしここが空っぽならFM TOWNS版ALICEの館CDに収録されたIntruderだと思われる
		unsigned __int16 Start;
		unsigned __int16 len_x8; // divided by 8
		unsigned __int16 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;

	struct Palette_depth4 Pal4_GM3[16] =
	{ { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
	  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF },
	  { 0x0, 0x0, 0x0 }, { 0xF, 0x0, 0x0 }, { 0x0, 0xF, 0x0 }, { 0xF, 0xF, 0x0 },
	  { 0x0, 0x0, 0xF }, { 0xF, 0x0, 0xF }, { 0x0, 0xF, 0xF }, { 0xF, 0xF, 0xF } };

	size_t len_buf = 0;
	bool is_GM3 = false;

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC9801_H;
	png_uint_32 len_y = PC9801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	unsigned transparent = 16;
	png_uint_32 disp_y = PC9801_V;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_GL3)) {
			wouterr(L"File too short.");
			return true;
		}

		unsigned __int8 t = 0;
		for (int i = 0; i < 0x30; i++) {
			t |= buffer.at(i);
		}

		if (t >= 0x10) {
			wouterr(L"Wrong palette.");
			return true;
		}

		if (t == 0) {
			this->is_GM3 = true;
		}

		this->buf = (format_GL3*)&buffer.at(0);
		this->len_buf = buffer.size();

		unsigned start = this->buf->Start;
		if (start < 0x8000) {
			wouterr(L"Wrong start address.");
			return true;
		}

		start -= 0x8000;

		if (this->buf->len_x8 == 0 || this->buf->len_y == 0) {
			wouterr(L"Wrong size.");
			return true;
		}

		this->len_col = this->buf->len_x8;
		this->len_x = 8 * this->buf->len_x8;
		this->len_y = this->buf->len_y;
		this->offset_x = start % 80 * 8;
		this->offset_y = start / 80;

		if (((size_t)this->len_x + this->offset_x > PC9801_H) || ((size_t)this->len_y + this->offset_y) > PC9801_V) {
			wouterr(L"Wrong size.");
			return true;
		}

		std::wcout << L"From " << std::setw(4) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" Size " << std::setw(4) << len_x << L"," << std::setw(3) << len_y << std::endl;
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;

		for (size_t i = 0; i < 16; i++) {
			struct Palette_depth4* p;
			if (this->is_GM3) {
				p = &this->Pal4_GM3[i];
			}
			else {
				p = &this->buf->Pal4[i];
			}
			c.red = d4tod8(p->R);
			c.green = d4tod8(p->G);
			c.blue = d4tod8(p->B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}
		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		trans.push_back(0);
	}

	bool decode_body(std::vector<png_bytep>& out_body)
	{
		const unsigned planes = 4;
		const size_t decode_size = (size_t)this->len_col * this->len_y * planes;
		const size_t image_size = (size_t)this->len_x * this->len_y;
		const size_t Row1_step = this->len_col * planes;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL3);

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < decode_size) {
			switch (*src) {
			case 0x00:
				if (*(src + 1) & 0x80) { // (2行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*(src + 1) & 0x7F)回コピー
					D.insert(D.end(), *(src + 1) & 0x7F, *(D.end() - Row1_step * 2 + 1));
					src += 2;
					count--;
				}
				else { // *(src + 2)の8ドットデータを末尾に*(src + 1)回コピー
					D.insert(D.end(), *(src + 1), *(src + 2));
					src += 3;
					count -= 2;
				}
				break;

			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				// *(src + 1)の8ドットデータを末尾に*src回コピー
				D.insert(D.end(), *src, *(src + 1));
				src += 2;
				count--;
				break;

			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0D:
				// (2行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*src - 6)回コピー
				D.insert(D.end(), *src - 6, *(D.end() - Row1_step * 2 + 1));
				src++;
				break;

			case 0x0E:
			{
				// 別プレーンのデータを(*(src + 1) & 0x7F)*8ドット分末尾にコピー
				size_t target_plane = ((D.size() / len_col) % planes) - 2;
				auto cp_src = D.end() - len_col * target_plane;
				size_t cp_len = *(src + 1) & 0x7F;

				D.insert(D.end(), cp_src, cp_src + cp_len);
				src += 2;
				count--;
				break;
			}

			case 0x0F:
			{
				// 別プレーンのデータを(*(src + 1) & 0x7F)*8ドット分末尾にコピー
				size_t target_plane = ((D.size() / len_col) % planes) - ((*(src + 1) & 0x80) ? 1 : 0);
				auto cp_src = D.end() - len_col * target_plane;
				size_t cp_len = *(src + 1) & 0x7F;

				D.insert(D.end(), cp_src, cp_src + cp_len);
				src += 2;
				count--;
				break;
			}

			default:
				D.push_back(*src);
				src++;
			}
		}

		// 4プレーン表現をインデックスカラーに変換 std::bitsetでより簡潔になった。
		std::vector<unsigned __int8> P;
		for (size_t l = 0; l < D.size(); l += Row1_step) {
			for (size_t c = 0; c < this->len_col; c++) {
				std::bitset<8> b[4] = { D.at(l + c), D.at(l + c + this->len_col), D.at(l + c + this->len_col * 2), D.at(l + c + this->len_col * 3) };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0) | (b[3][7 - i] ? 8 : 0);
					P.push_back(a);
				}
			}
		}

		if (this->is_GM3) {
			this->disp_y = 201;
		}

		this->I.insert(this->I.end(), PC9801_H * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), P.begin() + this->len_x * y, P.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), PC9801_H - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), PC9801_H * ((size_t)this->disp_y - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < this->disp_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_H));
		}

		return this->is_GM3;
	}
};
#pragma pack(pop)
#endif
