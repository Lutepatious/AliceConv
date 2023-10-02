#include "toPNG.hpp"
#pragma pack(push)
#pragma pack(1)
#pragma pack(pop)

enum class decode_mode {
	NONE = 0, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS8, PMS16, QNT, X68R, X68T, X68B, TIFF_TOWNS, DRS_CG003, DRS_CG003_TOWNS, DRS_OPENING_TOWNS, SPRITE_X68K, MASK_X68K, AUTO, DAT
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
		::silent = false;
		if (**++argv == L'-') {
			// already used: sSOYPMghRrvTBpkiad

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
			else if (*(*argv + 1) == L'r') { // VSP 200lines
				dm = decode_mode::VSP200l;
			}
			else if (*(*argv + 1) == L'v') { // VSP
				dm = decode_mode::VSP;
			}
			else if (*(*argv + 1) == L'T') { // 闘神都市 X68000 256色
				dm = decode_mode::X68T;
			}
			else if (*(*argv + 1) == L'B') { // RanceIII オプションセット あぶない文化祭前夜 X68000 256色
				dm = decode_mode::X68B;
			}
			else if (*(*argv + 1) == L'p') { // 旧PMS(VSP256)
				dm = decode_mode::VSP256;
			}
			else if (*(*argv + 1) == L'k') { // PMS8 鬼畜王ランス以降
				dm = decode_mode::PMS8;
			}
			else if (*(*argv + 1) == L'i') { // PMS16 いけないかつみ先生
				dm = decode_mode::PMS16;
			}
			else if (*(*argv + 1) == L'a') { // auto detect ghRrvTBpki 
				dm = decode_mode::AUTO;
			}
			else if (*(*argv + 1) == L'd') { // archive DAT file 
				dm = decode_mode::DAT;
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

		DAT dat;

		DRS003 drs;
		DRS003T drst;
		DRSOPNT drsot;
		TIFFT tifft;
		SPRITE spr;
		MASK mask;
		GL gl;
		GL3 gl3;
		GL_X68K glx68k;
		VSP200l vsp200l;

		VSP vsp;
		X68K_TT x68t;
		X68K_ABZ x68b;

		PMS pms;
		PMS16 pms16;

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

		case decode_mode::VSP200l:
			if (vsp200l.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			vsp200l.decode_palette(out.palette, out.trans);
			vsp200l.decode_body(out.body);
			out.set_size_and_change_resolution(PC8801_H, PC8801_V);
			break;

		case decode_mode::VSP:
			if (vsp.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			vsp.decode_palette(out.palette, out.trans);
			vsp.decode_body(out.body);
			out.set_size(PC9801_H, PC9801_V);
			break;

		case decode_mode::X68T:
			if (x68t.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			x68t.decode_palette(out.palette, out.trans);
			x68t.decode_body(out.body);
			out.change_resolution_x68k_g();
			out.set_size(X68000_G, X68000_G);
			break;

		case decode_mode::X68B:
			if (x68b.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			x68b.decode_palette(out.palette, out.trans);
			x68b.decode_body(out.body);
			out.set_size(x68b.disp_x, x68b.disp_y);
			break;

		case decode_mode::VSP256:
			if (pms.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			pms.decode_palette(out.palette, out.trans);
			pms.decode_body(out.body);
			out.set_size(pms.disp_x, pms.disp_y);
			break;

		case decode_mode::PMS8:
			if (pms.init8(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			pms.decode_palette(out.palette, out.trans);
			pms.decode_body(out.body);
			out.set_size(pms.disp_x, pms.disp_y);
			break;

		case decode_mode::PMS16:
			if (pms16.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			pms16.decode_body(out.body);
			out.set_directcolor();
			out.set_size(pms16.disp_x, pms16.disp_y);
			break;

		case decode_mode::DAT:
			if (dat.init(inbuf)) {
				std::wcerr << L"Wrong file. " << *argv << std::endl;
				continue;
			}
			for (auto& a : dat.files) {
//				std::wcout << a << L":" << std::hex << std::setw(6) << dat.offsets.at(a) << L":" << std::hex << std::setw(6) << dat.lengths.at(a) << std::endl;
				std::vector<__int8> cbuf(inbuf.begin() + dat.offsets.at(a), inbuf.begin() + dat.offsets.at(a) + dat.lengths.at(a));
//				std::wcout << cbuf.size() << std::endl;
				::silent = true;

				toPNG cout;
				GL3 cgl3;
				VSP cvsp;
				if (!cgl3.init(cbuf)) {
					cgl3.decode_palette(cout.palette, cout.trans);
					if (cgl3.decode_body(cout.body)) {
						cout.change_resolution_halfy();
					}
					cout.set_size(PC9801_H, cgl3.disp_y);
				}
				else if (!cvsp.init(cbuf)) {
					cvsp.decode_palette(cout.palette, cout.trans);
					cvsp.decode_body(cout.body);
					cout.set_size(PC9801_H, PC9801_V);
				}

				wchar_t cpath[_MAX_PATH];
				wchar_t cfname[_MAX_FNAME];
				wchar_t cfname_new[_MAX_FNAME];
				wchar_t cdir[_MAX_DIR];
				wchar_t cdrive[_MAX_DRIVE];

				_wsplitpath_s(*argv, cdrive, _MAX_DRIVE, cdir, _MAX_DIR, cfname, _MAX_FNAME, NULL, 0);
				swprintf_s(cfname_new, _MAX_FNAME, L"%s_%03zu", cfname, a);
				_wmakepath_s(cpath, _MAX_PATH, cdrive, cdir, cfname_new, L".png");

				int result = cout.create(cpath);
				if (result) {
					std::wcerr << L"output failed." << std::endl;
				}
			}
			continue;

		case decode_mode::AUTO:
			::silent = true;
			if (!dat.init(inbuf)) {
				for (auto& a : dat.files) {
					//				std::wcout << a << L":" << std::hex << std::setw(6) << dat.offsets.at(a) << L":" << std::hex << std::setw(6) << dat.lengths.at(a) << std::endl;
					std::vector<__int8> cbuf(inbuf.begin() + dat.offsets.at(a), inbuf.begin() + dat.offsets.at(a) + dat.lengths.at(a));
					//				std::wcout << cbuf.size() << std::endl;
					::silent = true;

					toPNG cout;
					GL3 cgl3;
					VSP cvsp;
					if (!cgl3.init(cbuf)) {
						cgl3.decode_palette(cout.palette, cout.trans);
						if (cgl3.decode_body(cout.body)) {
							cout.change_resolution_halfy();
						}
						cout.set_size(PC9801_H, cgl3.disp_y);
					}
					else if (!cvsp.init(cbuf)) {
						cvsp.decode_palette(cout.palette, cout.trans);
						cvsp.decode_body(cout.body);
						cout.set_size(PC9801_H, PC9801_V);
					}

					wchar_t cpath[_MAX_PATH];
					wchar_t cfname[_MAX_FNAME];
					wchar_t cfname_new[_MAX_FNAME];
					wchar_t cdir[_MAX_DIR];
					wchar_t cdrive[_MAX_DRIVE];

					_wsplitpath_s(*argv, cdrive, _MAX_DRIVE, cdir, _MAX_DIR, cfname, _MAX_FNAME, NULL, 0);
					swprintf_s(cfname_new, _MAX_FNAME, L"%s_%03zu", cfname, a);
					_wmakepath_s(cpath, _MAX_PATH, cdrive, cdir, cfname_new, L".png");

					int result = cout.create(cpath);
					if (result) {
						std::wcerr << L"output failed." << std::endl;
					}
				}
				continue;
			}
			else if (!gl.init(inbuf)) {
				gl.decode_palette(out.palette, out.trans);
				gl.decode_body(out.body);
				out.set_size_and_change_resolution(PC8801_H, PC8801_V);
			}
			else if (!gl3.init(inbuf)) {
				gl3.decode_palette(out.palette, out.trans);
				if (gl3.decode_body(out.body)) {
					out.change_resolution_halfy();
				}
				out.set_size(PC9801_H, gl3.disp_y);
			}
			else if (!glx68k.init(inbuf)) {
				glx68k.decode_palette(out.palette, out.trans);
				glx68k.decode_body(out.body);
				out.set_size(PC9801_H, PC9801_V);
			}
			else if (!vsp200l.init(inbuf)) {
				vsp200l.decode_palette(out.palette, out.trans);
				vsp200l.decode_body(out.body);
				out.set_size_and_change_resolution(PC8801_H, PC8801_V);
			}
			else if (!vsp.init(inbuf)) {
				vsp.decode_palette(out.palette, out.trans);
				vsp.decode_body(out.body);
				out.set_size(PC9801_H, PC9801_V);
			}
			else if (!x68t.init(inbuf)) {
				x68t.decode_palette(out.palette, out.trans);
				x68t.decode_body(out.body);
				out.change_resolution_x68k_g();
				out.set_size(X68000_G, X68000_G);
			}
			else if (!x68b.init(inbuf)) {
				x68b.decode_palette(out.palette, out.trans);
				x68b.decode_body(out.body);
				out.set_size(x68b.disp_x, x68b.disp_y);
			}
			else if (!pms.init(inbuf)) {
				pms.decode_palette(out.palette, out.trans);
				pms.decode_body(out.body);
				out.set_size(pms.disp_x, pms.disp_y);
			}
			else if (!pms.init8(inbuf)) {
				pms.decode_palette(out.palette, out.trans);
				pms.decode_body(out.body);
				out.set_size(pms.disp_x, pms.disp_y);
			}
			else if (!pms16.init(inbuf)) {
				pms16.decode_body(out.body);
				out.set_directcolor();
				out.set_size(pms16.disp_x, pms16.disp_y);
			}
			else {
				std::wcerr << L"Unknown or cannot autodetect. " << *argv << std::endl;
				wchar_t tpath[_MAX_PATH];
				wchar_t tfname[_MAX_FNAME];
				wchar_t tdir[_MAX_DIR];
				wchar_t tdrive[_MAX_DRIVE];

				_wsplitpath_s(*argv, tdrive, _MAX_DRIVE, tdir, _MAX_DIR, tfname, _MAX_FNAME, NULL, 0);
				_wmakepath_s(tpath, _MAX_PATH, tdrive, tdir, tfname, L".txt");
				_wsetlocale(LC_ALL, L"ja_JP");

				size_t len_t = strlen(&inbuf.at(0));
				std::vector<wchar_t> wt(len_t * 2);

				size_t len_wt;
				mbstowcs_s(&len_wt, &wt.at(0), wt.size(), &inbuf.at(0), len_t);

				std::wstring fn(tpath);
				std::wofstream outfile(fn, std::ios::out | std::ios::binary);
				outfile.imbue(std::locale("ja_JP.UTF-8"));
				outfile << &wt.at(0);
				outfile.close();

				continue;
			}
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
