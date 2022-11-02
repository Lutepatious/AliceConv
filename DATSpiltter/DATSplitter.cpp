#include <fstream>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cerrno>

#pragma pack (1)
struct LINKMAP {
	unsigned __int8 ArchiveID;
	unsigned __int8 FileNo;
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L"file ...\n" << std::endl;
		exit(-1);
	}

	bool use_linkmap = false;
	std::vector<__int8> lmbuf;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'l') {
				std::ifstream lmfile(L"linkmap", std::ios::binary);
				if (!lmfile) {
					std::wcerr << L"File linkmap open error." << std::endl;

					continue;
				}

				lmbuf.insert(lmbuf.end(), std::istreambuf_iterator<__int8>(lmfile), std::istreambuf_iterator<__int8>());
				lmfile.close();

				use_linkmap = true;
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

		size_t lmlen = 0;
		size_t entries = 0;

		unsigned __int16* Addr = (unsigned __int16*)&inbuf.at(0);
		for (size_t i = 0; *(Addr + i); i++) {
			if ((size_t)(*(Addr + i) - 1) * 0x100 < inbuf.size()) {
				size_t len;
				if (*(Addr + i + 1)) {
					len = (size_t)(*(Addr + i + 1) - *(Addr + i)) * 0x100;
				}
				else {
					len = inbuf.size() - (size_t)(*(Addr + i) - 1) * 0x100;
				}

				if (i == 0) {
					lmlen = len;
				}
				std::wcout << L"Entry " << std::dec << i << L":" << std::hex << *(Addr + i) - 1 << L":" << std::dec << len << std::endl;
				entries = i + 1;
			}
		}

		struct LINKMAP* linkmap = (struct LINKMAP*)(&inbuf.at(0) + 0x100LL * (*Addr - 1));
		bool have_linkmap = true;
		size_t entries_lm = 0;

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];
		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		unsigned __int8 arc_ID = towupper(fname[0]) - L'A' + 1;

		for (size_t i = 0; (linkmap + i)->ArchiveID != 0x1A; i++) {
			if ((i >= lmlen / sizeof(struct LINKMAP)) || ((linkmap + i)->ArchiveID > 0x1A && (linkmap + i)->ArchiveID != 0x63)) {
				have_linkmap = false;
				break;
			}

			if (arc_ID == (linkmap + i)->ArchiveID) {
				entries_lm = (linkmap + i)->FileNo;
			}
		}


		if (have_linkmap) {
			if (entries_lm != entries) {
				have_linkmap = false;
				std::wcerr << L"File " << *argv << L" has no linkmap for itself." << std::endl;
				_wmakepath_s(path, _MAX_PATH, drive, dir, L"linkmap", NULL);
				std::ofstream outfile(path, std::ios::binary);
				if (!outfile) {
					std::wcerr << L"File open error " << path << L"." << std::endl;
					outfile.close();
					continue;
				}
				outfile.write(&inbuf.at(0) + 0x100LL * (*Addr - 1), lmlen);

				outfile.close();
			}
		}

		std::wcerr << have_linkmap << L"reach." << std::endl;

		size_t entry_offset = 0;
		if (use_linkmap) {
			std::cout << lmbuf.size() << std::endl;
			linkmap = (struct LINKMAP*)&lmbuf.at(0);
			lmlen = lmbuf.size();
			entry_offset = 1;
			have_linkmap = true;
		}

		std::wcerr << have_linkmap << L"reach." << std::endl;


		wchar_t newdir[_MAX_DIR];
		wchar_t newfname[_MAX_FNAME];
		if (have_linkmap) {
			std::wcout << L"With link map.\n" << std::endl;

			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname + 1);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT)
			{
				if (!_wmkdir(path)) {
					std::wcerr << L"Out path" << path << L"created." << std::endl;
				}
			}
			for (size_t i = 0; (linkmap + i)->ArchiveID != 0x1A; i++) {
				if (arc_ID == (linkmap + i)->ArchiveID) {
					size_t wsize;
					if (*(Addr + (linkmap + i)->FileNo + 1 - entry_offset)) {
						wsize = (size_t)(*(Addr + (linkmap + i)->FileNo + 1 - entry_offset) - *(Addr + (linkmap + i)->FileNo - entry_offset)) * 0x100;
					}
					else {
						wsize = inbuf.size() - (size_t)(*(Addr + (linkmap + i)->FileNo - entry_offset) - 1) * 0x100;
					}
					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%04zu%c%03u", i + 1, towupper(*fname), (linkmap + i)->FileNo);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");

						std::ofstream outfile(path, std::ios::binary);
						if (!outfile) {
							std::wcerr << L"File open error " << path << L"." << std::endl;
							outfile.close();
							exit(-1);
						}
						outfile.write(&inbuf.at(0) + (size_t)(*(Addr + (linkmap + i)->FileNo - entry_offset) - 1) * 0x100, wsize);

						outfile.close();
						std::wcout << L"Out size " << wsize << L", name " << path << L"." << std::endl;
					}
				}
			}
		}
		else {
			std::wcout << L"No link map.\n" << std::endl;

			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT) {
				if (!_wmkdir(path)) {
					std::wcerr << L"Out path" << path << L"created." << std::endl;
				}
			}

			for (size_t i = 0; *(Addr + i); i++) {
				if ((size_t)(*(Addr + i) - 1) * 0x100 < inbuf.size()) {
					size_t wsize;
					if (*(Addr + i + 1)) {
						wsize = (size_t)(*(Addr + i + 1) - *(Addr + i)) * 0x100;
					}
					else {
						wsize = inbuf.size() - (size_t)(*(Addr + i) - 1) * 0x100;
					}
					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%s%03zu", fname, i + 1);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");


						std::ofstream outfile(path, std::ios::binary);
						if (!outfile) {
							std::wcerr << L"File open error " << path << L"." << std::endl;
							outfile.close();
							exit(-1);
						}
						outfile.write(&inbuf.at(0) + (size_t)(*(Addr + i) - 1) * 0x100, wsize);

						outfile.close();
						std::wcout << L"Out size " << wsize << L", name " << path << L"." << std::endl;
					}
				}
#if 0

				size_t F_Addr = 0x100LL * *(Addr + i);
				if (F_Addr < fs.st_size) {
					__int16 wblock = *(Addr + i + 1) - *(Addr + i);
					if (wblock < 0) {
						wblock = fs.st_size / 0x100 + 1 - *(Addr + i);
					}
					size_t wsize = 0x100L * (size_t)wblock;
					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%s%03d", fname, i + 1);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");
						wprintf_s(L"Entry %03u: %06zX %10zu bytes, name %s.\n", i, F_Addr, wsize, path);

						ecode = _wfopen_s(&pFo, path, L"wb");
						if (ecode) {
							wprintf_s(L"File open error %s.\n", *argv);
							exit(ecode);
						}

						rcount = fwrite(buffer + F_Addr, 1, wsize, pFo);
						if (rcount != wsize) {
							wprintf_s(L"File write error %s.\n", *argv);
							fclose(pFo);
							exit(-2);
						}

						fclose(pFo);
					}
				}
#endif
			}
		}
#if 0

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];
		wchar_t newdir[_MAX_DIR];
		wchar_t newfname[_MAX_FNAME];
		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		if (have_linkmap) {
			wprintf_s(L"With link map.\n");

			unsigned __int8 arc_ID = towupper(*fname) - L'A' + 1;

			linkmap = buffer + 0x100LL * *Addr;
			i = 0;
			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname + 1);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT)
			{
				_wmkdir(path);
				wprintf_s(L"Out path %s created.\n", path);
			}
			while ((linkmap + i)->ArchiveID != 0x1A) {
				if (arc_ID == (linkmap + i)->ArchiveID) {
					size_t wsize = 0x100L * ((size_t) * (Addr + (linkmap + i)->FileNo + 1) - (size_t) * (Addr + (linkmap + i)->FileNo));

					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%04d%c%03d", i + 1, towupper(*fname), (linkmap + i)->FileNo);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");
						wprintf_s(L"Out size %6zd, name %s.\n", wsize, path);

						ecode = _wfopen_s(&pFo, path, L"wb");
						if (ecode || !pFo) {
							wprintf_s(L"File open error %s.\n", *argv);
							exit(ecode);
						}

						rcount = fwrite(buffer + 0x100LL * *(Addr + (linkmap + i)->FileNo), 1, wsize, pFo);
						if (rcount != wsize) {
							wprintf_s(L"File write error %s.\n", *argv);
							fclose(pFo);
							exit(-2);
						}
						fclose(pFo);
					}
				}
				i++;
			}

		}
		else {
			wprintf_s(L"No link map.\n");
			i = 0;
			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT) {
				_wmkdir(path);
				wprintf_s(L"Out path %s created.\n", path);
			}
			while (*(Addr + i) || i == 0) {
				size_t F_Addr = 0x100LL * *(Addr + i);
				if (F_Addr < fs.st_size) {
					__int16 wblock = *(Addr + i + 1) - *(Addr + i);
					if (wblock < 0) {
						wblock = fs.st_size / 0x100 + 1 - *(Addr + i);
					}
					size_t wsize = 0x100L * (size_t)wblock;
					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%s%03d", fname, i + 1);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");
						wprintf_s(L"Entry %03u: %06zX %10zu bytes, name %s.\n", i, F_Addr, wsize, path);

						ecode = _wfopen_s(&pFo, path, L"wb");
						if (ecode) {
							wprintf_s(L"File open error %s.\n", *argv);
							exit(ecode);
						}

						rcount = fwrite(buffer + F_Addr, 1, wsize, pFo);
						if (rcount != wsize) {
							wprintf_s(L"File write error %s.\n", *argv);
							fclose(pFo);
							exit(-2);
						}

						fclose(pFo);
					}
				}
				i++;
			}

		}

#endif
	}
}
