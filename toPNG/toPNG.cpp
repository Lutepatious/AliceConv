#include "GL.hpp"
#include "GL3.hpp"
#include "GL_X68K.hpp"

#include "DRS003.hpp"
#include "TIFF_TOWNS.hpp"
#include "X68K.hpp"

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

public:
	size_t len_col = 0;
	png_uint_32 len_x = PC9801_H;
	png_uint_32 len_y = PC9801_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_VSP)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		unsigned __int8 t = 0;
		for (int i = 0x0A; i < 0x3A; i++) {
			t |= buffer.at(i);
		}

		if (t >= 0x10) {
			std::wcerr << "Wrong palette." << std::endl;
			return true;
		}

		this->buf = (format_VSP*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (this->buf->end_x8 <= this->buf->start_x8 || this->buf->end_y <= this->buf->start_y) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		this->len_col = this->buf->end_x8 - this->buf->start_x8;
		this->len_x = 8 * (this->buf->end_x8 - this->buf->start_x8);
		this->len_y = this->buf->end_y - this->buf->start_y;
		this->offset_x = 8 * this->buf->start_x8;
		this->offset_y = this->buf->start_y;

		if ((8ULL * this->buf->end_x8 > PC9801_H) || ((this->buf->end_x8 > PC9801_V))) {
			std::wcerr << "Wrong size." << std::endl;
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
};
#pragma pack(pop)

enum class decode_mode {
	NONE = 0, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS8, PMS16, QNT, X68R, X68T, X68V, TIFF_TOWNS, DRS_CG003, DRS_CG003_TOWNS, DRS_OPENING_TOWNS, SPRITE_X68K, MASK_X68K
};

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
			// already used: sSOYPMghRv

			if (*(*argv + 1) == L's') { // Dr.STOP! CG003
				dm = decode_mode::DRS_CG003;
			}
			else if (*(*argv + 1) == L'S') { // Dr.STOP! FM TOWNS CG003
				dm = decode_mode::DRS_CG003_TOWNS;
			}
			else if (*(*argv + 1) == L'O') { // Dr.STOP! FM TOWNS OPENING
				dm = decode_mode::DRS_OPENING_TOWNS;
			}
			else if (*(*argv + 1) == L'Y') { // ALICEの館CD他TIFF FM TOWNS
				dm = decode_mode::TIFF_TOWNS;
			}
			else if (*(*argv + 1) == L'P') { // 闘神都市 X68000 PCG
				dm = decode_mode::SPRITE_X68K;
			}
			else if (*(*argv + 1) == L'M') { // 闘神都市 X68000 Attack effect mask
				dm = decode_mode::MASK_X68K;
			}
			else if (*(*argv + 1) == L'g') { // GL 
				dm = decode_mode::GL;
			}
			else if (*(*argv + 1) == L'h') { // GL3, GM3
				dm = decode_mode::GL3;
			}
			else if (*(*argv + 1) == L'R') { // GL_X68K RanceII
				dm = decode_mode::X68R;
			}
			else if (*(*argv + 1) == L'v') { // VSP
				dm = decode_mode::VSP;
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
		SPRITE spr;
		MASK mask;
		GL gl;
		GL3 gl3;
		GL_X68K glx68k;
		VSP vsp;

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
			out.set_size(VGA_H, VGA_V);
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

		case decode_mode::SPRITE_X68K:
			if (spr.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			spr.decode_palette(out.palette, out.trans);
			spr.decode_body(out.body);
			out.set_size(16 * 16, 16 * 128 / 16);
			break;

		case decode_mode::MASK_X68K:
			if (mask.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			mask.decode_palette(out.palette, out.trans);
			mask.decode_body(out.body);
			out.set_size(5 * 8 * 14, 24 * 70 / 14);
			break;

		case decode_mode::GL:
			if (gl.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			gl.decode_palette(out.palette, out.trans);
			gl.decode_body(out.body);
			out.set_size_and_change_resolution(PC8801_H, PC8801_V);
			break;

		case decode_mode::GL3:
			if (gl3.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			gl3.decode_palette(out.palette, out.trans);
			if (gl3.decode_body(out.body)) {
				out.change_resolution_halfy();
			}
			out.set_size(PC9801_H, gl3.disp_y);
			break;

		case decode_mode::X68R:
			if (glx68k.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			glx68k.decode_palette(out.palette, out.trans);
			glx68k.decode_body(out.body);
			out.set_size(PC9801_H, PC9801_V);
			break;

		case decode_mode::VSP:
			if (vsp.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			vsp.decode_palette(out.palette, out.trans);
//			vsp.decode_body(out.body);
			out.set_size(PC9801_H, PC9801_V);
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
