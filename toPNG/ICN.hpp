#ifndef TOPNG_ICN
#define TOPNG_ICN

#pragma pack(push)
#pragma pack(1)
class ICN {
	std::vector<unsigned __int8> I;

	struct format_ICN {
		unsigned __int32 bodyBE[][32];
	} *buf = nullptr;
	size_t ICN_entries = 0;

	struct format_ICNFILE {
		unsigned __int8 Sig[8]; // "ICNFILE"
		unsigned __int32 Unk0; // 0x00011A00
		unsigned __int16 info_entries;
		unsigned __int16 info2_entries;
		unsigned __int32 start_info2;
		unsigned __int8 Unk1[12];
		struct {
			unsigned __int8 ID;
			unsigned __int8 Unk0[5];
			unsigned __int16 len;
			unsigned __int32 offs_info2;
			unsigned __int32 offs_body;
			unsigned __int32 Unk2[4];
		} info[];
	} *buf_ICNFILE = nullptr;

	struct INFO2 {
		unsigned __int8 ID;
		unsigned __int8 Unk0[3];
		unsigned __int16 l0;
		unsigned __int16 l1;
		unsigned __int16 l2;
		unsigned __int16 colors;
		unsigned __int16 Unk1;
		unsigned __int16 len;
		unsigned __int32 Unk2;
		unsigned __int32 offs_body;
		unsigned __int32 Unk[6];
	} *buf_info2 = nullptr;

	// TownsMENUのアイコンで使うパレットが不明のため、unzなどで割り出し。
	struct Palette_depth4RGB TPal[16] = {
	{0x0, 0x0, 0x0}, {0x2, 0x7, 0xB}, {0xC, 0x6, 0x4}, {0xF, 0xC, 0xA},
	{0x9, 0x9, 0x9}, {0x0, 0xC, 0x7}, {0xC, 0xC, 0xC}, {0x7, 0x7, 0x7},
	{0x2, 0x2, 0x2}, {0x8, 0xB, 0xD}, {0xC, 0x0, 0x0}, {0x0, 0x0, 0xA},
	{0x5, 0x5, 0x5}, {0x0, 0xF, 0xF}, {0xF, 0xD, 0x0}, {0xF, 0xF, 0xF} };

public:
	png_uint_32 disp_x = 32;
	png_uint_32 disp_y = 32;
	bool is_new = false;

	bool init(std::vector<__int8>& buffer)
	{
		size_t len_buf = buffer.size();
		if (strlen(&buffer.at(0)) == 0) {
			if (len_buf % (sizeof(unsigned __int32) * 32)) {
				wouterr(L"Size mismatch.");
				return true;
			}
			this->buf = (struct format_ICN*)&buffer.at(0);
			this->ICN_entries = len_buf / (sizeof(unsigned __int32) * 32);
			return false;
		}
		else if (strcmp(&buffer.at(0), "ICNFILE") == 0) {
			this->buf_ICNFILE = (struct format_ICNFILE*)&buffer.at(0);
			this->buf_info2 = (struct INFO2*)(&buffer.at(0) + this->buf_ICNFILE->start_info2);
			this->is_new = true;

			return false;
		}
		return true;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;

		if (this->is_new) {
			for (size_t i = 0; i < 16; i++) {
				c.red = d4tod8(this->TPal[i].R);
				c.green = d4tod8(this->TPal[i].G);
				c.blue = d4tod8(this->TPal[i].B);
				pal.push_back(c);
				trans.push_back(0xFF);
			}
			c.red = 0;
			c.green = 0;
			c.blue = 0;
			pal.push_back(c);
			trans.push_back(0);
		}
		else {
			c.red = 0xFF;
			c.green = 0xFF;
			c.blue = 0xFF;
			pal.push_back(c);
			trans.push_back(0xFF);

			c.red = 0;
			c.green = 0;
			c.blue = 0;
			pal.push_back(c);
			trans.push_back(0xFF);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		if (this->is_new) {
			// 後期フォーマット 不定形不定長であるため、各エントリごとに64*64の領域を与え、範囲外を透過にして出力。
#if 0
			for (size_t i = 0; i < this->buf_ICNFILE->info_entries; i++) {
				std::wcout << std::setw(3) << i << L":" << std::hex << std::setw(2) << this->buf_ICNFILE->info[i].ID
					<< L":" << std::hex << std::setw(8) << this->buf_ICNFILE->info[i].offs_info2
					<< L":" << std::setw(8) << this->buf_ICNFILE->info[i].offs_body
					<< L":" << std::setw(4) << this->buf_ICNFILE->info[i].len << std::endl;
			}
			for (size_t i = 0; i < this->buf_ICNFILE->info2_entries; i++) {
				std::wcout << std::setw(3) << i << L":" << std::hex << std::setw(2) << (this->buf_info2 + i)->ID
					<< std::setw(4) << (this->buf_info2 + i)->l0 << L":" << std::setw(3) << (this->buf_info2 + i)->colors << std::endl;
			}
#endif


			this->disp_x = 64;
			this->disp_y = 64 * this->buf_ICNFILE->info2_entries;

			for (size_t i = 0; i < this->buf_ICNFILE->info2_entries; i++) {
				if ((this->buf_info2 + i)->colors == 2) {
					unsigned __int8* src = (unsigned __int8*)this->buf_ICNFILE + (this->buf_info2 + i)->offs_body;

					if ((this->buf_info2 + i)->l0 % 8) {
						std::wcout << L"Invalid column length." << std::endl;
					}

					size_t col = (this->buf_info2 + i)->l0 / 8 + ((this->buf_info2 + i)->l0 % 8 ? 1 : 0);

					for (size_t y = 0; y < (this->buf_info2 + i)->l1; y++) {
						for (size_t c = 0; c < col; c++) {
							std::bitset<8> t{ *src };

							for (size_t i = 0; i < 8; i++) {
								unsigned __int8 a = (t[7 - i] ? 0 : 0xF);
								this->I.push_back(a);
							}
							src++;
						}

						size_t remain = 64 - (this->buf_info2 + i)->l0;
						if (remain) {
							I.insert(I.end(), remain, 0x10);
						}
					}

					size_t yremain = 64 - (this->buf_info2 + i)->l1;
					if (yremain) {
						I.insert(I.end(), yremain * 64, 0x10);
					}
				}
				else if ((this->buf_info2 + i)->colors == 16) {
					unsigned __int8* src = (unsigned __int8*)this->buf_ICNFILE + (this->buf_info2 + i)->offs_body;

					if ((this->buf_info2 + i)->l0 % 2) {
						std::wcout << L"Invalid column length." << std::endl;
					}

					size_t col = (this->buf_info2 + i)->l0 / 2 + ((this->buf_info2 + i)->l0 % 2 ? 1 : 0);

					for (size_t y = 0; y < (this->buf_info2 + i)->l1; y++) {
						for (size_t c = 0; c < col; c++) {
							union {
								unsigned __int8 A;
								struct {
									unsigned __int8 L : 4;
									unsigned __int8 H : 4;
								} B;
							} u;
							u.A = *src;

							this->I.push_back(u.B.L);
							this->I.push_back(u.B.H);
							src++;
						}

						if ((this->buf_info2 + i)->l0 == 0x36) {
							src++;
						}

						size_t remain = 64 - (this->buf_info2 + i)->l0;
						if (remain) {
							I.insert(I.end(), remain, 0x10);
						}
					}

					size_t yremain = 64 - (this->buf_info2 + i)->l1;
					if (yremain) {
						I.insert(I.end(), yremain * 64, 0x10);
					}
				}
			}
		}
		else {
			size_t blocks = 16;
			for (size_t i = 0; i < this->ICN_entries; i += blocks) {
				for (size_t y = 0; y < 32; y++) {
					for (size_t b = 0; b < blocks; b++) {
						std::bitset<32> t{ _byteswap_ulong(this->buf->bodyBE[i + b][y]) };

						for (size_t i = 0; i < 32; i++) {
							unsigned __int8 a = (t[31 - i] ? 1 : 0);
							this->I.push_back(a);
						}
					}
				}
			}

			this->disp_x = 32 * blocks;
			this->disp_y = 32 * this->ICN_entries / blocks;
		}

		for (size_t j = 0; j < this->disp_y; j++) {
			out_body.push_back((png_bytep)&this->I.at(j * this->disp_x));
		}
	}

};
#pragma pack(pop)
#endif
