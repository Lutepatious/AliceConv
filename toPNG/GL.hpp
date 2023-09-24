#ifndef TOPNG_GL
#define TOPNG_GL
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class GL {
	std::vector<unsigned __int8> I;

	struct format_GL {
		unsigned __int16 Start; // (big endian表現) PC-8801ベースでのLoad address
		unsigned __int8 len_x8; // divided by 8
		unsigned __int8 len_y;
		unsigned __int32 unk;
		struct Palette_depth3 Pal3[8];
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC8801_H;
	png_uint_32 len_y = PC8801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	unsigned transparent = 8;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_GL)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		for (int i = 0x04; i < 0x0C; i++) {
			unsigned __int16 t = *((unsigned __int16*)&buffer.at(0) + i) & 0xF8C0;
			if (t != 0x4000) {
				std::wcerr << "Wrong palette." << std::endl;
				return true;
			}
		}

		this->buf = (format_GL*)&buffer.at(0);
		this->len_buf = buffer.size();

		unsigned start = _byteswap_ushort(this->buf->Start);
		if (start < 0xC000) {
			std::wcerr << "Wrong start address." << std::endl;
			return true;
		}

		start -= 0xC000;

		if (this->buf->len_x8 == 0 || this->buf->len_y == 0) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		this->len_col = this->buf->len_x8;
		this->len_x = 8 * this->buf->len_x8;
		this->len_y = this->buf->len_y;
		this->offset_x = start % 80 * 8;
		this->offset_y = start / 80;

		if (((size_t)this->len_x + this->offset_x > PC8801_H) || ((size_t)this->len_y + this->offset_y) > PC8801_V) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		std::wcout << L"From " << std::setw(4) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" Size " << std::setw(4) << len_x << L"," << std::setw(3) << len_y << std::endl;
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		for (size_t i = 0; i < 8; i++) {
			c.red = d3tod8(this->buf->Pal3[i].R);
			c.green = d3tod8(this->buf->Pal3[i].G);
			c.blue = d3tod8(this->buf->Pal3[i].B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}
		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.push_back(c);
		trans.push_back(0);
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const unsigned planes = 3;
		const size_t decode_size = (size_t)this->len_col * this->len_y * planes;
		const size_t image_size = (size_t)this->len_x * this->len_y;
		const size_t Row1_step = this->len_col * planes;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_GL);

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < decode_size) {
			switch (*src) {
			case 0x00:
				if (*(src + 1) & 0x80) { // (4行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*(src + 1) & 0x7F)回コピー
					D.insert(D.end(), *(src + 1) & 0x7F, *(D.end() - Row1_step * 4 + 1));
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
			case 0x0E:
				// (4行前+8ドット先)の同プレーンのデータ8ドットを末尾に(*src - 6)回コピー
				D.insert(D.end(), *src - 6, *(D.end() - Row1_step * 4 + 1));
				src++;
				break;

			case 0x0F:
			{
				// 別プレーンのデータを(*(src + 1) & 0x7F)*8ドット分末尾にコピー
				size_t target_plane = ((D.size() / len_col) % planes) - ((*(src + 1) & 0x80) ? 1 : 0);
				auto cp_src = D.end() - len_col * target_plane;
				D.insert(D.end(), cp_src, cp_src + (*(src + 1) & 0x7F));
				src += 2;
				count--;
				break;
			}

			default:
				D.push_back(*src);
				src++;
			}
		}

		// 3プレーン表現をインデックスカラーに変換 std::bitsetでより簡潔になった。
		std::vector<unsigned __int8> P;
		for (size_t l = 0; l < D.size(); l += Row1_step) {
			for (size_t c = 0; c < this->len_col; c++) {
				std::bitset<8> b[3] = { D.at(l + c), D.at(l + c + this->len_col), D.at(l + c + this->len_col * 2) };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0);
					P.push_back(a);
				}
			}
		}

		this->I.insert(this->I.end(), PC8801_H * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), P.begin() + this->len_x * y, P.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), PC8801_H - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), PC8801_H * (PC8801_V - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < PC8801_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC8801_H));
		}
	}
};
#pragma pack(pop)
#endif
