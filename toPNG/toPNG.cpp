#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <bitset>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "zlib.h"
#include "png.h"
#include "tiffio.h"

static inline png_byte d4tod8(png_byte a)
{
	//	png_byte r = (double) (a) * 255.0L / 15.0L + 0.5L;
	png_byte r = (unsigned)a * 17;
	return r;
}

static inline png_byte d5tod8(png_byte a)
{
	//	png_byte r = (double) (a) * 255.0L / 31.0L + 0.5L;
	png_byte r = ((unsigned)a * 527 + 23) >> 6;
	return r;
}

static inline png_byte d16tod8(png_uint_16 a)
{
	//	png_byte r = ((double) a) * 255.0L / 65535.0L + 0.5L;
	png_byte r = ((unsigned)a * 255L + 32895L) >> 16;
	return  r;
}


#pragma pack(push)
#pragma pack(1)

constexpr size_t VGA_V = 480;
constexpr size_t VGA_H = 640;
constexpr size_t PC9801_V = 400;
constexpr size_t PC9801_H = 640;
constexpr size_t RES = 40000;

struct Palette_depth4 {
	unsigned __int8 B : 4;
	unsigned __int8 : 4;
	unsigned __int8 R : 4;
	unsigned __int8 : 4;
	unsigned __int8 G : 4;
	unsigned __int8 : 4;
};

struct Palette_depth5 {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
};

struct toPNG {
	std::vector<png_color> palette;
	std::vector<png_byte> trans;
	std::vector<png_bytep> body;

	png_uint_32 pixels_V = PC9801_V;
	png_uint_32 pixels_H = PC9801_H;
	int depth = 8;
	bool indexed = true;
	int res_x = RES;
	int res_y = RES;
	unsigned background = 0;

	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	bool enable_offset = false;

	void set_size_and_change_resolution(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;

		res_x = ((RES * in_x * 2) / PC9801_H + 1) >> 1;
		res_y = ((RES * in_y * 2) / PC9801_V + 1) >> 1;
	}

	void set_size(png_uint_32 in_x, png_uint_32 in_y)
	{
		this->pixels_V = in_y;
		this->pixels_H = in_x;
	}

	void set_offset(png_uint_32 offs_x, png_uint_32 offs_y)
	{
		this->offset_x = offs_x;
		this->offset_y = offs_y;
		this->enable_offset = true;
	}

	void set_depth(int in_depth)
	{
		this->depth = in_depth;
	}

	void set_directcolor(void)
	{
		this->indexed = false;
	}

	int create(wchar_t* outfile)
	{
		if (body.size()) {

			FILE* pFo;
			errno_t ecode = _wfopen_s(&pFo, outfile, L"wb");
			if (ecode || !pFo) {
				std::wcerr << L"File open error." << outfile << std::endl;
				return -1;
			}
			png_structp png_ptr = NULL;
			png_infop info_ptr = NULL;

			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (png_ptr == NULL) {
				fclose(pFo);
				return -1;
			}

			info_ptr = png_create_info_struct(png_ptr);
			if (info_ptr == NULL) {
				png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
				fclose(pFo);
				return -1;
			}

			png_init_io(png_ptr, pFo);
			png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
			png_set_IHDR(png_ptr, info_ptr, this->pixels_H, this->pixels_V, this->depth, !this->indexed ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

			png_set_pHYs(png_ptr, info_ptr, this->res_x, this->res_y, PNG_RESOLUTION_METER);

			if (this->indexed) {
				png_set_PLTE(png_ptr, info_ptr, &this->palette.at(0), this->palette.size());

				if (!this->trans.empty()) {
					png_set_tRNS(png_ptr, info_ptr, &this->trans.at(0), this->trans.size(), NULL);
				}
			}

			if (this->enable_offset) {
				png_color_16 t;
				t.index = background;
				png_set_bKGD(png_ptr, info_ptr, &t);
				png_set_oFFs(png_ptr, info_ptr, this->offset_x, this->offset_y, PNG_OFFSET_PIXEL);
			}

			png_write_info(png_ptr, info_ptr);
			png_write_image(png_ptr, &this->body.at(0));
			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(pFo);

			return 0;
		}
		else {
			return -1;
		}
	}
};

class DRS003 {
	std::vector<unsigned __int8> I;

	struct format_DRS003 {
		struct Palette_depth4 Pal4[16];
		unsigned __int8 body[PC9801_H][4][PC9801_V / 8];
	} *buf = nullptr;

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_DRS003)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_DRS003*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->buf->Pal4[i].R);
			c.green = d4tod8(this->buf->Pal4[i].G);
			c.blue = d4tod8(this->buf->Pal4[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		for (size_t y = 0; y < PC9801_H; y++) {
			for (size_t x = 0; x < PC9801_V / 8; x++) {
				std::bitset<8> b[4] = { this->buf->body[y][0][x], this->buf->body[y][1][x], this->buf->body[y][2][x], this->buf->body[y][3][x] };

				for (size_t i = 0; i < 8; i++) {
					unsigned __int8 a = (b[0][7 - i] ? 1 : 0) | (b[1][7 - i] ? 2 : 0) | (b[2][7 - i] ? 4 : 0) | (b[3][7 - i] ? 8 : 0);
					I.push_back(a);
				}
			}
		}

		for (size_t j = 0; j < PC9801_H; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_V));
		}
	}
};

class DRS003T {
	std::vector<unsigned __int8> I;

	struct format_DRS003T {
		unsigned __int8 body[PC9801_H][PC9801_V / 2];
	} *buf = nullptr;

	const struct Palette_depth4 Pal4[16] = { {0x0, 0x0, 0x0}, {0x0, 0x0, 0x0}, {0xF, 0xF, 0xF}, {0xA, 0xA, 0xA},
											{0x6, 0x6, 0x6}, {0x1, 0xD, 0x0}, {0x4, 0x0, 0x7}, {0x2, 0x0, 0x3},
											{0xD, 0xD, 0xD}, {0xC, 0xF, 0xD}, {0x8, 0xF, 0xA}, {0x5, 0xD, 0x6},
											{0x1, 0x6, 0x2}, {0xD, 0xF, 0xB}, {0xA, 0xF, 0x7}, {0xE, 0xE, 0xE} };

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_DRS003T)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_DRS003T*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->Pal4[i].R);
			c.green = d4tod8(this->Pal4[i].G);
			c.blue = d4tod8(this->Pal4[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		for (size_t y = 0; y < PC9801_H; y++) {
			for (size_t x = 0; x < PC9801_V / 2; x++) {
				union {
					unsigned __int8 A;
					struct {
						unsigned __int8 L : 4;
						unsigned __int8 H : 4;
					} B;
				} u;
				u.A = this->buf->body[y][x];

				I.push_back(u.B.L);
				I.push_back(u.B.H);
			}
		}

		for (size_t j = 0; j < PC9801_H; j++) {
			out_body.push_back((png_bytep)&I.at(j * PC9801_V));
		}
	}
};

class DRSOPNT {
	std::vector<unsigned __int8> I;

	struct format_DRSOPNT {
		unsigned __int8 body[VGA_V][VGA_H / 2];
	} *buf = nullptr;

	const struct Palette_depth4 Pal4[16] = { {0x0, 0x0, 0x0}, {0x0, 0x0, 0x0}, {0xF, 0xF, 0xF}, {0xA, 0xA, 0xA},
											{0x6, 0x6, 0x6}, {0x1, 0xD, 0x0}, {0x4, 0x0, 0x7}, {0x2, 0x0, 0x3},
											{0xD, 0xD, 0xD}, {0xC, 0xF, 0xD}, {0x8, 0xF, 0xA}, {0x5, 0xD, 0x6},
											{0x1, 0x6, 0x2}, {0xD, 0xF, 0xB}, {0xA, 0xF, 0x7}, {0xE, 0xE, 0xE} };

public:
	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_DRSOPNT)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_DRSOPNT*)&buffer.at(0);

		return false;
	}

	void decode_palette(std::vector<png_color>& pal)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d4tod8(this->Pal4[i].R);
			c.green = d4tod8(this->Pal4[i].G);
			c.blue = d4tod8(this->Pal4[i].B);
			pal.push_back(c);
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		for (size_t y = 0; y < VGA_V; y++) {
			for (size_t x = 0; x < VGA_H / 2; x++) {
				union {
					unsigned __int8 A;
					struct {
						unsigned __int8 L : 4;
						unsigned __int8 H : 4;
					} B;
				} u;
				u.A = this->buf->body[y][x];

				I.push_back(u.B.L);
				I.push_back(u.B.H);
			}
		}

		for (size_t j = 0; j < VGA_V; j++) {
			out_body.push_back((png_bytep)&I.at(j * VGA_H));
		}
	}
};


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

class SPRITE {
	std::vector<unsigned __int8> I;

public:
	struct format_SPRITE {
		unsigned __int16 Pal16BE[16]; // X68000 big endian、バイトスワップの上で色分解する事。
		struct {
			unsigned __int8 H: 4;
			unsigned __int8 L: 4;
		} S[128][2][16][4];
	}  *buf = nullptr;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_SPRITE)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_SPRITE*)&buffer.at(0);

		return false;
	}



	void decode_palette(std::vector<png_color>& pal)
	{
		union X68Pal_conv {
			struct Palette_depth5 Pal5;
			unsigned __int16 Pin;
		} P;

		png_color c;
		for (size_t i = 0; i < 16; i++) {
			P.Pin = _byteswap_ushort(buf->Pal16BE[i]);

			c.red = d5tod8(P.Pal5.R);
			c.green = d5tod8(P.Pal5.G);
			c.blue = d5tod8(P.Pal5.B);
			pal.push_back(c);
		}
	}

};


enum class decode_mode {
	NONE = 0, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS8, PMS16, QNT, X68R, X68T, X68V, TIFF_TOWNS, DRS_CG003, DRS_CG003_TOWNS, DRS_OPENING_TOWNS, SPRITE_X68K, SPRITE_X68K_A
};

#pragma pack(pop)

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	enum class decode_mode dm = decode_mode::NONE;

	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L's') { // Dr.STOP!
				dm = decode_mode::DRS_CG003;
			}
			else if (*(*argv + 1) == L'S') { // Dr.STOP! FM TOWNS
				dm = decode_mode::DRS_CG003_TOWNS;
			}
			else if (*(*argv + 1) == L'O') { // Dr.STOP! OPENING FM TOWNS
				dm = decode_mode::DRS_OPENING_TOWNS;
			}
			else if (*(*argv + 1) == L'Y') { // ALICEの館CD他TIFF FM TOWNS
				dm = decode_mode::TIFF_TOWNS;
			}
			continue;
		}

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();


		toPNG out;
		DRS003 drs;
		DRS003T drst;
		DRSOPNT drsot;
		TIFFT tifft;

		switch (dm) {
		case decode_mode::DRS_CG003:
			if (drs.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			drs.decode_palette(out.palette);
			drs.decode_body(out.body);
			out.set_size(PC9801_V, PC9801_H);
			break;

		case decode_mode::DRS_CG003_TOWNS:
			if (drst.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			drst.decode_palette(out.palette);
			drst.decode_body(out.body);
			out.set_size(PC9801_V, PC9801_H);
			break;

		case decode_mode::DRS_OPENING_TOWNS:
			if (drsot.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			drsot.decode_palette(out.palette);
			drsot.decode_body(out.body);
			out.set_size(PC9801_H, VGA_V);
			break;

		case decode_mode::TIFF_TOWNS:
			if (tifft.init(*argv)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			tifft.decode_palette(out.palette);
			if (tifft.decode_body(out.body)) {
				out.set_directcolor();
			}
			out.set_depth(tifft.depth);
			out.set_size(tifft.Cols, tifft.Rows);
			break;

		default:
			break;
		}

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		int result = out.create(path);
		if (result) {
			std::wcerr << L"output failed." << std::endl;
		}
	}

	return 0;
}
