#include <stdlib.h>

wchar_t* filename_replace_ext(wchar_t* outfilename, const wchar_t* newext)
{
	static wchar_t path[_MAX_PATH];
	wchar_t fname[_MAX_FNAME];
	wchar_t dir[_MAX_DIR];
	wchar_t drive[_MAX_DRIVE];
	_wsplitpath_s(outfilename, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
	_wmakepath_s(path, _MAX_PATH, drive, dir, fname, newext);

	return path;
}

size_t LCM(size_t a, size_t b)
{
	size_t L, S;
	if (a == b) {
		return a;
	}
	else if (a > b) {
		L = a;
		S = b;
	}
	else {
		L = b;
		S = a;
	}
	while (S != 0) {
		size_t mod = L % S;
		L = S;
		S = mod;
	}
	return a * b / L;
}
