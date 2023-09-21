#include "tiffio.h"
#pragma pack(push)
#pragma pack(1)
class TIFFT {
	std::vector<unsigned __int8> I;
	std::vector<unsigned __int8> nI;

	struct TIFF_Palette {
		unsigned __int16 R[256];
		unsigned __int16 G[256];
		unsigned __int16 B[256];
	} Pal;

	const png_byte table2to8[4] = { 0, 0x7F, 0xBF, 0xFF };
	const png_byte table3to8[8] = { 0, 0x3F, 0x5F, 0x7F, 0x9F, 0xBF, 0xDF, 0xFF };

	unsigned __int16 Format;
	unsigned __int16 Samples_per_Pixel;

public:
	unsigned __int32 Rows;
	unsigned __int32 Cols;
	unsigned __int16 depth;

	bool init(wchar_t* infile)
	{
		TIFF* pTi = TIFFOpenW(infile, "r");

		if (pTi == NULL) {
			wprintf_s(L"File open error. %s\n", infile);
			TIFFClose(pTi);
			return true;
		}

		unsigned __int32 Rows_per_Strip;
		TIFFGetField(pTi, TIFFTAG_IMAGEWIDTH, &this->Cols);
		TIFFGetField(pTi, TIFFTAG_IMAGELENGTH, &this->Rows);
		TIFFGetField(pTi, TIFFTAG_BITSPERSAMPLE, &this->depth);
		TIFFGetField(pTi, TIFFTAG_SAMPLESPERPIXEL, &this->Samples_per_Pixel);
		TIFFGetFieldDefaulted(pTi, TIFFTAG_ROWSPERSTRIP, &Rows_per_Strip);
		TIFFGetField(pTi, TIFFTAG_PHOTOMETRIC, &this->Format);

		this->I.resize((size_t)this->Rows * this->Cols * Samples_per_Pixel);

		for (unsigned __int32 l = 0; l < this->Rows; l += Rows_per_Strip) {
			size_t read_rows = (l + Rows_per_Strip > this->Rows) ? this->Rows - l : Rows_per_Strip;
			if (-1 == TIFFReadEncodedStrip(pTi, TIFFComputeStrip(pTi, l, 0), &this->I[(size_t)this->Cols * Samples_per_Pixel * l], (size_t)this->Cols * read_rows * Samples_per_Pixel)) {
				fwprintf_s(stderr, L"File read error.\n");
				TIFFClose(pTi);
				return true;
			}
		}

		if (this->Format == PHOTOMETRIC_PALETTE) {
			unsigned __int16* PalR, * PalG, * PalB;
			TIFFGetField(pTi, TIFFTAG_COLORMAP, &PalR, &PalG, &PalB);
			memcpy_s(this->Pal.R, sizeof(this->Pal.R), PalR, sizeof(unsigned __int16) * (1LL << this->depth));
			memcpy_s(this->Pal.G, sizeof(this->Pal.G), PalG, sizeof(unsigned __int16) * (1LL << this->depth));
			memcpy_s(this->Pal.B, sizeof(this->Pal.B), PalB, sizeof(unsigned __int16) * (1LL << this->depth));
		}

		TIFFClose(pTi);
		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		// 実際にはこのブロックは意味を持たない。パレット持ちTIFFは64bit RGBA direct colorのPNGに変換するため。これはindexed color用コードの残骸であえて残す。
		// そもそもの原因はPNGが16bit階調のパレットの仕様を持たないため。
		if (this->Format == PHOTOMETRIC_PALETTE) {
			for (size_t i = 0; i < 256; i++) {
				png_color t;
				t.red = d16tod8(this->Pal.R[i]);
				t.green = d16tod8(this->Pal.G[i]);
				t.blue = d16tod8(this->Pal.B[i]);
				pal.push_back(t);
			}
		}
		// TOWNS TIFFはグレイスケールであるべきデータを8bitダイレクトカラーとして扱うため、対応する色を集めたパレットを作成して8bitインデックスカラーに修正する
		else if (this->Format == PHOTOMETRIC_MINISBLACK) {
			for (size_t i = 0; i < 256; i++) {
				union {
					unsigned __int8 a;
					struct color8 {
						unsigned __int8 B : 2;
						unsigned __int8 R : 3;
						unsigned __int8 G : 3;
					} c;
				} u;
				u.a = i;

				png_color t;
				t.red = table3to8[u.c.R];
				t.green = table3to8[u.c.G];
				t.blue = table2to8[u.c.B];
				pal.push_back(t);
			}
		}
		else {
			wprintf_s(L"Unexpected format.\n");
			exit(-1);
		}
	}

	bool decode_body(std::vector<png_bytep>& out_body)
	{
		// 16bit階調のパレットを持つTIFFは64bit RGBA direct colorのPNGに変換する。
		// このif分を恒真にすればindexed colorにできるがパレットの階調が16btiから8bitに下がる。
		if (this->Format == PHOTOMETRIC_MINISBLACK) {
			for (size_t j = 0; j < this->Rows; j++) {
				out_body.push_back((png_bytep)&I.at(j * this->Cols * this->Samples_per_Pixel));
			}
			return false;
		}
		else {
			union {
				unsigned __int8 B[8];
				struct {
					unsigned __int16 R;
					unsigned __int16 G;
					unsigned __int16 B;
					unsigned __int16 A;
				} P;
			} u;

			for (auto& i : I) {
				u.P.R = this->Pal.R[i];
				u.P.G = this->Pal.G[i];
				u.P.B = this->Pal.B[i];
				u.P.A = (1U << 16) - 1;

				nI.push_back(u.B[1]);
				nI.push_back(u.B[0]);
				nI.push_back(u.B[3]);
				nI.push_back(u.B[2]);
				nI.push_back(u.B[5]);
				nI.push_back(u.B[4]);
				nI.push_back(u.B[7]);
				nI.push_back(u.B[6]);
			}

			for (size_t j = 0; j < this->Rows; j++) {
				out_body.push_back((png_bytep)&nI.at(j * this->Cols * sizeof(u)));
			}

			this->depth = 16;
			return true;
		}
	}
};
#pragma pack(pop)
