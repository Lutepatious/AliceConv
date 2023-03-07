#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "MSXtoPNG.hpp"

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
			else if (*(*argv + 1) == L's') { // D.P.S -SG-
				dm = decode_mode::SG;
			}
			else if (*(*argv + 1) == L't') { // Toushin Toshi
				dm = decode_mode::TT;
			}
			else if (*(*argv + 1) == L'd') { // Dr.STOP!
				dm = decode_mode::DRS;
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
		Dual_GL sg;
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
			wchar_t fname[_MAX_FNAME];
			_wsplitpath_s(*argv, NULL, 0, NULL, 0, fname, _MAX_FNAME, NULL, 0);
			size_t n = wcstoull(fname, &t, 10);

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
			wchar_t fname[_MAX_FNAME];
			_wsplitpath_s(*argv, NULL, 0, NULL, 0, fname, _MAX_FNAME, NULL, 0);
			size_t n = wcstoull(fname, &t, 10);

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
		case decode_mode::SG:
		{
			wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];

			_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);

			if (wcslen(fname) != 8 || !iswdigit(fname[0]) || !iswdigit(fname[1]) || !iswdigit(fname[2]) || !iswdigit(fname[3]) ||
				!iswdigit(fname[5]) || !iswdigit(fname[6]) || !iswdigit(fname[7]) || !iswalpha(fname[4])) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}

			unsigned image_number, archive_entry;
			wchar_t archive_volume;
			bool odd;

			swscanf_s(fname, L"%4u%c%3u", &image_number, &archive_volume, 1, &archive_entry);

			if ((image_number & 1) != (archive_entry & 1)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
			}

			if (image_number & 1) {
				odd = true;
			}
			else {
				odd = false;
			}

			if (odd) {
				image_number++;
				archive_entry++;
			}
			else {
				image_number--;
				archive_entry--;
			}

			wchar_t mate_filename[_MAX_FNAME], mate_file[_MAX_PATH];
			swprintf_s(mate_filename, _MAX_FNAME, L"%04u%c%03u", image_number, archive_volume, archive_entry);
			_wmakepath_s(mate_file, _MAX_PATH, drive, dir, mate_filename, ext);

			std::ifstream infile2(mate_file, std::ios::binary);
			if (!infile2) {
				std::wcerr << L"File " << mate_file << L" open error." << std::endl;
				continue;
			}

			std::vector<__int8> inbuf2{ std::istreambuf_iterator<__int8>(infile2), std::istreambuf_iterator<__int8>() };

			infile2.close();

			if (odd) {
				if (sg.init(inbuf, inbuf2)) {
					std::wcerr << L"Wrong file. " << *argv << std::endl;
					continue;
				}
			}
			else {
				if (sg.init(inbuf2, inbuf)) {
					std::wcerr << L"Wrong file. " << *argv << std::endl;
					continue;
				}
			}

			sg.decode_palette(out.palette, out.trans);
			sg.decode_body(out.body);
			out.set_size_and_change_resolution(MSX_SCREEN7_H, MSX_SCREEN7_V * 2);
			break;
		}
		case decode_mode::TT:
			if (td.init(inbuf, false)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			td.decode_palette(out.palette, out.trans);
			if (td.decode_body(out.body)) {
				out.set_size_and_change_resolution(MSX_SCREEN7_H, MSX_SCREEN7_V * 2);
			}
			else {
				out.set_size(MSX_SCREEN7_H, MSX_SCREEN7_V);
			}
			// Almost all viewer not support png offset feature 
			// out.set_offset(gl.offset_x, gl.offset_y);
			break;

		case decode_mode::DRS:
			if (td.init(inbuf, true)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			td.decode_palette(out.palette, out.trans);
			if (td.decode_body(out.body)) {
				out.set_size_and_change_resolution(MSX_SCREEN7_H, MSX_SCREEN7_V * 2);
			}
			else {
				out.set_size(MSX_SCREEN7_H, MSX_SCREEN7_V);
			}
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