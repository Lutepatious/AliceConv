#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>

#pragma pack (1)
struct LINKMAP {
	unsigned __int8 ArchiveID;
	unsigned __int8 FileNo;
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L"file ..." << std::endl;
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
				lmbuf.clear();
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

		size_t j = 0;
		do 
		{
			unsigned __int16* target = Addr + j;
			if (*target == 0) {
				continue;
			}

			if ((size_t)(*target - 1) * 0x100 < inbuf.size()) {
				size_t len;
				if (*(target + 1)) {
					len = (size_t)(*(target + 1) - *target) * 0x100;
				}
				else {
					len = inbuf.size() - (size_t)(*target - 1) * 0x100;
				}

				if (j == 0) {
					lmlen = len;
				}
				std::wcout << L"Entry " << std::dec << j << L":" << std::hex << *target - 1 << L":" << std::dec << len << std::endl;
				entries = j;
			}
		} while (*(Addr + ++j));

		struct LINKMAP* linkmap = nullptr;

		if (*Addr) {
			linkmap = (struct LINKMAP*)(&inbuf.at(0) + (size_t)(*Addr - 1) * 0x100);
		}

		bool have_linkmap = true;
		bool have_linkmap_other = false;
		size_t entries_lm = 0;

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];
		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		unsigned __int8 arc_ID = towupper(fname[0]) - L'A' + 1;

		if (linkmap == nullptr || linkmap->ArchiveID == 0) {
			have_linkmap = false;
		}
		else {
			for (size_t i = 0; (linkmap + i)->ArchiveID != 0x1A && (linkmap + i)->ArchiveID; i++) {
				if ((i >= lmlen / sizeof(struct LINKMAP)) || ((linkmap + i)->ArchiveID > 0x1A && (linkmap + i)->ArchiveID != 0x63)) {
					have_linkmap = false;
					break;
				}

				if (arc_ID == (linkmap + i)->ArchiveID) {
					size_t index = (linkmap + i)->FileNo;
					unsigned short* target = Addr + index;
					if ((*target - 1) * 0x100LL < inbuf.size())
					{
						entries_lm = std::max(entries_lm, (size_t)(linkmap + i)->FileNo);
					}
				}
			}
		}

		if (have_linkmap) {
			std::wcout << entries_lm << L"/" << entries << std::endl;
			if (entries_lm != entries && entries_lm != entries - 1) {
				have_linkmap = false;
				have_linkmap_other = true;
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

		size_t entry_offset = 0;
		if (use_linkmap) {
			linkmap = (struct LINKMAP*)&lmbuf.at(0);
			lmlen = lmbuf.size();
			entry_offset = 1;
			have_linkmap = true;
		}

		wchar_t newdir[_MAX_DIR];
		wchar_t newfname[_MAX_FNAME];
		if (have_linkmap) {
			std::wcout << L"With link map." << std::endl;

			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname + 1);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT)
			{
				if (!_wmkdir(path)) {
					std::wcerr << L"Out path " << path << L" created." << std::endl;
				}
			}
			for (size_t i = 0; (linkmap + i)->ArchiveID != 0x1A && (linkmap + i)->ArchiveID; i++) {
				if (arc_ID == (linkmap + i)->ArchiveID) {
					size_t wsize;
					size_t index = (linkmap + i)->FileNo;
					unsigned short *target = Addr + index - entry_offset;
					if (*(target + 1)) {
						wsize = (size_t)(*(target + 1) - *target) * 0x100;
					}
					else {
						wsize = inbuf.size() - (size_t)(*target - 1) * 0x100;
					}
					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%04zu%c%03zu", i + 1, towupper(*fname), index);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");

						std::ofstream outfile(path, std::ios::binary);
						if (!outfile) {
							std::wcerr << L"File open error " << path << L"." << std::endl;
							outfile.close();
							exit(-1);
						}
						outfile.write(&inbuf.at(0) + (size_t)(*target - 1) * 0x100, wsize);

						outfile.close();
						std::wcout << L"Out size " << wsize << L", name " << path << L"." << std::endl;
					}
				}
			}
		}
		else {
			std::wcout << L"No link map." << std::endl;

			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT) {
				if (!_wmkdir(path)) {
					std::wcerr << L"Out path " << path << L" created." << std::endl;
				}
			}

			size_t j = 0;
			do {
				unsigned short* target = Addr + j;
				if (*target == 0) {
					entry_offset = 1;
					continue;
				}

				if ((size_t)(*target - 1) * 0x100 < inbuf.size()) {
					size_t wsize;
					if (*(target + 1)) {
						wsize = (size_t)(*(target + 1) - *target) * 0x100;
					}
					else {
						wsize = inbuf.size() - (size_t)(*target - 1) * 0x100;
					}
					if (wsize) {
						size_t entry_no = j + 1 - entry_offset;
						if (have_linkmap_other) {
							entry_no--;
						}
						swprintf_s(newfname, _MAX_FNAME, L"%s%03zu", fname, entry_no);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");


						std::ofstream outfile(path, std::ios::binary);
						if (!outfile) {
							std::wcerr << L"File open error " << path << L"." << std::endl;
							outfile.close();
							exit(-1);
						}
						outfile.write(&inbuf.at(0) + (size_t)(*target - 1) * 0x100, wsize);

						outfile.close();
						std::wcout << L"Out size " << wsize << L", name " << path << L"." << std::endl;
					}
				}
			} while (*(Addr + ++j));
		}
	}
}
