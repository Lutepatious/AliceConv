#ifndef TOPNG_TXT
#define TOPNG_TXT
#pragma pack(push)
#pragma pack(1)
class TXT {

public:
	bool init(std::vector<__int8>& buffer)
	{
		setlocale(LC_ALL, "Japanese_Japan");

		std::cout << (char *)&buffer.at(0) << std::endl;
		return false;
	}
};
#pragma pack(pop)
#endif
