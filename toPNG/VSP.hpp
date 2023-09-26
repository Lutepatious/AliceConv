#ifndef TOPNG_VSP
#define TOPNG_VSP
#include "toPNG.hpp"

#pragma pack(push)
#pragma pack(1)
class VSP { // VSPのVSはVertical Scanの事か? 各プレーンの8ドット(1バイト)を縦にスキャンしてデータが構成されている
	std::vector<unsigned __int8> I;

	struct format_VSP {
		unsigned __int16 start_x8; // divided by 8
		unsigned __int16 start_y;
		unsigned __int16 end_x8; // divided by 8
		unsigned __int16 end_y;
		unsigned __int16 Unknown;
		struct Palette_depth4 Pal4[16];
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;
	unsigned transparent = 16;

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC9801_H;
	png_uint_32 len_y = PC9801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_VSP)) {
			wouterr(L"File too short.");
			return true;
		}

		unsigned __int8 t = 0;
		for (int i = 0x0A; i < 0x3A; i++) {
			t |= buffer.at(i);
		}

		if (t >= 0x10) {
			wouterr(L"Wrong palette.");
			return true;
		}

		this->buf = (format_VSP*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (this->buf->end_x8 <= this->buf->start_x8 || this->buf->end_y <= this->buf->start_y) {
			wouterr(L"Wrong size.");
			return true;
		}

		this->len_col = this->buf->end_x8 - this->buf->start_x8;
		this->len_x = 8 * (this->buf->end_x8 - this->buf->start_x8);
		this->len_y = this->buf->end_y - this->buf->start_y;
		this->offset_x = 8 * this->buf->start_x8;
		this->offset_y = this->buf->start_y;

		if ((8ULL * this->buf->end_x8 > PC9801_H) || ((this->buf->end_x8 > PC9801_V))) {
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
			c.red = d4tod8(this->buf->Pal4[i].R);
			c.green = d4tod8(this->buf->Pal4[i].G);
			c.blue = d4tod8(this->buf->Pal4[i].B);
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
		const unsigned planes = 4;
		const size_t decode_size = (size_t)this->len_col * this->len_y * planes;
		const size_t image_size = (size_t)this->len_x * this->len_y;
		const size_t Col1_step = (size_t)this->len_y * planes;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_VSP);
		bool negate = false;

		std::vector<unsigned __int8> D;
		size_t index;
		while (count-- && D.size() < decode_size) {
			switch (*src) {
			case 0x00: // 1列前の同プレーンのデータを1バイト目で指定した長さだけコピー
				D.insert(D.end(), D.end() - Col1_step, D.end() - Col1_step + *(src + 1) + 1);
				src += 2;
				count--;
				break;

			case 0x01: // 2バイト目の値を1バイト目で指定した長さ繰り返す
				for (size_t len = 0; len < 1ULL + *(src + 1); len++) {
					D.push_back(*(src + 2));
				}
				src += 3;
				count -= 2;
				break;

			case 0x02: // 2-3バイト目の値を1バイト目で指定した長さ繰り返す
				for (size_t len = 0; len < 1ULL + *(src + 1); len++) {
					D.push_back(*(src + 2));
					D.push_back(*(src + 3));
				}
				src += 4;
				count -= 3;
				break;

			case 0x03: // 第0プレーン(青)のデータを1バイト目で指定した長さだけコピー(negateが1ならビット反転する)
				index = D.size() - this->len_y * ((D.size() / this->len_y) % planes);
				for (size_t len = 0; len < 1ULL + *(src + 1); len++) {
					auto t = D.at(index++);
					D.push_back(negate ? ~t : t);
				}
				src += 2;
				count--;
				negate = false;
				break;

			case 0x04: // 第1プレーン(赤)のデータを1バイト目で指定した長さだけコピー(negateが1ならビット反転する)
				index = D.size() - this->len_y * ((D.size() / this->len_y) % planes - 1);
				for (size_t len = 0; len < 1ULL + *(src + 1); len++) {
					auto t = D.at(index++);
					D.push_back(negate ? ~t : t);
				}
				src += 2;
				count--;
				negate = false;
				break;

			case 0x05: // 第2プレーン(緑)のデータを1バイト目で指定した長さだけコピー(negateが1ならビット反転する)、VSP200lではここに制御が来てはならない。
				index = D.size() - this->len_y * ((D.size() / this->len_y) % planes - 2);
				for (size_t len = 0; len < 1ULL + *(src + 1); len++) {
					auto t = D.at(index++);
					D.push_back(negate ? ~t : t);
				}
				src += 2;
				count--;
				negate = false;
				break;

			case 0x06: // 機能3,4,5でコピーするデータを反転するか指定
				src++;
				negate = 1;
				break;

			case 0x07: // 何もしない、エスケープ用
				src++;
				D.push_back(*src);
				src++;
				count--;
				break;

			default:
				D.push_back(*src);
				src++;
			}
		}

		// 4プレーン表現をインデックスカラーに変換 std::bitsetでより簡潔になった。
		std::vector<unsigned __int8> P;
		for (size_t l = 0; l < this->len_y; l++) {
			for (size_t c = 0; c < D.size(); c += Col1_step) {
				std::bitset<8> b[4] = { D.at(l + c), D.at(l + c + this->len_y), D.at(l + c + this->len_y * 2), D.at(l + c + this->len_y * 3) };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0) | (b[3][7 - i] ? 8 : 0);
					P.push_back(a);
				}
			}
		}

		this->I.insert(this->I.end(), PC9801_H * this->offset_y, this->transparent);
		for (size_t y = 0; y < this->len_y; y++) {
			this->I.insert(this->I.end(), this->offset_x, this->transparent);
			this->I.insert(this->I.end(), P.begin() + this->len_x * y, P.begin() + this->len_x * (y + 1));
			this->I.insert(this->I.end(), PC9801_H - this->offset_x - this->len_x, this->transparent);
		}
		this->I.insert(I.end(), PC9801_H * (PC9801_V - this->offset_y - this->len_y), this->transparent);

		for (size_t j = 0; j < PC9801_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_H));
		}
	}
};
#pragma pack(pop)
#endif
