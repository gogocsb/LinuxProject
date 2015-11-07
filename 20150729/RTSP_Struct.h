#include <stdio.h>

typedef struct SESSION
{
	char * fMediumName;
	char * fCodecName;
	char * fControlPath;// "a=control:<control-path>" 
	char * fConfig;
	unsigned short fClientPortNum;
	unsigned fNumChannels;
	unsigned frtpTimestampFrequency;
	unsigned char fRTPPayloadFormat;
	char * fSessionId;
	char *fFileName;
	FILE* fFid;
	SESSION *fNext;
}SESSION;


typedef struct MEDIAATTRIBUTE
{
	unsigned fVideoFrequency;
	unsigned char fVideoPayloadFormat;
	char fConfigAsc[200];
	unsigned char fConfigHex[100];
	int fConfigHexLen;
	unsigned fVideoFrameRate;
	unsigned fTimeIncBits;
	unsigned fixed_vop_rate;
	unsigned fVideoWidth;
	unsigned fVideoHeight;
	unsigned fAudioFrequency;
	unsigned char fAudioPayloadFormat;
	unsigned fTrackNum;
} MEDIAATTRIBUTE;

typedef struct XVID_DEC_PARAM
{
	unsigned int width;
	unsigned int height;
	unsigned int time_inc_bits;
	unsigned int framerate;
}XVID_DEC_PARAM;

typedef struct
{
	unsigned int bufa;
	unsigned int bufb;
	unsigned int buf;
	unsigned int pos;
	unsigned int *tail;
	unsigned int *start;
	unsigned int length;
}Bitstream;

typedef struct RESULTDATA
{
	unsigned char *buffer;
	int len;
	unsigned char fRTPPayloadFormat;
	unsigned long frtpTimestamp;
	int frtpMarkerBit;
} RESULTDATA;


/*	0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+ -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| V = 2 | P | X | CC | M | PT | sequence number |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| timestamp |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| synchronization source(SSRC) identifier |
+= += += += += += += += += += += += += += += += += += += += += += += += += += += += += += += += +
| contributing source(CSRC) identifiers |
| .... |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-*/
struct RTP_HEADER
{
	unsigned char cc : 4;
	unsigned char x : 1;
	unsigned char p : 1;
	unsigned char v : 2;

	unsigned char pt : 7;
	unsigned char m : 1;

	unsigned short sequence_number;
	unsigned int timestamp;
	unsigned int ssrc;
};


