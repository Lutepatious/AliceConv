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

static unsigned __int16 Var[512];

enum class VarName {
	RND = 0,
	D01, D02, D03, D04, D05, D06, D07, D08, D09, D10, D11, D12, D13, D14, D15, D16, D17, D18, D19, D20,
	U01, U02, U03, U04, U05, U06, U07, U08, U09, U10, U11, U12, U13, U14, U15, U16, U17, U18, U19, U20,
	B01, B02, B03, B04, B05, B06, B07, B08, B09, B10, B11, B12, B13, B14, B15, B16, B17, B18, B19, B20,
	M_X, M_Y
};

bool is_Little_Vampire_MSX2 = false;

static unsigned __int16 VVal(unsigned __int8** in)
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

std::wstring CALI(unsigned __int8** in)
{
	// import from T.T sys32
	std::vector<std::wstring> mes;

	while (1) {
		if ((*(++*in) & 0xC0) == 0x80) { // 0x80-0xBF
			wchar_t tstr[10];
			swprintf_s(tstr, sizeof(tstr) / sizeof(wchar_t), L"Var%d", **in & 0x3F);
			mes.push_back(tstr);
		}
		else if ((**in & 0xC0) == 0xC0) { // 0xC0-0xFF
			wchar_t tstr[10];
			swprintf_s(tstr, sizeof(tstr) / sizeof(wchar_t), L"Var%d", ((**in & 0x3F) << 8) | *(++*in));
			mes.push_back(tstr);
		}
		else if ((**in & 0xC0) == 0x00) {
			wchar_t tstr[7];
			if (::is_Little_Vampire_MSX2) {
				swprintf_s(tstr, sizeof(tstr) / sizeof(wchar_t), L"%d", (**in & 0x3F));
			}
			else {
				swprintf_s(tstr, sizeof(tstr) / sizeof(wchar_t), L"%d", ((**in & 0x3F) << 8) | *(++ * in));
			}
			mes.push_back(tstr);
		}
		else if ((**in & 0xC0) == 0x40) { // 0x40-0x7F
			if (**in < 0x78) {
				wchar_t tstr[7];
				swprintf_s(tstr, sizeof(tstr) / sizeof(wchar_t), L"%d", (**in & 0x3F));
				mes.push_back(tstr);
			}
			else if (**in == 0x78) {
				std::wstring t = L"(" + * (mes.end() - 2) + L" *= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x79) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" += " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x7A) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" -= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x7B) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" == " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x7C) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" < " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x7D) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" > " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x7E) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" != " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
			}
			else if (**in == 0x7F) {
				return *mes.begin();
			}
		}
	}
}

static wchar_t* InttoDEC(int val)
{
	static wchar_t a[12] = L"\0";
	_itow_s(val, a, 10);

	return a;
}

static wchar_t* InttoHEX(int val)
{
	static wchar_t a[9] = L"\0";
	_itow_s(val, a, 16);

	return a;
}


int wmain(int argc, wchar_t** argv)
{

	int sysver = -1;
	int nest = 0;
	bool encoding_MSX = false;
	bool debug = false;
	bool set_menu = false;

	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'0') {
				sysver = 0;
				if (*(*argv + 2) == L'v') {
					encoding_MSX = true;
					::is_Little_Vampire_MSX2 = true;
				}
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
		unsigned __int8* src_start = src;
		unsigned __int8* src_end = (unsigned __int8*)&inbuf.at(inbuf.size() - 1);
		unsigned __int16 h;
		if (sysver == 1 || sysver == 2 || sysver == 3) {
			h = *(unsigned __int16*)src;
			src += 2;
		}

		_wsetlocale(LC_ALL, L"ja_JP");
		_locale_t loc_jp = _create_locale(LC_CTYPE, "ja_JP");
		std::wstring str;
		std::vector<unsigned __int16> Labels;
		while (src <= src_end) {
			for (auto& i : Labels) {
				if (src - src_start == i) {
					wchar_t slabel[14];
					swprintf_s(slabel, sizeof(slabel) / sizeof(wchar_t), L"\nLabel%04X:\n", i);
					str += slabel;
				}
			}

			// !$&@AFGLPQRSUYZ]{}
			if (encoding_MSX && MSX_GS[*src] != 0) {
				str.push_back(::MSX_GS[*src]);
			}
			else if (*src == 0x20) {
				str.push_back(*src);
			}
			else if (*src >= 0xA0 && *src < 0xE0) {
				str.push_back(::X0201kana[*src - 0xA0]);
			}
			else if (_mbbtype_l(*src, 0, loc_jp) == 1 && _mbbtype_l(*(src + 1), 1, loc_jp) == 2) {
				wchar_t tmp;
				int ret = _mbtowc_l(&tmp, (const char*)src, 2, loc_jp);
				src++;

				if (ret != 2) {
					std::wcerr << L"character convert failed." << std::endl;
				}
				str.push_back(tmp);
			}
			else if (*src == 'R') {
				str.push_back('\n');
			}
			else if (*src == 'A') {
				str += L"\nPush any Key.\n";
			}
			else if (*src == 'F') {
				str += L"Return to top.\n";
			}
			else if (*src == ']') {
				str += L"\nOpen Menu.\n";
			}
			else if (*src == 'P') {
				str += L"(Change Text Color:";
				str += InttoDEC(*++src);
				str.push_back(L')');
			}
			else if (*src == '!') {
				int val = VVal(&src);
				wchar_t slabel[20];
				swprintf_s(slabel, sizeof(slabel) / sizeof(wchar_t), L"\nVar%d = %s\n", val, CALI(&src).c_str());
				str += slabel;
			}
			else if (*src == 'G') {
				str += L"Load Graphic ";
				str += InttoDEC(*++src);
				str.push_back(L'\n');
			}
			else if (*src == 'Y') {
				str += L"Extra1: ";
				str += CALI(&src);
				str += L", ";
				str += CALI(&src);
				str.push_back(L'\n');
			}
			else if (*src == 'Z') {
				str += L"Extra2: ";
				str += CALI(&src);
				str += L", ";
				str += CALI(&src);
				str.push_back(L'\n');
			}
			else if (*src == '[') {
				int p0 = *++src;
				int p1 = *++src;
				int Addr = *(unsigned __int16*)(++src);
				Labels.push_back(Addr);
				src++;

				wchar_t slabel[20];
				swprintf_s(slabel, sizeof(slabel) / sizeof(wchar_t), L"Label%04X %u, %u\n", Addr, p0, p1);
				str += slabel;
			}
			else if (*src == ':') {
				std::wstring A = CALI(&src);
				int p0 = *++src;
				int p1 = *++src;
				int Addr = *(unsigned __int16*)(++src);
				Labels.push_back(Addr);
				src++;

				wchar_t slabel[40];
				swprintf_s(slabel, sizeof(slabel) / sizeof(wchar_t), L"if %s then Label%04X %u, %u\n", A.c_str(), Addr, p0, p1);
				str += slabel;
			}
			else if (*src == '&') {
				str += L"\nJump to page " + CALI(&src) + L".\n";
			}
			else if (*src == '{') {
				nest++;
				str += L"\n{ " + CALI(&src) + L"\n";

			}
			else if (*src == '}') {
				if (nest) {
					nest--;
				}
				str += L"\n}\n";
			}
			else if (*src == '@') {
				int Addr = *(unsigned __int16*)(++src);
				Labels.push_back(Addr);
				src++;

				wchar_t slabel[20];
				swprintf_s(slabel, sizeof(slabel) / sizeof(wchar_t), L"Jump to Label%04X\n", Addr);
				str += slabel;
			}
			else if (*src == '$') {
				if (set_menu) {
					set_menu = false;
				}
				else {
					int Addr = *(unsigned __int16*)(++src);
					Labels.push_back(Addr);
					src++;

					wchar_t slabel[20];
					swprintf_s(slabel, sizeof(slabel) / sizeof(wchar_t), L"\nLabel%04X ", Addr);
					str += slabel;

					set_menu = true;
				}
			}
			else {
				std::wcout << *src << L"," << *(src + 1) << L"," << *(src + 2) << L"," << *(src + 3) << std::endl;
			}
			src++;

		}








		std::wcout << str << std::endl;
	}


}
