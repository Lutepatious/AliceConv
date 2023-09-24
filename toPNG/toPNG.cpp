#include "toPNG.hpp"
#pragma pack(push)
#pragma pack(1)
class X68K_TT { // 闘神都市X68000版のGRAPHICSモードで使われる256色用フォーマット。
	struct format_X68K_TT {
		unsigned __int32 Sig0BE; // 0x00000011 固定
		unsigned __int32 Sig1BE; // 0x00000000 固定
		unsigned __int32 Sig2BE; // 0x00010007 固定
		unsigned __int16 Pal5BE[0xC0]; // Palette No. 0x40 to 0xFF 0x00から0x3FはテキストモードやPCGのパレットと共用だが利用例なし。
		unsigned __int16 Overlay;
		unsigned __int16 StartBE;
		unsigned __int16 len_x_BE;
		unsigned __int16 len_y_BE;
		unsigned __int8 body[];
	} *buf = nullptr;

	size_t len_buf = 0;
	unsigned transparent = 0;

public:
	png_uint_32 len_x = X68000_G;
	png_uint_32 len_y = X68000_G;
	png_int_32 offset_x = 0;
	png_int_32 offset_y = 0;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < sizeof(format_X68K_TT)) {
			std::wcerr << "File too short." << std::endl;
			return true;
		}

		this->buf = (format_X68K_TT*)&buffer.at(0);
		this->len_buf = buffer.size();

		if (_byteswap_ulong(this->buf->Sig0BE) != 0x11 || _byteswap_ulong(this->buf->Sig1BE) != 0 || _byteswap_ulong(this->buf->Sig2BE) != 0x00010007) {
			std::wcerr << "Wrong Signature." << std::endl;
			return true;
		}

		unsigned __int16 start = _byteswap_ushort(this->buf->StartBE);
		this->offset_y = start / X68000_G;
		this->offset_x = start % X68000_G;
		this->len_x = _byteswap_ushort(this->buf->len_x_BE);
		this->len_y = _byteswap_ushort(this->buf->len_y_BE);

		if (this->len_x > X68000_G || this->len_y > X68000_G) {
			std::wcerr << "Wrong size." << std::endl;
			return true;
		}

		std::wcout << L"From " << std::setw(4) << this->offset_x << L"," << std::setw(3) << this->offset_y << L" Size " << std::setw(4) << len_x << L"," << std::setw(3) << len_y << std::endl;
		return false;
	}

	void decode_palette(std::vector<png_color>& pal, std::vector<png_byte>& trans)
	{
		union X68Pal_conv {
			struct Palette_depth5 Pal5;
			unsigned __int16 Pin;
		} P;

		png_color c;

		c.red = 0;
		c.green = 0;
		c.blue = 0;
		pal.insert(pal.end(), 0x40, c);
		trans.insert(trans.end(), 0x40, 0xFF);
		for (size_t i = 0; i < 0xC0; i++) {
			P.Pin = _byteswap_ushort(buf->Pal5BE[i]);

			c.red = d5tod8(P.Pal5.R);
			c.green = d5tod8(P.Pal5.G);
			c.blue = d5tod8(P.Pal5.B);
			pal.push_back(c);
			trans.push_back(0xFF);
		}

		trans.at(this->transparent) = 0;
	}

	void decode_body(std::vector<png_bytep>& out_body)
	{
		const size_t image_size = (size_t)this->len_x * this->len_y;
		unsigned __int8* src = this->buf->body;
		size_t count = this->len_buf - sizeof(format_X68K_TT);

		std::vector<unsigned __int8> D;
		while (count-- && D.size() < image_size) {
			switch (*src) {
			case 0x00:
				for (size_t len = 0; len < 1ULL + *(src + 2); len++) {
					D.push_back(*(src + 1));
				}
				src += 3;
				count -= 2;
				break;
			case 0x01:
				for (size_t len = 0; len < 1ULL + *(src + 3); len++) {
					D.push_back(*(src + 1));
					D.push_back(*(src + 2));
				}
				src += 4;
				count -= 3;
				break;
			default:
				D.push_back(*src);
			}
		}
	}

};

#pragma pack(pop)

enum class decode_mode {
	NONE = 0, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS8, PMS16, QNT, X68R, X68T, X68V, TIFF_TOWNS, DRS_CG003, DRS_CG003_TOWNS, DRS_OPENING_TOWNS, SPRITE_X68K, MASK_X68K
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
		if (**++argv == L'-') {
			// already used: sSOYPMghRv

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
//			x68t.decode_body(out.body);
			out.change_resolution_x68k_g();
			out.set_size(X68000_G, X68000_G);
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
