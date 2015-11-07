#include "RTSP_Parser.h"
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <iostream>
#include <stdlib.h>
#include <vector>

using namespace std;
string UserHeaderstr = "LIVE666 (LIVE STREAM MEDIA v6666)";
unsigned int RTP_HEADLEN = 12;
unsigned int clientPort = 61488;

char optFmt[] = "OPTIONS %s RTSP/1.0\r\n"                         //get options
"CSeq: %d\r\n"
"User-Agent: %s\r\n\r\n";

char desFmt[] = "DESCRIBE %s RTSP/1.0\r\n"                        //get sdp
"CSeq: %d\r\n"
"User-Agent: %s\r\n"
"Accept: application/sdp\r\n\r\n";

char setFmt[] = "SETUP %s RTSP/1.0\r\n"                          //setup sdp   then use udp socket get data 
"CSeq: %d\r\n"
"User-Agent: %s\r\n"
"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n";

char playFmt[] = "PLAY %s RTSP/1.0\r\n"                          //play  then get rtp packet
"CSeq: %d\r\n"
"User-Agent: %s\r\n"
"Session: %s\r\n"
"Range: npt=0.00-\r\n\r\n";

char getFmt[] = "GET_PARAMETER %s RTSP/1.0\r\n"                  //keep connection alive
"CSeq: %d\r\n"
"User-Agent: %s\r\n"
"Session: %s\r\n\r\n";

char stopFmt[] = "TEARDOWN %s RTSP/1.0\r\n"                  //keep connection alive
"CSeq: %d\r\n"
"User-Agent: %s\r\n"
"Session: %s\r\n\r\n";

bool RTSP_SESSION::setNonBlock(int fd)
{
	int flags;
	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if (-1 == fcntl(fd, F_SETFL, flags))
		return -1;
	return 0;
}

bool RTSP_SESSION::setBlock(int fd)
{
	int flags;
	flags = fcntl(fd, F_GETFL, 0);
	flags ^= O_NONBLOCK;
	if (-1 == fcntl(fd, F_SETFL, flags))
		return -1;
	return 0;
}

int RTSP_SESSION::init_TCP(string IP, string Port, string file)
{

	struct sockaddr_in serveraddr;
	tcpFd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&serveraddr, 0, sizeof(serveraddr));
	serverIP = IP;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(Port.c_str()));
	serveraddr.sin_addr.s_addr = inet_addr(IP.c_str());
	
	socklen_t len = sizeof(serveraddr);

	if (-1 == connect(tcpFd, (struct sockaddr *)&serveraddr, len))
		return -1;
	BaseURL = "rtsp://" + IP + ":" + Port + file;
	CSeq = 1;

	return 0;
}

char * RTSP_SESSION::getLine(char *startOfLine, string &str)
{
	// returns the start of the next line, or NULL if none
	char* ptr = NULL;
	for (ptr=startOfLine; *ptr != '\0'; ++ptr)
	{
		if (*ptr == '\r' || *ptr == '\n')
		{
			// We found the end of the line
			*ptr++ = '\0';
			if (*ptr == '\n') ++ptr;
			str = startOfLine;
			return ptr;
		}
	}
	return NULL;
}

string RTSP_SESSION::getSessionID(string str)  //some server append timeout behind sessionid
{
	string line = str;
	int index;
	int pos;
	index = line.find("Session:");
	if ((pos = line.find(";timeout=")) != string::npos)
		line = line.substr(index + 9, pos - index - 9);
	else
	{
		line = line.substr(index + 9);
	}
	return line;
}

int RTSP_SESSION::getResponseCode(char *buf) //return response code
{
	string line;
	char *next = buf;
	vector<string> str_list;
	nextLine = getLine(next, line);
	do
	{
		string tmp_s = "";
		int comma_n = line.find(" ");
		if (-1 == comma_n)
		{
			tmp_s = line.substr(0, line.length());
			str_list.push_back(tmp_s);
			break;
		}
		tmp_s = line.substr(0, comma_n);
		line.erase(0, comma_n + 1);
		str_list.push_back(tmp_s);
	} while (true);
	return atoi(str_list[1].c_str());
}

int RTSP_SESSION::getOptions()
{
	int ret;
	char *cmd = new char[1024];
	char *buf = new char[1024];
	string line;
	sprintf(cmd, optFmt, BaseURL.c_str(), CSeq++, UserHeaderstr.c_str());
	cout << cmd << endl;

	if (send(tcpFd, cmd, strlen(cmd), 0) < 0)
	{
		cout << "send OPTIONS request  failed" << endl;
		return -1;
	}

	if (ret = recv(tcpFd, buf, 1024, 0) <= 0)
	{
		cout << "recv OPTIONS response failed" << endl;
		return -1;
	}
	if (200 == getResponseCode(buf))
	{
		delete[]buf;
		delete[]cmd;
		return 0;
	}
	else
	{		
		delete[]buf;
		delete[]cmd;
		return -1; 
	}

}

int RTSP_SESSION::getDescribe()
{
	int ret = 0;
	string str;
	sdp = new SDP;
	char *cmd = new char[1024];
	char *buf = new char[1024];
	sdpStatus nextstatus = getmediaName;
	sprintf(cmd, desFmt, BaseURL.c_str(), CSeq++, UserHeaderstr.c_str());
	cout << cmd << endl;
	if (send(tcpFd, cmd, strlen(cmd), 0) < 0)
	{
		cout << "send DESCRIBE request failed" << endl;
		return -1;
	}

	if (ret = recv(tcpFd, buf, 1024, 0) <= 0)
	{
		cout << "recv DESCRIBE response failed" << endl;
		return -1;
	}
	
	if (200 == getResponseCode(buf))
	{ 
		do
		{
			nextLine = getLine(nextLine, str);
		} while (str != "v=0");
		SDP *cursdp = sdp;
		while (nextLine != NULL)
		{
			int index;
			switch (nextstatus)
			{
			case getmediaName:
				if ((index = str.find("m=")) != string::npos)
				{
					cursdp->mediaName = str.substr(index + 2, 5);
					nextstatus = getformat;
				}
				break;
			case getformat:
				if ((index = str.find("a=rtpmap:")) != string::npos)
				{
					int comma;
					index = str.find(" ");
					comma = str.find("/");
					cursdp->format = str.substr(index + 1, comma - index - 1);
					cursdp->timestamp = atoi(str.substr(comma + 1).c_str());
					nextstatus = getcontrolPath;
				}
				break;
			case getcontrolPath:
				if ((index = str.find("a=control:")) != string::npos)
				{
					cursdp->controlPath = str.substr(index + 10);
					nextstatus = getnextmediaName;
				}
				break;
			case getnextmediaName:
				if ((index = str.find("m=")) != string::npos)
				{
					SDP *nextsdp = new SDP;
					nextsdp->mediaName = str.substr(index + 2, 5);
					cursdp->next = nextsdp;
					cursdp = nextsdp;
					nextstatus = getformat;
				}
				break;
			default:
				break;
			}
			nextLine = getLine(nextLine, str);
		}
		delete []buf;
		delete []cmd;
		return 0;
	}
	else
	{ 	
		delete[]buf;
		delete[]cmd;
		return -1;
	}
}

int RTSP_SESSION::setupSDP()
{
	int ret = 0;
	int index;
	int comma;
	string str;
	char *cmd = new char[1024];
	char *buf = new char[1024];
	sprintf(cmd, setFmt, (BaseURL+"/"+sdp->controlPath).c_str(), CSeq++, UserHeaderstr.c_str(), clientPort, clientPort+1);
	cout << cmd << endl;
	if (send(tcpFd, cmd, strlen(cmd), 0) < 0)
	{
		cout << "send SETUP request failed" << endl;
		return -1;
	}
	if (ret = recv(tcpFd, buf, 1024, 0) <= 0)
	{
		cout << "recv SETUP response failed" << endl;
		return -1;
	}
	if (200 == getResponseCode(buf))
	{
		do
		{
			nextLine = getLine(nextLine, str);
			index = str.find("server_port=");
			comma = str.find_last_of("-");
		} while (index == string::npos);
		udpPort = atoi(str.substr(index + 12, comma - index - 12).c_str());
		nextLine = getLine(nextLine, str);
		sessionID = getSessionID(str);
		return 0;
	}
	else
	{
		delete[]buf;
		delete[]cmd;
		return -1;
	}
}

int RTSP_SESSION::init_UDP()
{
	char sendline[] = {0xce, 0xfa, 0xed, 0xfe};
	char buf[2048];
	struct sockaddr_in serveraddr;
	int addr_len = sizeof(struct sockaddr_in);
	memset(&serveraddr, 0, sizeof(serveraddr));
	udpFd = socket(AF_INET, SOCK_DGRAM, 0);


	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(udpPort);
	serveraddr.sin_addr.s_addr = inet_addr(serverIP.c_str());

	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(clientPort);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	int ret = bind(udpFd, (struct sockaddr*)&myaddr, sizeof(myaddr));

	socklen_t len = sizeof(serveraddr);

	ret = sendto(udpFd, sendline, sizeof(sendline), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	//ret = recvfrom(udpFd, buf, 2048, 0, (struct sockaddr*)&serveraddr, &len);
	return 0;
}

int RTSP_SESSION::playMedia()
{
	int ret = 0;
	char *cmd = new char[1024];
	char *buf = new char[1024];
	string line;
	sprintf(cmd, playFmt, BaseURL.c_str(), CSeq++, UserHeaderstr.c_str(),sessionID.c_str());
	cout << cmd << endl;
	if (send(tcpFd, cmd, strlen(cmd), 0) < 0)
		cout << "send PLAY request failed" << endl;

	if (ret = recv(tcpFd, buf, 1024, 0) <= 0)
		cout << "recv PLAY response failed" << endl;
	return 0;
}


bool RTSP_SESSION::unpackRTP(void * bufIn, int len, void **pBufOut, int &pOutLen)
{
	pOutLen = 0;
	if (len < RTP_HEADLEN)
	{
		return -1;
	}
	unsigned  char *  src = (unsigned  char *)bufIn + RTP_HEADLEN;
	unsigned  char  head1 = *src; // 获取第一个字节 
	unsigned  char  head2 = *(src + 1); // 获取第二个字节 
	unsigned  char  nal = head1 & 0x1f; // 获取FU indicator的类型域， 
	unsigned  char  flag = head2 & 0xe0; // 获取FU header的前三位，判断当前是分包的开始、中间或结束 
	unsigned  char  nal_fua = (head1 & 0xe0) | (head2 & 0x1f); // FU_A nal 
	bool  bFinishFrame = false;
	if (nal == 0x1c)
	{
		if (flag == 0x80) //start frame
		{
			*pBufOut = src - 3;
			*((int *)(*pBufOut)) = 0x01000000;
			*((char *)(*pBufOut) + 4) = nal_fua;
			pOutLen = len - RTP_HEADLEN + 3;
		}
		else   if (flag == 0x40) // end frame
		{
			*pBufOut = src + 2;
			pOutLen = len - RTP_HEADLEN - 2;
		}
		else //middle
		{
			*pBufOut = src + 2;
			pOutLen = len - RTP_HEADLEN - 2;
		}
	}
	else // 单包数据 
	{
		*pBufOut = src - 4;
		*((int *)(*pBufOut)) = 0x01000000;
		pOutLen = len - RTP_HEADLEN + 4;
	}
	unsigned  char *  bufTmp = (unsigned  char *)bufIn;
	if (bufTmp[1] & 0x80)
		bFinishFrame = true; // rtp mark 
	else
		bFinishFrame = false;
	return  bFinishFrame;
}


int RTSP_SESSION::readRTP()
{
	int i = 70;
	int ret;
	int outLen;
	unsigned char buf[1500];
	unsigned char * toWritebuf = new unsigned char;
	//unsigned char startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
	FILE *fp;
	unsigned int RTP_HEADLEN = 12;
	
	fp = fopen("ddd.h264", "w+t");
	while (i>0)
	{	
		i--;
		cout << i << " read " << ret << endl;

		ret = read(udpFd, buf, 1450);
		
		unpackRTP(buf, ret, (void **)&toWritebuf, outLen);
		fwrite(toWritebuf, outLen, 1, fp);
		
		//if (i == 10000)
		//{
		//	if (-1 == keepAlive())
		//		break;
		//	i = 0;
		//}
	}
	return 0;
}

int RTSP_SESSION::tearDown()
{
	int ret;
	char *cmd = new char[1024];
	char *buf = new char[1024];
	string line;
	sprintf(cmd, stopFmt, BaseURL.c_str(), CSeq++, UserHeaderstr.c_str(), sessionID.c_str());
	cout << cmd << endl;

	if (send(tcpFd, cmd, strlen(cmd), 0) < 0)
	{
		cout << "send TEARDOWN request  failed" << endl;
		return -1;
	}

	if (ret = recv(tcpFd, buf, 1024, 0) <= 0)
	{
		cout << "recv TEARDOWN response failed" << endl;
		return -1;
	}
	if (200 == getResponseCode(buf))
	{
		delete[]buf;
		delete[]cmd;
		return 0;
	}
	else
	{
		delete[]buf;
		delete[]cmd;
		return -1;
	}

}

int RTSP_SESSION::keepAlive()
{
	int ret;
	char *cmd = new char[1024];
	char *buf = new char[1024];
	string line;
	sprintf(cmd, getFmt, BaseURL.c_str(), CSeq++, UserHeaderstr.c_str(), sessionID.c_str());
	cout << cmd << endl;

	if (send(tcpFd, cmd, strlen(cmd), 0) < 0)
	{
		cout << "send GET PARAMETER request  failed" << endl;
		return -1;
	}

	if (ret = recv(tcpFd, buf, 1024, 0) <= 0)
	{
		cout << "recv GET PARAMETER response failed" << endl;
		return -1;
	}
	if (200 == getResponseCode(buf))
	{
		delete[]buf;
		delete[]cmd;
		return 0;
	}
	else
	{
		delete[]buf;
		delete[]cmd;
		return -1;
	}

}