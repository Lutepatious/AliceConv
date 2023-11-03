#include <fstream>

#include "toTXT.hpp"

int wmain(int argc, wchar_t** argv)
{
	wchar_t path[_MAX_PATH];
	wchar_t fname[_MAX_FNAME];
	wchar_t dir[_MAX_DIR];
	wchar_t drive[_MAX_DRIVE];

	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	int sysver = -1;
	bool encoding_MSX = false;
	bool is_GakuenSenki = false;
	bool is_DALK = false;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'0') {
				sysver = 0;
				if (*(*argv + 2) == L'm') {
					encoding_MSX = true;
				}
			}
			else if (*(*argv + 1) == L'1') {
				sysver = 1;
				if (*(*argv + 2) == L'm') {
					encoding_MSX = true;
				}
				else if (*(*argv + 2) == L'g') {
					encoding_MSX = true;
					is_GakuenSenki = true;
				}
			}
			else if (*(*argv + 1) == L'2') {
				sysver = 2;
				if (*(*argv + 2) == L'd') {
					is_DALK = true;
				}
			}
			else if (*(*argv + 1) == L'3') {
				sysver = 3;
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

		std::wstring out;

		// MSX版ではEOFが途中でも現れるため、その後にNULが3バイト続くことを想定しているが、ファイル末端にEOFが来る事もあるので3バイトのNULを付加。
		if (encoding_MSX) {
			inbuf.insert(inbuf.end(), 3, 0);
		}

		if (sysver == 0) {
			toTXT0 sys0;
			sys0.init(inbuf, encoding_MSX);
			out = sys0.decode();
		}

		else if (sysver == 1) {
			toTXT1 sys1;
			sys1.init(inbuf, encoding_MSX);
			if (is_GakuenSenki) {
				sys1.is_GakuenSenkiMSX = true;
			}
			out = sys1.decode();
		}

		else if (sysver == 2) {
			if (is_DALK) {
				toTXT2d sys2;
				sys2.init(inbuf);
				out = sys2.decode();
			}
			else {
				toTXT2 sys2;
				sys2.init(inbuf);
				out = sys2.decode();
			}
		}

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".txt");
		_wsetlocale(LC_ALL, L"ja_JP");
		std::wstring fn(path);
		std::wofstream outfile(fn, std::ios::out);
		outfile.imbue(std::locale("ja_JP.UTF-8"));
		outfile << out;
		outfile.close();

	}
}
