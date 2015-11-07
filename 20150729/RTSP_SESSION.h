#include <string>
#include "RTSP_Struct.h"
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;
class RTSP_SESSION
{
public:
	int status;
	unsigned z_totalsize;
	unsigned needsize;
	unsigned int  responseCode;
	void Keep_alive_connect();
	int init_RTP(string SerIP, unsigned int  nPort, string Source);
	int handleRead(unsigned char* buffer, int Size, int type);
	int RTP_ReadHandler(unsigned char* buf, int zMax, int type);
	int playMediaSession(int start, int end);
	int playMediaSessionUDP(int start, int end);
	int parseResponseCode(char* line, unsigned int  *presponseCode);
	char * getLine(char* startOfLine);
	char fBaseURL[200];
	char *fLastSessionId;
	int setupMediaSubsession(SESSION* pCur);
	void setupStreams();
	unsigned long VFrameRate;
	void BitstreamInit(Bitstream * bs, unsigned int length);
	int decoder_find_vol(unsigned int length, XVID_DEC_PARAM * param);
	unsigned int *bitstream;
	int parse_vovod(unsigned char *ConfigHex, unsigned int HexLen, unsigned *FrameRate, unsigned *TimeIncBits, unsigned *Width, unsigned *Height);
	int a_hex(char *a, unsigned char *hex, unsigned char len);
	MEDIAATTRIBUTE * GetMediaAttrbute(MEDIAATTRIBUTE *Attribute);
	char * parseSDPAttribute_rtpmap(char* sdpLine, SESSION *pCur);
	SESSION *Session;
	int initializeWithSDP(char* sdpDescription);
	int getSDPDescription(char *responseBuffer);
	int fCSeq;
	int SockNum;
	char Errtxt[200];
	int ConnectToSerevr(string serverIP, unsigned int serverPort);
	unsigned int nPort;
	char SerIP[100];
	char responseBuffer[1024 * 10];
	bool setNonBlock(int);
	bool setBlock(int);
	const char *getSessionID(char *buf);
	//udp
	int init_UDP(string ip, unsigned int port);
};