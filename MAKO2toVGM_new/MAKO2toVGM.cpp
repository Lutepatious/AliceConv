#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <limits>

enum class CHIP { NONE = 0, YM2203, YM2608, YM2151 };

#pragma pack (1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[];
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
}
