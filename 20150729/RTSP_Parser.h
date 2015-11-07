#include <string>
#include "RTSP_Struct.h"
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

typedef struct SDP
{
	string mediaName ;
	string format ;
	unsigned int timestamp = 0;
	string controlPath ;
	SDP *next  ;
}SDP;

class RTSP_SESSION
{
public:
	int status;
	int tcpFd;
	int udpFd;
	string BaseURL;
	int CSeq;
	char *nextLine;
	unsigned int responseCode;
	SDP *sdp;
	string serverIP;
	unsigned int udpPort;
	string sessionID;
//function

	bool setBlock(int fd);
	bool setNonBlock(int fd);
	int init_TCP(string IP, string Port, string);
	//int init_UDP(string IP, unsigned int Port);
	int init_UDP();
	string getSessionID(string str);
	char *getLine(char *startLine, string &str);  //store a line and return next line ptr
	int getResponseCode(char *buf);
	int getOptions();
	int getDescribe();
	int setupSDP();
	int playMedia();
	int readRTP();
	int tearDown();
	int keepAlive();
	bool unpackRTP(void * buf, int len, void **pBufOut, int &pOutLen);
};



enum sdpStatus
{
	getmediaName,
	getformat,
	gettimestamp,
	getcontrolPath,
	getnextmediaName
};