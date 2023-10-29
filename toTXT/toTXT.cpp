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

		toTXT0 sys0;
		toTXT1 sys1;
		toTXT2 sys2;
		std::wstring out;

		if (sysver == 0) {
			sys0.init(inbuf, encoding_MSX);
			out = sys0.decode();
		}

		else if (sysver == 1) {
			sys1.init(inbuf, encoding_MSX);
			if (is_GakuenSenki) {
				sys1.is_GakuenSenkiMSX = true;
			}
			out = sys1.decode();
		}

		else if (sysver == 2) {
			sys2.init(inbuf);
			out = sys2.decode();
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
