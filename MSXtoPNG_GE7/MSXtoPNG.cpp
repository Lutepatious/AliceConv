#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#pragma pack(push)
#pragma pack(1)

#include "MSXtoPNG.hpp"
#include "GE7.hpp"
#include "LP.hpp"
#include "LV.hpp"
#include "GS.hpp"
#include "GL.hpp"

class Intr {
	std::vector<unsigned __int8> I;
	std::vector<unsigned __int8> FI;
	struct format_Intr {
		unsigned __int8 len_hx; // divided by 2
		unsigned __int8 len_y;
		unsigned __int8 body[];
	} *buf = nullptr;
	size_t len_buf = 0;
	size_t color_num = 0;

	struct Pal {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} *pal = nullptr;

public:
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	int transparent = -1;

	bool init(std::vector<__int8>& buffer, std::vector<__int8>& palette_buffer, size_t num)
	{
		if (palette_buffer.size() != 3200) {
			return true;
		}

		pal = (Pal*)&palette_buffer.at(0);

		this->buf = (format_Intr*)&buffer.at(0);
		this->len_buf = buffer.size();

		this->len_x = this->buf->len_hx ? (size_t)this->buf->len_hx * 2 : MSX_SCREEN7_H;
		this->len_y = this->buf->len_y;

		if (this->len_y > MSX_SCREEN7_V) {
			return true;
		}
	}
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
			if (*(*argv + 1) == L'b') { // MSX BSAVE (GE7)
				dm = decode_mode::GE7;
			}
			else if (*(*argv + 1) == L'l') { // Little Princess
				dm = decode_mode::LP;
			}
			else if (*(*argv + 1) == L'v') { // Little Vampire
				dm = decode_mode::LV;
			}
			else if (*(*argv + 1) == L'g') { // Gakuen Senki
				dm = decode_mode::GS;
			}
			else if (*(*argv + 1) == L'r') { // Rance and other GL
				dm = decode_mode::GL;
			}
			else if (*(*argv + 1) == L'i') { // Intruder
				dm = decode_mode::Intr;
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

		MSXtoPNG out;
		GE7 ge7;
		LP lp;
		LV lv;
		GS gs;
		GL gl;
		Intr intr;

		switch (dm) {
		case decode_mode::GE7:
			if (ge7.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			ge7.decode_palette(out.palette);
			ge7.decode_body(out.body);
			break;

		case decode_mode::LP:
			if (lp.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			lp.decode_palette(out.palette);
			lp.decode_body(out.body);
			out.set_size(lp.len_x, lp.len_y);
			break;

		case decode_mode::LV:
			if (lv.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			lv.decode_palette(out.palette);
			lv.decode_body(out.body);
			out.set_size(lv.len_x, lv.len_y);
			break;

		case decode_mode::GS:
		{
			wchar_t* t;
			size_t n = wcstoull(*argv, &t, 10);
			if (n == 0 || n > 300) {
				std::wcerr << L"Out of range." << std::endl;

				continue;
			}

			std::ifstream palettefile("RXX", std::ios::binary);
			if (!palettefile) {
				std::wcerr << L"File " << "RXX" << L" open error." << std::endl;

				continue;
			}

			std::vector<__int8> palbuf{ std::istreambuf_iterator<__int8>(palettefile), std::istreambuf_iterator<__int8>() };

			palettefile.close();

			if (gs.init(inbuf, palbuf, n)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}

			gs.decode_palette(out.palette, out.trans);
			gs.decode_body(out.body);
			out.set_size(MSX_SCREEN7_H, MSX_SCREEN7_V);
			break;
		}
		case decode_mode::GL:
			if (gl.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			gl.decode_palette(out.palette, out.trans);
			gl.decode_body(out.body);
			out.set_size(MSX_SCREEN7_H, MSX_SCREEN7_V);
			// Almost all viewer not support png offset feature 
			// out.set_offset(gl.offset_x, gl.offset_y);
			break;
		case decode_mode::Intr:
		{
			wchar_t* t;
			size_t n = wcstoull(*argv, &t, 10);
			if (n == 0 || n > 99) {
				std::wcerr << L"Out of range." << std::endl;

				continue;
			}

			std::ifstream palettefile("COLPALET.DAT", std::ios::binary);
			if (!palettefile) {
				std::wcerr << L"File " << "COLPALET.DAT" << L" open error." << std::endl;

				continue;
			}

			std::vector<__int8> palbuf{ std::istreambuf_iterator<__int8>(palettefile), std::istreambuf_iterator<__int8>() };

			palettefile.close();

			if (intr.init(inbuf, palbuf, n)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}

//			intr.decode_body(out.body);
//			out.set_size(MSX_SCREEN7_H, MSX_SCREEN7_V);
			break;
		}

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
}