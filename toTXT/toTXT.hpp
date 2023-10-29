﻿#ifndef TOTXT_TOTXT
#define TOTXT_TOTXT
#include <locale>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
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

	// Shift-JISエンコードでのカタカナ→ひらがな復元テーブル
	const wchar_t* X0201kana_table = L" 。「」、・をぁぃぅぇぉゃゅょっーあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん゛゜";

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
		if (this->encoding_MSX && *(unsigned __int32*)this->src == 0x1A) {
			return true;
		}
		else if (!this->encoding_MSX && *(unsigned __int32*)this->src == 0) {
			return true;
		}
		else if (!this->encoding_MSX && *(unsigned __int32*)this->src == 0x02020102) {
			return true;
		}
		else if (!this->encoding_MSX && *this->src == 0x1A) {
			return true;
		}
		return false;
	}

	virtual unsigned __int16 get_16(void) = 0;
	virtual unsigned __int16 VL_Value(void) = 0;
	virtual std::wstring CALI(void) = 0;
	virtual std::wstring command_Q(void) = 0; // Save Playdata
	virtual std::wstring command_L(void) = 0; // Load Playdata
	virtual std::wstring command_P(void) = 0; // Change Text Color
	virtual std::wstring command_Y(void) = 0; // Extra1
	virtual std::wstring command_Z(void) = 0; // Extra2

protected:
	unsigned __int8* src = nullptr;
	unsigned __int8* src_start = nullptr;
	unsigned __int8* src_end = nullptr;
	wchar_t printf_buf[1000] = { 0 };
	wchar_t printf_buf_len = 1000;
	const wchar_t* text_color[8] = { L"(Black)", L"(Blue)", L"(Red)", L"(Magenta)", L"(Green)", L"(Cyan)", L"(Yellow)", L"(White)" };

	unsigned __int16 get_byte(void)
	{
		return *++this->src;
	}

	unsigned __int16 get_word(void)
	{
		unsigned __int16 ret = *(unsigned __int16*)(++this->src);
		this->src++;

		return ret;
	}

public:
	void init(std::vector<__int8>& s, bool enc = false)
	{
		this->src_start = this->src = (unsigned __int8*)&*s.begin();
		this->src_end = (unsigned __int8*)&*s.end();
		this->encoding_MSX = enc;
	}

	std::wstring decode(void)
	{
		std::vector<std::pair<unsigned __int16, std::wstring>> decoded_commands;
		std::pair<unsigned __int16, std::wstring> decoded_command;
		int nest = 0;
		bool set_menu = false;

		_wsetlocale(LC_ALL, L"ja_JP");
		_locale_t loc_jp = _create_locale(LC_CTYPE, "ja_JP");
		std::wstring str;
		std::vector<unsigned __int16> Labels;
		unsigned __int16 Val = this->get_16();
		swprintf_s(this->printf_buf, this->printf_buf_len, L"Condition 0x%04X\n", Val);

		decoded_command.first = 0;
		decoded_command.second = this->printf_buf;
		decoded_commands.push_back(decoded_command);

		while (this->src < this->src_end && !this->is_end()) {
			unsigned __int16 Address = this->src - this->src_start;
			decoded_command.first = Address;
			decoded_command.second.empty();

			// Output Characters
			if (this->encoding_MSX && this->MSX_char_table[*this->src] != 0) {
				decoded_command.second = this->MSX_char_table[*this->src];
			}
			else if (*this->src == 0x20) {
				decoded_command.second = *this->src;
			}
			else if (_ismbbkana_l(*this->src, loc_jp)) {
				decoded_command.second = this->X0201kana_table[*this->src - 0xA0];
			}
			else if (_ismbblead_l(*this->src, loc_jp) && _ismbbtrail_l(*(this->src + 1), loc_jp)) {
				wchar_t tmp;
				int ret = _mbtowc_l(&tmp, (const char*)this->src, 2, loc_jp);
				this->src++;

				if (ret != 2) {
					std::wcerr << L"character convert failed." << std::endl;
				}
				decoded_command.second = tmp;
			}

			// Output Texts
			else if (*this->src == 'R') {
				decoded_command.second = L"\n";
			}
			else if (*this->src == 'A') {
				decoded_command.second = L"\nHit any Key.\n";
			}


			else if (*this->src == 'F') {
				decoded_command.second = L"\nReturn to top.\n";
			}
			else if (*this->src == ']') {
				decoded_command.second = L"\nOpen Menu.\n";
			}

			else if (*this->src == 'G') {
				auto p1 = std::to_wstring(this->get_byte());
				decoded_command.second = L"\nLoad Graphics " + p1 + L"\n";
			}
			else if (*this->src == 'U') {
				auto p1 = std::to_wstring(this->get_byte());
				auto p2 = std::to_wstring(this->get_byte());
				decoded_command.second = L"\nLoad Graphics " + p1 + L", Transparent " + p2 + L"\n";
			}
			else if (*this->src == 'S') {
				auto p1 = std::to_wstring(this->get_byte());
				decoded_command.second = L"\nLoad Sound " + p1 + L"\n";
			}
			else if (*this->src == 'X') {
				auto p1 = std::to_wstring(this->get_byte());
				decoded_command.second = L"(Print $" + p1 + L")";
			}

			else if (*src == '!') {
				auto p1 = std::to_wstring(this->VL_Value());
				auto p2 = this->CALI();
				decoded_command.second = L"\nVar" + p1 + L" = " + p2 + L"\n";
			}
			else if (*this->src == '[') {
				auto p1 = this->get_byte();
				auto p2 = this->get_byte();
				auto Addr = this->get_word();
				Labels.push_back(Addr);

				swprintf_s(this->printf_buf, this->printf_buf_len, L"\nLabel%04X %u, %u\n", Addr, p1, p2);
				decoded_command.second = this->printf_buf;
			}
			else if (*src == ':') {
				std::wstring A = this->CALI();
				auto p2 = this->get_byte();
				auto p3 = this->get_byte();
				auto Addr = this->get_word();
				Labels.push_back(Addr);

				swprintf_s(this->printf_buf, this->printf_buf_len, L"\n%s Label%04X %u, %u\n", A.c_str(), Addr, p2, p3);
				decoded_command.second = this->printf_buf;
			}
			else if (*this->src == '&') {
				std::wstring A = this->CALI();
				decoded_command.second = L"\nJump to page " + A + L".\n";
			}
			else if (*this->src == '{') {
				nest++;
				std::wstring A = this->CALI();
				decoded_command.second = L"\n" + A + L" {\n";
			}
			else if (*this->src == '}') {
				if (nest) {
					nest--;
				}
				decoded_command.second = L"\n}\n";
			}
			else if (*this->src == '@') {
				int Addr = this->get_word();
				Labels.push_back(Addr);

				swprintf_s(this->printf_buf, this->printf_buf_len, L"\nJump to Label%04X\n", Addr);
				decoded_command.second = this->printf_buf;
			}
			else if (*this->src == '$') {
				if (set_menu) {
					set_menu = false;
				}
				else {
					int Addr = this->get_word();
					Labels.push_back(Addr);

					swprintf_s(this->printf_buf, this->printf_buf_len, L"\nLabel%04X ", Addr);
					decoded_command.second = this->printf_buf;
					set_menu = true;
				}
			}

			// System Dependent Functions
			else if (*this->src == 'Q') { // Save Playdata
				decoded_command.second = this->command_Q();
			}
			else if (*this->src == 'L') { // Load Playdata
				decoded_command.second = this->command_L();
			}
			else if (*this->src == 'P') { // Change Text Color
				decoded_command.second = this->command_P();
			}
			else if (*this->src == 'Y') { // Extra1
				decoded_command.second = this->command_Y();
			}
			else if (*this->src == 'Z') { // Extra2
				decoded_command.second = this->command_Z();
			}
			else {
				swprintf_s(this->printf_buf, this->printf_buf_len, L"\nUnknown at %04X:%02X %02X %02X %02X", decoded_command.first, *this->src, *(this->src + 1), *(this->src + 2), *(this->src + 3));
				std::wcout << decoded_command.second;
			}
			decoded_commands.push_back(decoded_command);
			this->src++;
		}

		for (auto& i : decoded_commands) {
			bool Label = false;
			for (auto& j : Labels) {
				if (i.first == j && !Label) {
					swprintf_s(this->printf_buf, this->printf_buf_len, L"\nLabel%04X:\n", j);
					str += this->printf_buf;
					Label = true;
				}
			}

			str += i.second;
		}


		return CleanUpString(str);
	}
};

class toTXT0 : public toTXT {
	unsigned __int16 get_16(void)
	{
		return 0;
	}

	unsigned __int16 VL_Value(void)
	{
		return *++this->src;
	}

	std::wstring CALI(void)
	{
		// import from T.T sys32
		std::vector<std::wstring> mes;
		while (1) {
			if (*++this->src & 0x80) {
				unsigned t;
				if ((*this->src & 0x40)) { // 0xC0-0xFF
					t = (*this->src & 0x3F) << 8;
					t += *++this->src;
				}
				else { // 0x80-0xBF
					t = *this->src & 0x3F;
				}
				swprintf_s(this->printf_buf, this->printf_buf_len, L"Var%d", t);
				mes.push_back(this->printf_buf);
			}
			else { // 0x00-0x7F
				if (*this->src < 0x78) {
					swprintf_s(this->printf_buf, this->printf_buf_len, L"%d", *this->src);
					mes.push_back(this->printf_buf);
				}
				else if (*this->src == 0x78) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" *= " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x79) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" += " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7A) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" -= " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7B) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" == " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7C) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" < " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7D) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" > " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7E) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" != " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7F) {
					return *mes.begin();
				}
			}
		}
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

	std::wstring command_P(void)
	{
		// Set text color 0-7 Black, Blue, Red, Magenta, Green, Cyan, Yellow, White
		auto p1 = this->get_byte();
		std::wstring ret = this->text_color[p1];
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
	unsigned __int16 get_16(void)
	{
		unsigned __int16 val = *(unsigned __int16*)(this->src);
		this->src += 2;
		return val;
	}

	unsigned __int16 VL_Value(void)
	{
		unsigned t = *++this->src;

		if ((t & 0x40)) {
			t = (t & 0x3F) << 8;
			t += *++this->src;
		}
		else {
			t &= 0x3F;
		}

		return t;
	}

	std::wstring CALI(void)
	{
		// import from T.T sys32
		std::vector<std::wstring> mes;

		while (1) {
			if ((*++this->src & 0xC0) == 0x80) { // 0x80-0xBF
				swprintf_s(this->printf_buf, this->printf_buf_len, L"Var%d", *this->src & 0x3F);
				mes.push_back(this->printf_buf);
			}
			else if ((*this->src & 0xC0) == 0xC0) { // 0xC0-0xFF
				unsigned t = (*this->src & 0x3F) << 8;
				t += *++this->src;
				swprintf_s(this->printf_buf, this->printf_buf_len, L"Var%d", t);
				mes.push_back(this->printf_buf);
			}
			else if ((*this->src & 0xC0) == 0x00) {
				unsigned t = (*this->src & 0x3F) << 8;
				t += *++this->src;
				swprintf_s(this->printf_buf, this->printf_buf_len, L"%d", t);
				mes.push_back(this->printf_buf);
			}
			else if ((*this->src & 0xC0) == 0x40) { // 0x40-0x7F
				if (*this->src < 0x78) {
					swprintf_s(this->printf_buf, this->printf_buf_len, L"%d", (*this->src & 0x3F));
					mes.push_back(this->printf_buf);
				}
				else if (*this->src == 0x78) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" *= " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x79) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" += " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7A) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" -= " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7B) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" == " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7C) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" < " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7D) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" > " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7E) {
					std::wstring t = L"(" + *(mes.end() - 2) + L" != " + *(mes.end() - 1) + L")";
					mes.pop_back();
					mes.pop_back();
					mes.push_back(t);
				}
				else if (*this->src == 0x7F) {
					return *mes.begin();
				}
			}
		}
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

	std::wstring command_P(void)
	{
		// Set text color 0-7 Black, Blue, Red, Magenta, Green, Cyan, Yellow, White
		auto p1 = this->get_byte();
		std::wstring ret = this->text_color[p1];
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


#endif