#ifndef TOPNG_DAT
#define TOPNG_DAT

#pragma pack(push)
#pragma pack(1)
class DAT {
	size_t entries = 0;
	unsigned __int16* Addr = nullptr;

	struct LINKMAP {
		unsigned __int8 ArchiveID;
		unsigned __int8 FileNo;
	} *linkmap = nullptr;

public:
	std::vector<size_t> offsets;
	std::vector<size_t> lengths;
	std::vector<size_t> files;

	bool init(std::vector<__int8>& buffer)
	{
		if (buffer.size() < 0x200) {
			return true;
		}

		this->Addr = (unsigned __int16*)&buffer.at(0);

		if (this->Addr == 0) {
			return true;
		}

		size_t index = 0;
		while (*(this->Addr + index++) != 0);

		size_t l = (index / 128 + (index % 128 ? 1 : 0))* 128;

		for (size_t i = index; i < l; i++) {
			if (*(this->Addr + index++) != 0) {
				return true;
			}
		}

		for (size_t i = 0; *(this->Addr + i) != 0; i++) {
			if (*(this->Addr + i + 1) != 0 && *(this->Addr + i) >= *(this->Addr + i + 1)) {
				return true;
			}

			size_t offset = 0x100ULL * (*(this->Addr + i) - 1);

			if (offset == buffer.size()) {
				break;
			}

			else if (offset > buffer.size()) {
				return true;
			}

			this->offsets.push_back(offset);

			if (*(this->Addr + i + 1) == 0) {
				this->lengths.push_back(buffer.size() - offset);
			}
			else {
				this->lengths.push_back(0x100ULL * (*(this->Addr + i + 1) - *(this->Addr + i)));
			}
			entries++;
		}

		this->linkmap = (struct LINKMAP*)(&buffer.at(0) + this->offsets.at(0));

		unsigned __int8 prevFileNo = 0;
		for (size_t i = 0; (this->linkmap + i)->ArchiveID != 0x1A; i++) {
			if ((this->linkmap + i)->ArchiveID == 0x63) {
				continue;
			}
			else if ((this->linkmap + i)->ArchiveID >= 0x1B || (this->linkmap + i)->ArchiveID == 0 || (this->linkmap + i)->FileNo == 0) {
				return true;
			}
			else if ((this->linkmap + i)->ArchiveID != 1) { // Only A*.DAT supported.
				continue;
			}

			if (prevFileNo + 1 != (this->linkmap + i)->FileNo) {
				std::wcout << L"Abnormal entry?" << std::endl;
			}
			prevFileNo = (this->linkmap + i)->FileNo;

			this->files.push_back(prevFileNo);
		}


		return false;
	}
};
#pragma pack(pop)
#endif
