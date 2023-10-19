#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cwchar>
#include <climits>
#include <mbstring.h>

static wchar_t MSX_GS[] = { L" !\"#$%&\'()*+,-./0123456789[]<=>?"
							L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
							L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
							L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
							L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
							L"@ABCDEFGHIJKLMNOPQRSTUVWXYZ\x0000\\\x0000^\x0000"
							L"\x0000\x0000\x0000\x0000\x0000\x0000をぁぃぅぇぉゃゅょっ\x0000あいうえおかきくけこさしすせそ"
							L"\x0000。「」、・ヲァィゥェォャュョッーアイウエオカキクケコサシスセソ"
							L"タチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワン゛゜"
							L"たちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん\x0000\x0000" };

static wchar_t X0201kana[] = L" 。「」、・をぁぃぅぇぉゃゅょっーあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん゛゜";


static unsigned __int16 VVal(unsigned __int8 **in)
{
	unsigned t = *(++*in);
	if (t & 0x40) {
		t = ((t & 0x3F) << 8);
		t += *(++*in);
	}
	else {
		t &= 0x3F;
	}

	return t;
}

static unsigned __int16 cali(unsigned __int8** in)
{

}


int wmain(int argc, wchar_t** argv)
{

	int sysver = -1;
	bool encoding_MSX = false;
	bool debug = false;
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'0') {
				sysver = 0;
			}
			else if (*(*argv + 1) == L'1') {
				sysver = 1;
				if (*(*argv + 2) == L'm') {
					encoding_MSX = true;
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
		std::wcout << inbuf.size() << std::endl;

		unsigned __int8* src = (unsigned __int8*)&inbuf.at(0);
		unsigned __int8* src_end = (unsigned __int8*)&inbuf.at(inbuf.size() - 1);
		unsigned __int16 h;
		if (sysver == 1 || sysver == 2 || sysver == 3) {
			h = *(unsigned __int16*)src;
			src += 2;
		}

		_wsetlocale(LC_ALL, L"ja_JP");
		_locale_t loc_jp = _create_locale(LC_CTYPE, "ja_JP");
		std::wstring str;
		while (src <= src_end) {
			// !$&@AFGLPQRSUYZ]{}
			std::wcout << *src << std::endl;
			if (encoding_MSX && MSX_GS[*src] != 0) {
				str.push_back(::MSX_GS[*src]);
				src++;
			}
			else if (*src == 0x20) {
				str.push_back(*src);
				src++;
			}
			else if (*src >= 0xA0 && *src < 0xE0) {
				str.push_back(::X0201kana[*src - 0xA0]);
				src++;
			}
			else if (_mbbtype_l(*src, 0, loc_jp) == 1 && _mbbtype_l(*(src + 1), 1, loc_jp) == 2) {
				wchar_t tmp;
				int ret = _mbtowc_l(&tmp, (const char*)src, 2, loc_jp);

				if (ret != 2) {
					std::wcerr << L"character convert failed." << std::endl;
				}
				str.push_back(tmp);
				src += 2;
			}
			else if (*src == 'R') {
				str.push_back('\n');
				src++;
			}
			else if (*src == 'A') {
				str += L"\nPush any Key.\n";
				src++;
			}
			else if (*src == 'F') {
				str += L"Return to top.\n";
				src++;
			}
			else if (*src == ']') {
				str += L"\nOpen Menu.\n";
				src++;
			}
			else if (*src == 'P') {
				if (sysver == 1) {
					str += L"(Change Text Color:";
					int t = *(src + 1);
					wchar_t a[10] = L"\0";
					_itow_s(t, a, 10);
					str += a;
					str.push_back(L')');
					src += 2;
				}
				else if (sysver == 2) {
					str += L"(Change Cursor Color:";
					int t = *(src + 1);
					wchar_t a[10] = L"\0";
					_itow_s(t, a, 10);
					str += a;
					str.push_back(L')');
					src += 2;
				}
				else if (sysver == 3) {
					// incomplete.
					str += L"(Change Cursor Color:";
					int t = *(src + 1);
					wchar_t a[10] = L"\0";
					_itow_s(t, a, 10);
					str += a;
					str.push_back(L')');
					src += 4;
				}
			}
			else if (*src == '!') {
				str += L"Val";
				int t = VVal(&src);

				wchar_t a[10] = L"\0";
				_itow_s(t, a, 10);
				str += a;
				str += L", ";

				int u = *(++src);
				if (u & 0x40) {
					u = ((u & 0x3F) << 8);
					u += *(++src);
				}
				else {
					u &= 0x3F;
				}

				wchar_t b[10] = L"\0";
				_itow_s(u, b, 10);
				str += b;

				str.push_back(L'\n');
			}
			else if (*src == 'G') {
				str += L"Load Graphic ";
				int t = *(src + 1);
				wchar_t a[10] = L"\0";
				_itow_s(t, a, 10);
				str += a;
				src += 2;
				str.push_back(L'\n');
			}
			else if (*src == 'Z') {
				int sc = *(src + 1) - '@';
				str += L"Extra: ";
				wchar_t o[10] = L"\0";
				_itow_s(sc, o, 10);
				str += o;
				str += L", ";

				int t = *(src + 3) - '@';
				wchar_t a[10] = L"\0";
				_itow_s(t, a, 10);
				str += a;
				src += 5;
				str.push_back(L'\n');
			}
			else {
				src++;
			}

		}

		std::wcout << str << std::endl;
	}


}
