#pragma pack(1)
struct AC_FM_PARAMETER {
	unsigned __int8 Connect : 3; // CON Connection
	unsigned __int8 FB : 3; // FL Self-FeedBack
	unsigned __int8 L : 1; // YM2151 Only
	unsigned __int8 R : 1; // YM2151 Only
	unsigned __int8 OPR_MASK : 4;
	unsigned __int8 : 4;
	struct {
		unsigned __int8 MULTI : 4; // MUL Multiply
		unsigned __int8 DT : 3; // DT1 DeTune
		unsigned __int8 : 1; // Not used
		unsigned __int8 TL : 7; // TL Total Level
		unsigned __int8 : 1; // Not used
		unsigned __int8 AR : 5; // AR Attack Rate
		unsigned __int8 : 1; // Not used
		unsigned __int8 KS : 2; // KS Key Scale
		unsigned __int8 DR : 5; // D1R Decay Rate
		unsigned __int8 : 2; // Not used
		unsigned __int8 AMON : 1; // AMS-EN AMS On
		unsigned __int8 SR : 5; // D2R Sustain Rate
		unsigned __int8 : 1; // Not used
		unsigned __int8 DT2 : 2; // DT2
		unsigned __int8 RR : 4; // RR Release Rate
		unsigned __int8 SL : 4; // D1L Sustain Level
	} Op[4];
};

#define LEN_AC_FM_PARAMETER (sizeof(struct AC_FM_PARAMETER))

struct AC_FM_PARAMETER_BYTE {
	unsigned __int8 FB_CON;       // CON Connection, FL Self-FeedBack
	unsigned __int8 OPMASK;       // Operator Mask
	struct {
		unsigned __int8 DT_MULTI; // MUL Multiply,  DT1 DeTune
		unsigned __int8 TL;       // TL Total Level
		unsigned __int8 KS_AR;    // AR Attack Rate, KS Key Scale
		unsigned __int8 AM_DR;    // D1R Decay Rate, AMS-EN AMS On
		unsigned __int8 DT2_SR;   // D2R Sustain Rate,  DT2
		unsigned __int8 SL_RR;    // RR Release Rate, D1L Sustain Level
	} Op[4];
};

#define LEN_AC_FM_PARAMETER_BYTE (sizeof(struct AC_FM_PARAMETER_BYTE))

union AC_Tone {
	struct AC_FM_PARAMETER S;
	struct AC_FM_PARAMETER_BYTE B;
};


struct AC_FM_PARAMETER_x68 {
	unsigned __int8 Connect : 3; // CON Connection
	unsigned __int8 FB : 3; // FL Self-FeedBack
	unsigned __int8 L : 1; // YM2151 Only
	unsigned __int8 R : 1; // YM2151 Only
	unsigned __int8 OPR_MASK : 4;
	unsigned __int8 : 4;
	unsigned __int8 AMS : 2; // A-mod Sense
	unsigned __int8 : 2;
	unsigned __int8 PMS : 3; // P-mod Sense
	unsigned __int8 : 1;
	unsigned __int8 LFRQ;   // LFRQ
	unsigned __int8 PMD : 7; // P-mod Depth
	unsigned __int8 Pflag : 1; // must be 1
	unsigned __int8 AMD : 7; // A-mod Depth
	unsigned __int8 Aflag : 1; // must be 0
	unsigned __int8 W : 2; // Waveform
	unsigned __int8 : 4;
	unsigned __int8 CT1 : 1; // Out1
	unsigned __int8 CT2 : 1; // Out2


	struct {
		unsigned __int8 MULTI : 4; // MUL Multiply
		unsigned __int8 DT : 3; // DT1 DeTune
		unsigned __int8 : 1; // Not used
		unsigned __int8 TL : 7; // TL Total Level
		unsigned __int8 : 1; // Not used
		unsigned __int8 AR : 5; // AR Attack Rate
		unsigned __int8 : 1; // Not used
		unsigned __int8 KS : 2; // KS Key Scale
		unsigned __int8 DR : 5; // D1R Decay Rate
		unsigned __int8 : 2; // Not used
		unsigned __int8 AMON : 1; // AMS-EN AMS On
		unsigned __int8 SR : 5; // D2R Sustain Rate
		unsigned __int8 : 1; // Not used
		unsigned __int8 DT2 : 2; // DT2
		unsigned __int8 RR : 4; // RR Release Rate
		unsigned __int8 SL : 4; // D1L Sustain Level
	} Op[4];
};

#define LEN_AC_FM_PARAMETER_x68 (sizeof(struct AC_FM_PARAMETER_x68))

struct AC_FM_PARAMETER_BYTE_x68 {
	unsigned __int8 FB_CON;       // CON Connection, FL Self-FeedBack
	unsigned __int8 OPMASK;       // Operator Mask
	unsigned __int8 PMS_AMS;      // PMS, AMS
	unsigned __int8 LFRQ;   // LFRQ
	unsigned __int8 PMD;   // PMD | 0x80
	unsigned __int8 AMD;   // AMD & ~0x80
	unsigned __int8 CT_WAVE;   // Waveform
	struct {
		unsigned __int8 DT_MULTI; // MUL Multiply,  DT1 DeTune
		unsigned __int8 TL;       // TL Total Level
		unsigned __int8 KS_AR;    // AR Attack Rate, KS Key Scale
		unsigned __int8 AM_DR;    // D1R Decay Rate, AMS-EN AMS On
		unsigned __int8 DT2_SR;   // D2R Sustain Rate,  DT2
		unsigned __int8 SL_RR;    // RR Release Rate, D1L Sustain Level
	} Op[4];
};

#define LEN_AC_FM_PARAMETER_BYTE_x68 (sizeof(struct AC_FM_PARAMETER_BYTE_x68))

union AC_Tone_x68 {
	struct AC_FM_PARAMETER_x68 S;
	struct AC_FM_PARAMETER_BYTE_x68 B;
};

#pragma pack()
