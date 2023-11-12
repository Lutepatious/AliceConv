#ifndef TOTXT_TOTXT
#define TOTXT_TOTXT
#include <locale>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>
#include <utility>
#include <cwchar>
#include <climits>
#include <mbstring.h>
#include <mbctype.h>

enum class VarName {
	RND = 0,
	D01, D02, D03, D04, D05, D06, D07, D08, D09, D10, D11, D12, D13, D14, D15, D16, D17, D18, D19, D20,
	U01, U02, U03, U04, U05, U06, U07, U08, U09, U10, U11, U12, U13, U14, U15, U16, U17, U18, U19, U20,
	B01, B02, B03, B04, B05, B06, B07, B08, B09, B10, B11, B12, B13, B14, B15, B16, B17, B18, B19, B20,
	M_X, M_Y
};

class toTXT {
	// printfの出力用バッファ

	// リトルヴァンパイアMSX版と学園戦記MSX版用文字変換テーブル
	const wchar_t* MSX_char_table = L" !\"#$%&\'()*+,-./0123456789[]<=>?"
		L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
		L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
		L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
		L"\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000\x0000"
		L"@ABCDEFGHIJKLMNOPQRSTUVWXYZ\x0000\\\x0000^\x0000"
		L"\x0000\x0000\x0000\x0000\x0000\x0000をぁぃぅぇぉゃゅょっ\x0000あいうえおかきくけこさしすせそ"
		L"\x0000。「」、・ヲァィゥェォャュョッーアイウエオカキクケコサシスセソ"
		L"タチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワン゛゜"
		L"たちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん\x0000\x0000";

	// 清音濁音変換テーブル
	const wchar_t* seion = L"うかきくけこさしすせそたちつてとはひふへほウカキクケコサシスセソタチツテトハヒフヘホ";
	const wchar_t* dakuon = L"ゔがぎぐげござじずぜぞだぢづでどばびぶべぼヴガギグゲゴザジズゼゾダヂヅデドバビブベボ";

	// 清音半濁音変換テーブル
	const wchar_t* seion2 = L"はひふへほハヒフヘホ";
	const wchar_t* handakuon = L"ぱぴぷぺぽパピプペポ";

	// 元ソース
	// https://www.wabiapp.com/WabiSampleSource/windows/replace_string_w.html
	std::wstring ReplaceString(std::wstring target, std::wstring from, std::wstring to)
	{
		std::wstring::size_type  Pos(target.find(from));

		while (Pos != std::string::npos)
		{
			target.replace(Pos, from.length(), to);
			Pos = target.find(from, Pos + to.length());
		}

		return target;
	}

	// 文字列の正規化
	std::wstring CleanUpString(std::wstring& in)
	{
		// 清音+濁点を濁音へ
		for (size_t i = 0; i < 42; i++) {
			std::wstring f{ this->seion[i] };
			std::wstring t{ this->dakuon[i] };
			f += L"゛";
			in = ReplaceString(in, f, t);
		}

		// 清音+半濁点を半濁音へ
		for (size_t i = 0; i < 10; i++) {
			std::wstring f{ this->seion2[i] };
			std::wstring t{ this->handakuon[i] };
			f += L"゜";
			in = ReplaceString(in, f, t);
		}

		// 連続する改行をまとめる
		in = ReplaceString(in, L"\n\n", L"\n");

		return in;
	}

	bool encoding_MSX = false;
	bool is_end(void)
	{
		if (this->encoding_MSX) {
			if (*(unsigned __int32*)this->src == 0x1A) {
				return true;
			}
			else {
				return false;
			}
		}
		else if (*(unsigned __int32*)this->src == 0) {
			return true;
		}
		else if (*(unsigned __int32*)this->src == 0x02020102) {
			return true;
		}
		else if (*this->src == 0x1A) {
			return true;
		}
		return false;
	}

	virtual std::wstring command_block_begin(void)
	{
		auto p1 = this->CALI();
		std::wstring ret = L"\n" + p1 + L" {\n";
		return ret;
	}

	virtual std::wstring command_block_end(void)
	{
		std::wstring ret = L"\n}\n";
		return ret;
	}

	virtual std::wstring command_G(void) // Load Graphics
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nLoad Graphics " + p1 + L"\n";
		return ret;
	}

	virtual std::wstring command_U(void) // Load Graphics
	{
		auto p1 = std::to_wstring(this->get_byte());
		auto p2 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nLoad Graphics " + p1 + L", Transparent " + p2 + L"\n";
		return ret;
	}

	virtual std::wstring command_P(void) // Set Text Color
	{
		// Set text color 0-7 Black, Blue, Red, Magenta, Green, Cyan, Yellow, White
		auto p1 = this->get_byte();
		std::wstring ret = this->text_color[p1];
		return ret;
	}

	virtual std::wstring command_B(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_D(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_E(void)
	{
		std::wstring ret;
		return ret;
	}
	virtual std::wstring command_H(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_I(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_J(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_K(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_M(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_N(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_O(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_T(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_V(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual std::wstring command_W(void)
	{
		std::wstring ret;
		return ret;
	}

	virtual unsigned __int16 get_16(void) = 0;
	virtual unsigned __int16 get_Vword(void) = 0;
	virtual std::wstring CALI(void) = 0;
	virtual std::wstring command_Q(void) = 0; // Save Playdata
	virtual std::wstring command_L(void) = 0; // Load Playdata
	virtual std::wstring command_Y(void) = 0; // Extra1
	virtual std::wstring command_Z(void) = 0; // Extra2

protected:
	unsigned __int8* src = nullptr;
	unsigned __int8* src_start = nullptr;
	unsigned __int8* src_end = nullptr;
	wchar_t printf_buf[1000] = { 0 };
	wchar_t printf_buf_len = 1000;
	const wchar_t* text_color[8] = { L"(Black)", L"(Blue)", L"(Red)", L"(Magenta)", L"(Green)", L"(Cyan)", L"(Yellow)", L"(White)" };
	unsigned __int16 header = 0;

	unsigned __int16 get_byte(void)
	{
		return *this->src++;
	}

	unsigned __int16 get_word(void)
	{
		unsigned __int16 ret = *(unsigned __int16*)(this->src);
		this->src += 2;

		return ret;
	}

	unsigned __int16 VL_Value(void)
	{
		unsigned t = *this->src++;

		if (t & 0x40) {
			t = (t & 0x3F) << 8;
			t += *this->src++;
		}
		else {
			t &= 0x3F;
		}

		return t;
	}

	unsigned __int16 VL_Value_NEG(void)
	{
		unsigned t = *this->src++;

		if (t & 0x40) {
			t &= 0x3F;
		}
		else {
			t = (t & 0x3F) << 8;
			t += *this->src++;
		}

		return t;
	}

	std::wstring CALI3(void)
	{
		// import from T.T sys32
		std::vector<std::wstring> mes;
		while (*this->src != 0x7F) {
			if (*this->src == 0x78) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" *= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x79) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" += " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7A) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" -= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7B) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" == " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7C) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" < " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7D) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" > " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7E) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" != " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src & 0x80) {
				int t = VL_Value();
				swprintf_s(this->printf_buf, this->printf_buf_len, L"Var%d", t);
				mes.push_back(this->printf_buf);
			}
			else { // 0x00-0x77
				swprintf_s(this->printf_buf, this->printf_buf_len, L"%d", *this->src);
				mes.push_back(this->printf_buf);
				++this->src;
			}
		}

		++this->src;
		return *mes.begin();
	}

	std::wstring CALI5(void)
	{
		// import from T.T sys32
		std::vector<std::wstring> mes;

		while (*this->src != 0x7F) {
			if (*this->src == 0x78) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" *= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x79) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" += " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7A) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" -= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7B) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" == " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7C) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" < " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7D) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" > " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7E) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" != " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src & 0x80) { // 0x80-0xFF
				int t = VL_Value();
				swprintf_s(this->printf_buf, this->printf_buf_len, L"Var%d", t);
				mes.push_back(this->printf_buf);
			}
			else {
				int t = VL_Value_NEG();
				swprintf_s(this->printf_buf, this->printf_buf_len, L"%d", t);
				mes.push_back(this->printf_buf);
			}
		}

		++this->src;
		return *mes.begin();
	}

	std::wstring CALI7(void)
	{
		// import from T.T sys32
		std::vector<std::wstring> mes;

		while (*this->src != 0x7F) {
			if (*this->src == 0x77) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" *= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x78) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" /= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x79) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" += " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7A) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" -= " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7B) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" == " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7C) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" < " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7D) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" > " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src == 0x7E) {
				std::wstring t = L"(" + *(mes.end() - 2) + L" != " + *(mes.end() - 1) + L")";
				mes.pop_back();
				mes.pop_back();
				mes.push_back(t);
				++this->src;
			}
			else if (*this->src & 0x80) { // 0x80-0xFF
				int t = VL_Value();
				swprintf_s(this->printf_buf, this->printf_buf_len, L"Var%d", t);
				mes.push_back(this->printf_buf);
			}
			else {
				int t = VL_Value_NEG();
				swprintf_s(this->printf_buf, this->printf_buf_len, L"%d", t);
				mes.push_back(this->printf_buf);
			}
		}

		++this->src;
		return *mes.begin();
	}

public:
	// Shift-JISエンコードでのカタカナ→ひらがな復元テーブル
	const wchar_t* X0201kana_table = L" 。「」、・をぁぃぅぇぉゃゅょっーあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん゛゜";

	void init(std::vector<__int8>& s, bool enc = false)
	{
		this->src_start = this->src = (unsigned __int8*)&*s.begin();
		this->src_end = (unsigned __int8*)&*s.end();
		this->encoding_MSX = enc;
		this->header = this->get_16();
	}

	std::wstring get_string(void)
	{
		std::wstring ret;

		if (this->encoding_MSX) {
			while (this->MSX_char_table[*this->src] != 0) {
				ret += MSX_char_table[*this->src];
				this->src++;
			}
		}
		else {
			_wsetlocale(LC_ALL, L"ja_JP");
			_locale_t loc_jp = _create_locale(LC_CTYPE, "ja_JP");
			while (*this->src == ' ' || _ismbbkana_l(*this->src, loc_jp) || _ismbblead_l(*this->src, loc_jp) && _ismbbtrail_l(*(this->src + 1), loc_jp)) {
				if (*this->src == ' ') {
					ret += L" ";
					this->src++;
				}
				else if (_ismbbkana_l(*this->src, loc_jp)) {
					ret += this->X0201kana_table[*this->src - 0xA0];
					this->src++;
				}
				else if (_ismbblead_l(*this->src, loc_jp) && _ismbbtrail_l(*(this->src + 1), loc_jp)) {
					wchar_t tmp;
					int nret = _mbtowc_l(&tmp, (const char*)this->src, 2, loc_jp);

					if (nret != 2) {
						std::wcerr << L"character convert failed." << std::endl;
					}
					ret += tmp;
					this->src += 2;
				}
			}
		}
		return ret;
	}

	std::wstring decode(void)
	{
		bool debug = false;
		std::vector<std::pair<unsigned __int16, std::wstring>> decoded_commands;
		std::pair<unsigned __int16, std::wstring> decoded_command;
		int nest = 0;
		bool set_menu = false;

		_wsetlocale(LC_ALL, L"ja_JP");
		_locale_t loc_jp = _create_locale(LC_CTYPE, "ja_JP");
		std::wstring str;
		std::set<unsigned __int16> Labels;

		decoded_command.first = 0;
		decoded_command.second = this->printf_buf;
		decoded_commands.push_back(decoded_command);
		std::vector<unsigned __int16> Call_Address;

		while (this->src < this->src_end && !this->is_end()) {
			unsigned __int16 Address = this->src - this->src_start;
			decoded_command.first = Address;
			decoded_command.second.clear();

			if (debug) {
				swprintf_s(printf_buf, printf_buf_len, L"%04X", Address);
				std::wcout << printf_buf << std::endl;
			}

			// Output Characters
			decoded_command.second = this->get_string();
			decoded_commands.push_back(decoded_command);

			Address = this->src - this->src_start;
			decoded_command.first = Address;
			decoded_command.second.clear();

			if (debug) {
				swprintf_s(printf_buf, printf_buf_len, L"%04X", Address);
				std::wcout << printf_buf << std::endl;
			}

			switch (*this->src++) {
			case '!': // Set Variable
			{
				auto p1 = std::to_wstring(this->get_Vword());
				auto p2 = this->CALI();
				decoded_command.second = L"\nVar" + p1 + L" = " + p2 + L"\n";
				break;
			}

			case '{':
				nest++;
				decoded_command.second = this->command_block_begin();
				break;

			case '}':
				if (nest) {
					nest--;
				}
				decoded_command.second = this->command_block_end();
				break;

			case ']':
				decoded_command.second = L"\nOpen Menu.\n";
				break;

			case '\\':
			{
				int Addr = this->get_word();

				if (Addr) {
					Labels.insert(Addr);
					Labels.insert(decoded_command.first);
					Call_Address.push_back(decoded_command.first);
					swprintf_s(this->printf_buf, this->printf_buf_len, L"\nCall to Label%04X\n", Addr);
				}
				else {
					auto Addr_ret = *(Call_Address.end() - 1);
					Call_Address.pop_back();
					swprintf_s(this->printf_buf, this->printf_buf_len, L"\nReturn to Label%04X\n", Addr_ret);
				}

				decoded_command.second = this->printf_buf;
				break;
			}

			case '%':
			{
				std::wstring A = this->CALI();
				decoded_command.second = L"\nCall page " + A + L".\n";
				break;
			}

			case 'P': // Set Text Color
				decoded_command.second = this->command_P();
				break;

			case 'R':
				decoded_command.second = L"\n";
				break;

			case '@':
			{
				int Addr = this->get_word();
				Labels.insert(Addr);

				swprintf_s(this->printf_buf, this->printf_buf_len, L"\nJump to Label%04X\n", Addr);
				decoded_command.second = this->printf_buf;
				break;
			}

			case 'K':
				decoded_command.second = this->command_K();
				break;

			case 'J':
				decoded_command.second = this->command_J();
				break;

			case 'O':
				decoded_command.second = this->command_O();
				break;

			case 'N':
				decoded_command.second = this->command_N();
				break;

			case 'T': // Save What?
				decoded_command.second = this->command_T();
				break;

			case 'W':
				decoded_command.second = this->command_W();
				break;

			case 'V':
				decoded_command.second = this->command_V();
				break;

			case 'B':
				decoded_command.second = this->command_B();
				break;

			case 'H':
				decoded_command.second = this->command_H();
				break;

			case 'I':
				decoded_command.second = this->command_I();
				break;

			case 'M':
				decoded_command.second = this->command_M();
				break;

			case 'D':
				decoded_command.second = this->command_D();
				break;

			case 'E':
				decoded_command.second = this->command_E();
				break;

			case 'F':
				decoded_command.second = L"\nReturn to top.\n";
				break;

			case '$':
				if (set_menu) {
					set_menu = false;
				}
				else {
					int Addr = this->get_word();
					Labels.insert(Addr);

					swprintf_s(this->printf_buf, this->printf_buf_len, L"\nLabel%04X ", Addr);
					decoded_command.second = this->printf_buf;
					set_menu = true;
				}
				break;

			case 'A':
				decoded_command.second = L"\nHit any Key.\n";
				break;

			case 'G': // Load Graphics
				decoded_command.second = this->command_G();
				break;

			case 'U': // Load Graphics with Transparent
				decoded_command.second = this->command_U();
				break;

			case 'S':
			{
				auto p1 = std::to_wstring(this->get_byte());
				decoded_command.second = L"\nLoad Sound " + p1 + L"\n";
				break;
			}

			case '&':
			{
				std::wstring A = this->CALI();
				decoded_command.second = L"\nJump to page " + A + L".\n";
				break;
			}

			case 'L': // Load Playdata
				decoded_command.second = this->command_L();
				break;

			case 'Q': // Save Playdata
				decoded_command.second = this->command_Q();
				break;


			case 'X':
			{
				auto p1 = std::to_wstring(this->get_byte());
				decoded_command.second = L"(Print $" + p1 + L")";
				break;
			}

			case 'Y': // Extra1
				decoded_command.second = this->command_Y();
				break;

			case 'Z': // Extra2
				decoded_command.second = this->command_Z();
				break;

			case '[':
			{
				auto p1 = this->get_byte();
				auto p2 = this->get_byte();
				auto Addr = this->get_word();
				Labels.insert(Addr);

				swprintf_s(this->printf_buf, this->printf_buf_len, L"\nLabel%04X %u, %u\n", Addr, p1, p2);
				decoded_command.second = this->printf_buf;
				break;
			}
			case ':':
			{
				std::wstring A = this->CALI();
				auto p2 = this->get_byte();
				auto p3 = this->get_byte();
				auto Addr = this->get_word();
				Labels.insert(Addr);

				swprintf_s(this->printf_buf, this->printf_buf_len, L"\n%s Label%04X %u, %u\n", A.c_str(), Addr, p2, p3);
				decoded_command.second = this->printf_buf;
				break;
			}
			default:
				swprintf_s(this->printf_buf, this->printf_buf_len, L"\nUnknown at %04X:%02X %02X %02X %02X", decoded_command.first, *(this->src - 1), *this->src, *(this->src + 1), *(this->src + 2));
				decoded_command.second = this->printf_buf;
				std::wcout << decoded_command.second;

			}

			decoded_commands.push_back(decoded_command);
		}

		unsigned __int16 prev = 0;
		for (auto& i : decoded_commands) {
			for (auto& j : Labels) {
				if (i.first == j) {
					if (prev != j) {
						swprintf_s(this->printf_buf, this->printf_buf_len, L"\nLabel%04X:\n", j);
						str += this->printf_buf;
						prev = j;
					}
				}
			}

			str += i.second;
		}

		return CleanUpString(str);
	}
};

class toTXT0 : public toTXT {
	unsigned __int16 inline get_16(void)
	{
		return 0;
	}

	unsigned __int16 inline get_Vword(void)
	{
		return *this->src++;
	}

	std::wstring inline CALI(void)
	{
		return this->CALI3();
	}

	std::wstring command_Q(void)
	{
		std::wstring ret{ L"\nSave Playdata\n" };
		return ret;
	}

	std::wstring command_L(void)
	{
		std::wstring ret{ L"\nLoad Playdata\n" };
		return ret;
	}

	std::wstring command_Y(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nExtra1 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_Z(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nExtra2 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

};

class toTXT1 : public toTXT {
	unsigned __int16 inline get_16(void)
	{
		return this->get_word();
	}

	unsigned __int16 inline get_Vword(void)
	{
		return this->VL_Value();
	}

	std::wstring inline CALI(void)
	{
		return this->CALI5();
	}

	std::wstring command_Q(void)
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nSave Playdata " + p1 + L"\n";
		return ret;
	}

	std::wstring command_L(void)
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nLoad Playdata " + p1 + L"\n";
		return ret;
	}

	std::wstring command_Y(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nExtra1 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_Z(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret;

		if (this->is_GakuenSenkiMSX) {
			unsigned __int32 f = std::stoul(p1);
			if (f == 1) {
				ret = L"\nLoad Graphics " + std::to_wstring(std::stoul(p2) + 250) + L"\n";
			}
			else {
				ret = L"\nExtra2 " + p1 + L", " + p2 + L"\n";
			}
		}
		else {
			ret = L"\nExtra2 " + p1 + L", " + p2 + L"\n";
		}
		return ret;
	}


public:
	bool is_GakuenSenkiMSX = false;

};

class toTXT2 : public toTXT {
	unsigned __int16 inline get_16(void)
	{
		return get_word();
	}

	unsigned __int16 inline get_Vword(void)
	{
		return this->VL_Value();
	}

	virtual std::wstring command_B(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring p5 = CALI();
		std::wstring p6 = CALI();
		std::wstring p7 = CALI();
		std::wstring ret = L"\nB " + p1 + L", " + p2 + L", " + p3 + L", " + p4 + L", " + p5 + L", " + p6 + L", " + p7 + L"\n";
		return ret;
	}
	virtual std::wstring command_D(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring p5 = CALI();
		std::wstring p6 = CALI();
		std::wstring p7 = CALI();
		std::wstring p8 = CALI();
		std::wstring ret = L"\nD " + p1 + L", " + p2 + L", " + p3 + L", " + p4 + L", " + p5 + L", " + p6 + L", " + p7 + L", " + p8 + L"\n";
		return ret;
	}

	virtual std::wstring command_E(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nE " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	virtual std::wstring command_I(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		auto p3 = get_byte();
		std::wstring ret = L"\nI " + p1 + L", " + p2 + L", " + std::to_wstring(p3) + L"\n";
		return ret;
	}

	std::wstring command_J(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nJ " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_N(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nN " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_T(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nT " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_O(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nO " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_M(void)
	{
		std::wstring ret = L"\n(" + this->get_string();

		if (*this->src == '\'') {
			ret += L"'";
			++this->src;
		}

		if (*this->src == ':') {
			ret += L")";
			++this->src;
		}
		return ret;
	}

	std::wstring command_H(void)
	{
		auto p1 = get_byte();
		std::wstring p2 = CALI();
		std::wstring ret = L"(";
		while (p1--) {
			ret += L"#";
		}
		ret += L"," + p2 + L")";
		return ret;
	}

	virtual std::wstring command_V(void)
	{
		auto p1 = get_byte();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nV " + std::to_wstring(p1) + L", " + p2;
		if (p1) {
			std::wstring p3[28];

			for (size_t i = 0; i < 28; i++) {
				p3[i] = CALI();
			}

			for (size_t i = 0; i < 28; i++) {
				ret += L", " + p3[i];
			}
		}
		else {
			std::pair<unsigned __int16, unsigned __int16> p3[28];

			for (size_t i = 0; i < 28; i++) {
				p3[i].first = VL_Value();
				p3[i].second = get_byte();
			}

			for (size_t i = 0; i < 28; i++) {
				ret += L", Var" + std::to_wstring(p3[i].first) + L" = " + std::to_wstring(p3[i].second);
			}
		}

		ret += L"\n";
		return ret;
	}

	virtual std::wstring command_W(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring ret = L"\nMask (" + p1 + L"," + p2 + L") - (" + p3 + L"," + p4 + L")\n";
		return ret;
	}

	std::wstring command_G(void) // Load Graphics
	{
		std::wstring p1 = CALI();
		std::wstring ret = L"\nLoad Graphics " + p1 + L"\n";
		return ret;
	}

	std::wstring command_U(void) // Load Graphics
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nLoad Graphics " + p1 + L", Transparent " + p2 + L"\n";
		return ret;
	}

	std::wstring command_P(void) // Set Text Color
	{
		// Set text color Palette 0-15
		auto p1 = this->get_byte();
		std::wstring ret = L"(Color #" + std::to_wstring(p1) + L")";
		return ret;
	}
	std::wstring command_Q(void)
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nSave Playdata " + p1 + L"\n";
		return ret;
	}

	std::wstring command_L(void)
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nLoad Playdata " + p1 + L"\n";
		return ret;
	}

	std::wstring command_Y(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nExtra1 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	virtual std::wstring command_Z(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret;

		ret = L"\nExtra2 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

public:
	std::wstring inline CALI(void)
	{
		return this->CALI7();
	}
};

class toTXT2d : public toTXT2 {
	std::wstring command_I(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nI " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}
};

class toTXT2r4 : public toTXT2 {
	std::wstring command_W(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nMask (" + p1 + L") - (" + p2 + L"," + p3 + L")\n";
		return ret;
	}

	std::wstring command_V(void)
	{
		auto p1 = get_byte();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nV " + std::to_wstring(p1) + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_B(void)
	{
		auto p1 = get_byte();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nB " + std::to_wstring(p1) + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_I(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nI " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_D(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nD " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_E(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring p5 = CALI();
		std::wstring p6 = CALI();
		std::wstring ret = L"\nE " + p1 + L", " + p2 + L", " + p3 + L"," + p4 + L", " + p5 + L", " + p6 + L"\n";
		return ret;
	}

	std::wstring command_Z(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret;

		ret = L"\nExtra2 " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}
};

class toTXT3 : public toTXT {
	unsigned __int16 inline get_16(void)
	{
		return get_word();
	}

	unsigned __int16 inline get_Vword(void)
	{
		return this->VL_Value();
	}

	std::wstring command_block_begin(void)
	{
		auto p1 = this->CALI();
		auto p2 = std::to_wstring(this->get_word());
		std::wstring ret = L"\n" + p1 + L"," + p2 + L" {\n";
		return ret;
	}

	std::wstring command_block_end(void)
	{
		auto p1 = this->CALI();
		auto p2 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\n" + p1 + L"," + p2 + L" {\n";
		return ret;
	}

	std::wstring command_P(void) // Set Text Color
	{
		// Set text color Pal using RGB
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring ret = L"(Color Pal#" + p1 + L" R" + p2 + L" G" + p3 + L" B" + p4 + L")";
		return ret;
	}

	std::wstring command_K(void)
	{
		std::wstring p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nK " + p1 + L"\n";
		return ret;
	}

	std::wstring command_J(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nJ " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_O(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = std::to_wstring(VL_Value());
		std::wstring p3 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nO " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_N(void)
	{
		std::wstring p1 = std::to_wstring(this->get_byte());
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nN " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_B(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring p5 = CALI();
		std::wstring p6 = CALI();
		std::wstring p7 = CALI();
		std::wstring ret = L"\nB " + p1 + L", " + p2 + L", " + p3 + L", " + p4 + L", " + p5 + L", " + p6 + L", " + p7 + L"\n";
		return ret;
	}
	std::wstring command_D(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring p5 = CALI();
		std::wstring p6 = CALI();
		std::wstring p7 = CALI();
		std::wstring p8 = CALI();
		std::wstring ret = L"\nD " + p1 + L", " + p2 + L", " + p3 + L", " + p4 + L", " + p5 + L", " + p6 + L", " + p7 + L", " + p8 + L"\n";
		return ret;
	}

	std::wstring command_E(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nE " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_I(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		auto p3 = get_byte();
		std::wstring ret = L"\nI " + p1 + L", " + p2 + L", " + std::to_wstring(p3) + L"\n";
		return ret;
	}

	std::wstring command_T(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring ret = L"\nT " + p1 + L", " + p2 + L", " + p3 + L"\n";
		return ret;
	}

	std::wstring command_M(void)
	{
		std::wstring ret = L"\n(" + this->get_string();

		if (*this->src == '\'') {
			ret += L"'";
			++this->src;
		}

		if (*this->src == ':') {
			ret += L")";
			++this->src;
		}
		return ret;
	}

	std::wstring command_H(void)
	{
		auto p1 = get_byte();
		std::wstring p2 = CALI();
		std::wstring ret = L"(";
		while (p1--) {
			ret += L"#";
		}
		ret += L"," + p2 + L")";
		return ret;
	}

	std::wstring command_V(void)
	{
		auto p1 = get_byte();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nV " + std::to_wstring(p1) + L", " + p2;
		if (p1) {
			std::wstring p3[28];

			for (size_t i = 0; i < 28; i++) {
				p3[i] = CALI();
			}

			for (size_t i = 0; i < 28; i++) {
				ret += L", " + p3[i];
			}
		}
		else {
			std::pair<unsigned __int16, unsigned __int16> p3[28];

			for (size_t i = 0; i < 28; i++) {
				p3[i].first = VL_Value();
				p3[i].second = get_byte();
			}

			for (size_t i = 0; i < 28; i++) {
				ret += L", Var" + std::to_wstring(p3[i].first) + L" = " + std::to_wstring(p3[i].second);
			}
		}

		ret += L"\n";
		return ret;
	}

	std::wstring command_W(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring p3 = CALI();
		std::wstring p4 = CALI();
		std::wstring ret = L"\nMask (" + p1 + L"," + p2 + L") - (" + p3 + L"," + p4 + L")\n";
		return ret;
	}

	std::wstring command_G(void) // Load Graphics
	{
		std::wstring p1 = CALI();
		std::wstring ret = L"\nLoad Graphics " + p1 + L"\n";
		return ret;
	}

	std::wstring command_U(void) // Load Graphics
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nLoad Graphics " + p1 + L", Transparent " + p2 + L"\n";
		return ret;
	}

	std::wstring command_Q(void)
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nSave Playdata " + p1 + L"\n";
		return ret;
	}

	std::wstring command_L(void)
	{
		auto p1 = std::to_wstring(this->get_byte());
		std::wstring ret = L"\nLoad Playdata " + p1 + L"\n";
		return ret;
	}

	std::wstring command_Y(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret = L"\nExtra1 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

	std::wstring command_Z(void)
	{
		std::wstring p1 = CALI();
		std::wstring p2 = CALI();
		std::wstring ret;

		ret = L"\nExtra2 " + p1 + L", " + p2 + L"\n";
		return ret;
	}

public:
	std::wstring inline CALI(void)
	{
		return this->CALI7();
	}
};
#endif
