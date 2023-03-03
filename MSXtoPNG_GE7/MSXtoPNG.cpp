#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "MSXtoPNG.hpp"
#include "GE7.hpp"
#include "LP.hpp"
#include "LV.hpp"
#include "GS.hpp"
#include "GL.hpp"
#include "Intr.hpp"

#pragma pack(push)
#pragma pack(1)

// MSX版闘神都市のフォーマット
class TT_DRS {
	std::vector<unsigned __int8> I;
	std::vector<unsigned __int8> FI;

	struct format_TT_DRS {
		unsigned __int16 offs_hx; // divided by 2
		unsigned __int16 offs_y;
		unsigned __int16 len_hx; // divided by 2
		unsigned __int16 len_y;
		MSX_Pal palette[16];
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;

public:
	int transparent = -1;
	png_uint_32 len_x = MSX_SCREEN7_H;
	png_uint_32 len_y = MSX_SCREEN7_V;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;
	bool is_DRS = false;

	bool init(std::vector<__int8>& buffer, bool flag_DRS)
	{
		this->is_DRS = flag_DRS;
		this->buf = (format_TT_DRS*)&buffer.at(0);
		this->len_buf = buffer.size();

		this->len_x = this->buf->len_hx * 2;
		this->len_y = this->buf->len_y;

		this->offset_x = this->buf->offs_hx * 2;
		this->offset_y = this->buf->offs_y;

		if (len_x > MSX_SCREEN7_H || len_y > MSX_SCREEN7_V) {
			return true;
		}

		std::wcout << std::setw(3) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" ("
			<< std::setw(3) << this->len_x << L"*" << std::setw(3) << this->len_y << L") transparent " << std::setw(2) << this->transparent << std::endl;

		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		png_color c;
		for (size_t i = 0; i < 16; i++) {
			c.red = d3tod8(this->buf->palette[i].R);
			c.green = d3tod8(this->buf->palette[i].G);
			c.blue = d3tod8(this->buf->palette[i].B);
			pal.push_back(c);

			trans.push_back(0xFF);
		}

		if (this->transparent == -1) {
			c.red = 0;
			c.green = 0;
			c.blue = 0;
			pal.push_back(c);
			this->transparent = trans.size();
			trans.push_back(0);
		}
		else {
			trans.at(this->transparent) = 0;
		}
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_TT_DRS);
		size_t cp_len;
		uPackedPixel4 u;

		while (count-- && I.size() < image_size) {
			switch (*src) {


#if 0
			case 0x00:
				cp_len = *(src + 1);
				if (cp_len == 0) {
					break;
				}
				memcpy_s(dst, cp_len, dst - len_col * 2, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0x01:
				cp_len = *(src + 1);
				memcpy_s(dst, cp_len, dst - len_col, cp_len);
				dst += cp_len;
				src += 2;
				count--;
				break;
			case 0x02:
				cp_len = *(src + 1);
				memset(dst, *(src + 2), cp_len);
				dst += cp_len;
				src += 3;
				count -= 2;
				break;
#endif
			case 0x03:
				u.B = *++src;

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
				break;

			default:
				u.B = *src;

				I.push_back(u.S.H);
				I.push_back(u.S.L);
				src++;
			}
		}

#if 0
		for (size_t j = 0; j < this->len_y; j++) {
			out_body.push_back((png_bytep)&I.at(j * this->len_x));
		}

#else
		FI.assign(MSX_SCREEN7_H * MSX_SCREEN7_V, this->transparent);
		for (size_t j = 0; j < this->len_y; j++) {
			// Almost all viewer not support png offset feature
			// memcpy_s(&FI[(j) * MSX_SCREEN7_H], this->len_x, &I[j * this->len_x], this->len_x);
			memcpy_s(&FI[(this->offset_y + j) * MSX_SCREEN7_H + this->offset_x], this->len_x, &I[j * this->len_x], this->len_x);
		}

		for (size_t j = 0; j < MSX_SCREEN7_V; j++) {
			out_body.push_back((png_bytep)&FI.at(j * MSX_SCREEN7_H));
		}
#endif
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
			else if (*(*argv + 1) == L't') { // Toushin Toshi
				dm = decode_mode::TT;
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
		TT_DRS td;

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

			intr.decode_palette(out.palette);
			intr.decode_body(out.body);
			out.set_size(intr.len_x, intr.len_y);
			break;
		}
		case decode_mode::TT:
			if (td.init(inbuf, false)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			td.decode_palette(out.palette, out.trans);
			//			td.decode_body(out.body);
			//			out.set_size(MSX_SCREEN7_H, MSX_SCREEN7_V);
						// Almost all viewer not support png offset feature 
						// out.set_offset(gl.offset_x, gl.offset_y);
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
}