#include <cstdio>
#include <cstdlib>

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
