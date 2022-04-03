#include "stdtype.h"
#include "VGMFile.h"

class VGMHEADER : public VGM_HEADER {
public:
	VGMHEADER(void)
	{
		fccVGM = FCC_VGM;
		lngVersion = 0x171;
		lngEOFOffset = lngHzPSG = lngHzYM2413 = lngGD3Offset = lngTotalSamples = lngLoopOffset = lngLoopSamples = lngRate =
			lngHzYM2612 = lngHzYM2151 = lngDataOffset = lngHzSPCM = lngSPCMIntf = lngHzRF5C68 = lngHzYM2203 = lngHzYM2608 = lngHzYM2610 =
			lngHzYM3812 = lngHzYM3526 = lngHzY8950 = lngHzYMF262 = lngHzYMF278B = lngHzYMF271 = lngHzYMZ280B = lngHzRF5C164 =
			lngHzPWM = lngHzAY8910 = lngHzGBDMG = lngHzNESAPU = lngHzMultiPCM = lngHzUPD7759 = lngHzOKIM6258 = lngHzOKIM6295 =
			lngHzK051649 = lngHzK054539 = lngHzHuC6280 = lngHzC140 = lngHzK053260 = lngHzPokey = lngHzQSound = lngHzSCSP =
			lngExtraOffset = lngHzWSwan = lngHzVSU = lngHzSAA1099 = lngHzES5503 = lngHzES5506 = lngHzX1_010 = lngHzC352 = lngHzGA20 = 0;
		shtPSG_Feedback = 0;
		bytPSG_SRWidth = bytPSG_Flags = bytAYType = bytAYFlag = bytAYFlagYM2203 = bytAYFlagYM2608 = bytVolumeModifier = bytReserved2 =
			bytLoopBase = bytLoopModifier = bytOKI6258Flags = bytK054539Flags = bytC140Type = bytReservedFlags = bytES5503Chns = bytES5506Chns =
			bytC352ClkDiv = bytESReserved = 0;

	}
};