#ifndef TOPNG_PMS16
#define TOPNG_PMS16

#pragma pack(push)
class PMS16 {
	std::vector<unsigned __int8> I;

	struct format_PMS16 {
		unsigned __int16 Sig; // 'P','M'
		unsigned __int16 Ver;
		unsigned __int16 len_hdr; // 0x30?
		unsigned __int8 depth; // 16
		unsigned __int8 bits;
		unsigned __int16 unk0;
		unsigned __int16 unk1;
		unsigned __int32 unk2;
		unsigned __int32 start_x;
		unsigned __int32 start_y;
		unsigned __int32 len_x;
		unsigned __int32 len_y;
		unsigned __int32 offset_body;
		unsigned __int32 offset_Alpha;
		unsigned __int64 unk3;
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

	struct COLOR16BGR {
		unsigned __int16 B : 5;
		unsigned __int16 G : 6;
		unsigned __int16 R : 5;
	};

	struct COLOR32RGBA {
		unsigned __int8 R;
		unsigned __int8 G;
		unsigned __int8 B;
		unsigned __int8 A;
	};

public:
	png_uint_32 len_x = PC9801_H;
	png_uint_32 len_y = PC9801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	png_uint_32 disp_x = PC9801_H;
	png_uint_32 disp_y = PC9801_V;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_PMS16)) {
			wouterr(L"File too short.");
			return true;
		}

		this->buf = (format_PMS16*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (this->buf->Sig != 0x4D50) {
			wouterr(L"Wrong signature.");
			return true;
		}

		if (this->buf->depth != 0x10) {
			wouterr(L"Not PMS16.");
			return true;
		}

		if ((size_t)this->buf->start_x + this->buf->len_x > SVGA_H || (size_t)this->buf->start_y + this->buf->len_y > SVGA_V) {
			wouterr(L"Wrong size.");
			return true;
		}

		this->len_x = this->buf->len_x;
		this->len_y = this->buf->len_y;
		this->offset_x = this->buf->start_x;
		this->offset_y = this->buf->start_y;

		out_image_info(this->offset_x, this->offset_y, this->len_x, this->len_y, L"PMS16", this->buf->unk0, this->buf->unk1);
		return false;
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		bool exist_Alpha = false;
		unsigned __int8* Alpha_body = nullptr;
		const size_t image_size = (size_t)this->len_x * this->len_y;

		std::vector<unsigned __int8> A;
		if (this->buf->offset_Alpha != 0) {
			exist_Alpha = true;
			size_t Acount = this->len_buf - this->buf->offset_Alpha;
			Alpha_body = (unsigned __int8*)this->buf + this->buf->offset_Alpha;

			while (Acount-- && A.size() < image_size) {
				switch (*Alpha_body) {
				case 0xF8: // 何もしない、エスケープ用
				case 0xF9:
				case 0xFA:
				case 0xFB:
					A.push_back(*(Alpha_body + 1));
					Alpha_body += 2;
					Acount--;
					break;

				case 0xFC: // 2,3バイト目の2ピクセルの値を1バイト目で指定した回数繰り返す。
					for (size_t len = 0; len < 3ULL + *(Alpha_body + 1); len++) {
						A.push_back(*(Alpha_body + 2));
						A.push_back(*(Alpha_body + 3));
					}
					Alpha_body += 4;
					Acount -= 3;
					break;

				case 0xFD: // 2バイト目の1ピクセルの値を1バイト目で指定した回数繰り返す。
					for (size_t len = 0; len < 4ULL + *(Alpha_body + 1); len++) {
						A.push_back(*(Alpha_body + 2));
					}
					Alpha_body += 3;
					Acount -= 2;
					break;

				case 0xFE: // 2行前の値を1バイト目で指定した長さでコピー
					A.insert(A.end(), A.end() - 2ULL * len_x, A.end() - 2ULL * len_x + *(Alpha_body + 1) + 3LL);
					Alpha_body += 2;
					Acount--;
					break;

				case 0xFF: // 1行前の値を1バイト目で指定した長さでコピー
					A.insert(A.end(), A.end() - len_x, A.end() - len_x + *(Alpha_body + 1) + 3LL);
					Alpha_body += 2;
					Acount--;
					break;

				default:
					A.push_back(*Alpha_body);
					Alpha_body++;
				}
			}
		}
		else {
			A.insert(A.end(), 0xFF, image_size);
		}

		unsigned __int8* src = (unsigned __int8*)this->buf + this->buf->offset_body;
		size_t count = (exist_Alpha ? (size_t)this->buf->offset_Alpha : this->len_buf) - this->buf->offset_body;

		size_t cp_len;
		std::vector<unsigned __int16> D16;
		while (count-- && D16.size() < image_size) {
			union {
				unsigned __int8 B[2];
				unsigned __int16 W;
			} u;

			// PMS16で近似する色を圧縮表現している部分から復元する為のビットフィールド構造体
			// 下位計8ビット分
			union {
				struct {
					unsigned __int8 Bl : 2;
					unsigned __int8 Gl : 4;
					unsigned __int8 Rl : 2;
				} S;
				unsigned __int8 B;
			} ul;

			// 上位計8ビット分
			union {
				struct {
					unsigned __int8 Bh : 3;
					unsigned __int8 Gh : 2;
					unsigned __int8 Rh : 3;
				} S;
				unsigned __int8 B;
			} uh;

			// 合成計16ビット分
			union {
				struct {
					unsigned __int16 Bl : 2;
					unsigned __int16 Bh : 3;
					unsigned __int16 Gl : 4;
					unsigned __int16 Gh : 2;
					unsigned __int16 Rl : 2;
					unsigned __int16 Rh : 3;
				} S;
				unsigned __int16 W;
			} uW;

			switch (*src) {
			case 0xF8:
				// 何もしない、エスケープ用
				u.B[0] = *(src + 1);
				u.B[1] = *(src + 2);
				D16.push_back(u.W);
				src += 3;
				break;

			case 0xF9:
				// 連続する近似した色の圧縮表現、1バイト目に長さ、2バイト目が各色上位ビット(B3G2R3)、3バイト目以降が各色下位ビット(B2G4R2)。
				cp_len = *(src + 1) + 1LL;
				uh.B = *(src + 2);

				uW.S.Rh = uh.S.Rh;
				uW.S.Gh = uh.S.Gh;
				uW.S.Bh = uh.S.Bh;
				src += 3;

				for (size_t len = 0; len < cp_len; len++) {
					ul.B = *src++;
					uW.S.Rl = ul.S.Rl;
					uW.S.Gl = ul.S.Gl;
					uW.S.Bl = ul.S.Bl;
					D16.push_back(uW.W);
				}
				break;

			case 0xFA: // 1行前の1ピクセル後の値をコピー
				D16.push_back(*(D16.end() - this->len_x + 1));
				src++;
				break;

			case 0xFB: // 1行前の1ピクセル前の値をコピー
				D16.push_back(*(D16.end() - this->len_x - 1));
				src++;
				break;

			case 0xFC: // 2-5バイト目の2ピクセルの値を1バイト目で指定した回数繰り返す。
				cp_len = *(src + 1) + 2LL;
				for (size_t len = 0; len < cp_len; len++) {
					u.B[0] = *(src + 2);
					u.B[1] = *(src + 3);
					D16.push_back(u.W);
					u.B[0] = *(src + 4);
					u.B[1] = *(src + 5);
					D16.push_back(u.W);
				}
				src += 6;
				break;
			case 0xFD: // 2,3バイト目の1ピクセルの値を1バイト目で指定した回数繰り返す。
				cp_len = *(src + 1) + 3LL;
				for (size_t len = 0; len < cp_len; len++) {
					u.B[0] = *(src + 2);
					u.B[1] = *(src + 3);
					D16.push_back(u.W);
				}
				src += 4;
				break;
			case 0xFE: // 2行前の値を1バイト目で指定した長さでコピー
				cp_len = *(src + 1) + 2LL;
				cp_src = dst - Col_per_Row * 2;
				wmemcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				break;
			case 0xFF: // 1行前の値を1バイト目で指定した長さでコピー
				cp_len = *(src + 1) + 2LL;
				cp_src = dst - Col_per_Row;
				wmemcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
				src += 2;
				break;

			default:
				u.B[0] = *src;
				u.B[1] = *(src + 1);
				D16.push_back(u.W);
				src += 2;
				break;
			}
		}

	}
};
#pragma pack(1)
#endif
