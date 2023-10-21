#ifndef TOTXT_TOTXT
#define TOTXT_TOTXT
#include <string>
#include <vector>
#include <cwchar>
#include <climits>
#include <mbstring.h>

enum class VarName {
	RND = 0,
	D01, D02, D03, D04, D05, D06, D07, D08, D09, D10, D11, D12, D13, D14, D15, D16, D17, D18, D19, D20,
	U01, U02, U03, U04, U05, U06, U07, U08, U09, U10, U11, U12, U13, U14, U15, U16, U17, U18, U19, U20,
	B01, B02, B03, B04, B05, B06, B07, B08, B09, B10, B11, B12, B13, B14, B15, B16, B17, B18, B19, B20,
	M_X, M_Y
};

class toTXT {
	// printfの出力用バッファ
	wchar_t printf_buf[1000] = { 0 };
	wchar_t printf_buf_len = 1000;

	// リトルヴァンパイアMSX版と学園戦記MSX版用文字変換テーブル
	const wchar_t *MSX_char_table = L" !\"#$%&\'()*+,-./0123456789[]<=>?"
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
	const wchar_t *X0201kana_table = L" 。「」、・をぁぃぅぇぉゃゅょっーあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん゛゜";

	// 清音濁音変換テーブル
	const wchar_t *seion = L"うかきくけこさしすせそたちつてとはひふへほウカキクケコサシスセソタチツテトハヒフヘホ";
	const wchar_t *dakuon = L"ゔがぎぐげござじずぜぞだぢづでどばびぶべぼヴガギグゲゴザジズゼゾダヂヅデドバビブベボ";

	// 清音半濁音変換テーブル
	const wchar_t *seion2 = L"はひふへほハヒフヘホ";
	const wchar_t *handakuon = L"ぱぴぷぺぽパピプペポ";

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
	std::wstring CleanUpString(std::wstring &in)
	{
		// 清音字+濁点を濁音字へ
		for (size_t i = 0; i < 42; i++) {
			std::wstring f{ seion[i] };
			std::wstring t{ dakuon[i] };
			f += L"゛";
			in = ReplaceString(in, f, t);
		}

		// 清音字+半濁点を半濁音字へ
		for (size_t i = 0; i < 10; i++) {
			std::wstring f{ seion2[i] };
			std::wstring t{ handakuon[i] };
			f += L"゜";
			in = ReplaceString(in, f, t);
		}

		// 連続する改行をまとめる
		in = ReplaceString(in, L"\n\n", L"\n");

		return in;
	}

	virtual unsigned __int16 VVal(unsigned __int8** in)
	{
		return 0;
	}

	virtual std::wstring CALI(unsigned __int8** in)
	{
		std::wstring r;

		return r;
	}

};


#endif
