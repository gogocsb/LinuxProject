#include "RTSP_SESSION.h"
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <memory.h>
#include <sys/time.h>
#include <stdlib.h>


const string UserAgentHeader = "Uestc 1.0(Streaming Media v2015.7.29)\r\n";

const unsigned int UDPPort = 8557;


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

int RTSP_SESSION::ConnectToSerevr(string serverIP, unsigned int serverPort)
{
	int ret;
	struct sockaddr_in serveraddr;
	SockNum = socket(AF_INET, SOCK_STREAM, 0);
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(serverPort);
	serveraddr.sin_addr.s_addr = inet_addr(serverIP.c_str());

	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(10001);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	bind(SockNum, (sockaddr*)&myaddr, sizeof(myaddr));


	int TimeOut = 3000;
	setsockopt(SockNum, SOL_SOCKET, SO_RCVTIMEO, (char*)&TimeOut, sizeof(TimeOut));
	setsockopt(SockNum, SOL_SOCKET, SO_SNDTIMEO, (char*)&TimeOut, sizeof(TimeOut));

	setNonBlock(SockNum);
	
	ret = connect(SockNum, (sockaddr*)&serveraddr, sizeof(serveraddr));

	struct   timeval   timeout;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(SockNum, &set);
	timeout.tv_sec = 3;   //连接超时3秒   
	timeout.tv_usec = 0;

	switch (select(SockNum + 1, 0, &set, 0, &timeout))
	{
	case   -1: //select   fail 
		close(SockNum);
		return -1;
	case   0: //连接超时 
		close(SockNum);
		return -1;
	default: //连接成功 
		break;
	}
	setBlock(SockNum);

	fCSeq = 1; Session = NULL; VFrameRate = 0; fLastSessionId = NULL;

	status = 0;

	return SockNum;
}


int RTSP_SESSION::getSDPDescription(char *responseBuffer)
{
	char cmd[1024], buf[1024 * 20];
	int i;

	sprintf(cmd, "OPTIONS %s RTSP/1.0\r\nCSeq: %d\r\nUser-Agent:%s\r\n", fBaseURL, fCSeq++, UserAgentHeader.c_str());
	cout << cmd << endl;
	if (send(SockNum, cmd, strlen(cmd), 0)<0)
	{
		strcpy(Errtxt, "OPTION send() failed!");	return -1;
	}

	memset(buf, 0, 1024 * 20);

	i = recv(SockNum, buf, 1024 * 20, 0);
	if (i <= 0) { strcpy(Errtxt, "Get SDPOption failed!");	return -1; }
	if (strstr(buf, "200 OK") == NULL)  return -1;


	sprintf(cmd, "DESCRIBE %s RTSP/1.0\r\nCSeq: %d\r\nAccept: application/sdp\r\nUser-Agent:%s\r\n", fBaseURL, fCSeq++, UserAgentHeader.c_str());
	cout << cmd << endl;
	if (send(SockNum, cmd, strlen(cmd), 0)<0) { strcpy(Errtxt, "DESCRIBE send() failed!");	return -1; }

	memset(buf, 0, 1024 * 20);

	i = recv(SockNum, buf, 1024 * 20, 0);
	if (i <= 0) { strcpy(Errtxt, "Get SDPDescription failed!");	return -1; }
	if (strstr(buf, "200 OK") == NULL)  return -1;

	buf[i] = '\0';
	strcpy(responseBuffer, buf);
	return 1;

}

int RTSP_SESSION::initializeWithSDP(char *sdpDescription)
{
	char *sdpLine = sdpDescription, *nextnextSDPLine = NULL;
	char buf[1024];
	SESSION *p_Session = NULL, *pCur;


	while (sdpLine[0] != 'm')
	{
		sdpLine = strstr(sdpLine, "\r\n");
		if (sdpLine == NULL) return -1;
		sdpLine += 2;
	}

	while (sdpLine != NULL)
	{
		//m=video 0 RTP/AVP 32   =<media> <port> <transport> <fmt list> 注 33 代表 TS   96 - H264

		pCur = new SESSION; memset(pCur, 0, sizeof(SESSION)); pCur->fNext = NULL;

		if (Session == NULL) Session = pCur;
		else
		{
			SESSION *p1 = Session;
			while (p1->fNext) p1 = p1->fNext;
			p1->fNext = pCur;
		}

		if (sscanf(sdpLine, "m=%s %hu RTP/AVP %u", buf, &pCur->fClientPortNum, &pCur->fRTPPayloadFormat) != 3
			|| pCur->fRTPPayloadFormat > 127)
		{
			return -1;
		}

		while (1)
		{
			sdpLine = strstr(sdpLine, "\r\n");//a=rtpmap...
			if (sdpLine == NULL) break;
			while (sdpLine[0] == '\r' || sdpLine[0] == '\n') sdpLine++;
			if (sdpLine == NULL) break;
			if (sdpLine[0] == 'm') break;

			//		char * RTSP_SESSION::parseSDPAttribute_rtpmap(char *sdpLine, SESSION *pCur);

			if (sscanf(sdpLine, "a=control:%s", buf) == 1)
			{
				pCur->fControlPath = new char[strlen(buf)];
				strcpy(pCur->fControlPath, buf);
			}

			if (sscanf(sdpLine, "a=fmtp:%s", buf) == 1)
			{
				pCur->fConfig = new char[strlen(buf)];
				strcpy(pCur->fConfig, buf);
			}

		} //while (1) 
	}//(sdpLine != NULL) 

	return 1;
}



int RTSP_SESSION::init_RTP(string SerIP, unsigned int  nPort, string Source)      //step 1
{
	sprintf(fBaseURL, "rtsp://%s:%u/%s", SerIP.c_str(), nPort, Source.c_str());


	if (ConnectToSerevr(SerIP, nPort) <= 0)  // Connect to RTSP Server
	{
		strcpy(Errtxt, "Connect to Server failed!");
		return -1;
	}

	//Get SDP Description

	if (getSDPDescription(responseBuffer) == -1) { close(SockNum); return -1; }

	//Initialize with SDP
	initializeWithSDP(responseBuffer);


	// get track

	MEDIAATTRIBUTE Attribute;
	GetMediaAttrbute(&Attribute);
	setupMediaSubsession(Session);

	playMediaSession(0, -1);  //Setp Play Range
	return 0;
}



MEDIAATTRIBUTE *RTSP_SESSION::GetMediaAttrbute(MEDIAATTRIBUTE *Attribute)
{
	SESSION *pCur = Session;
	int len;

	while (pCur != NULL)
	{
		if (pCur->fRTPPayloadFormat == 96) //H263格式
		{
			//		Attribute->fVideoFrequency = pCur->frtpTimestampFrequency;
			//		Attribute->fVideoPayloadFormat = pCur->fRTPPayloadFormat;

			if (pCur->fConfig != NULL)
			{
				len = strlen(pCur->fConfig);
				memcpy(Attribute->fConfigAsc, pCur->fConfig, len);
				a_hex(pCur->fConfig, Attribute->fConfigHex, len);
				Attribute->fConfigHexLen = len / 2;
			}
			Attribute->fixed_vop_rate = 0;//fixed_vop_rate;
			VFrameRate = Attribute->fVideoFrameRate;
		}
		else if (pCur->fRTPPayloadFormat == 33)//TS格式
		{
			Attribute->fAudioFrequency = pCur->frtpTimestampFrequency;
			Attribute->fAudioPayloadFormat = pCur->fRTPPayloadFormat;
			Attribute->fTrackNum = pCur->fNumChannels;
		}
		pCur = pCur->fNext;
	}//while(pCur != NULL)

	return Attribute;
}


int RTSP_SESSION::setupMediaSubsession(SESSION *pCur)
{
	static int rtpNumber = 0;
	static int rtcpNumber = 0;
	int bytesRead;

	char* firstLine, *nextLineStart;
	char buff[1024];
	char *lineStart;

	char Sessionstr[1024], cmd[2048], readBuf[2048];


	//	char cmdFmt[]= "SETUP %s/%s RTSP/1.0\r\nCSeq: %d\r\nTransport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n%s%s\r\n";
	//zzz
	char cmdFmt[] = "SETUP %s/%s RTSP/1.0\r\nCSeq: %d\r\nUser-Agent: %s\r\Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n";

	do
	{
		if (fLastSessionId == NULL) strcpy(Sessionstr, "");

		rtcpNumber = rtpNumber + 1;

		memset(cmd, 0, 2048);

		if (pCur)
		{
			sprintf(cmd, cmdFmt,
				fBaseURL, pCur->fControlPath,
				fCSeq++,
				UserAgentHeader.c_str(),UDPPort, UDPPort+1);
		}
		else
		{
			sprintf(cmd, cmdFmt,
				fBaseURL, pCur->fControlPath,
				fCSeq++,
				UserAgentHeader.c_str(), UDPPort, UDPPort + 1);
		}
		rtpNumber += 2;

		cout << cmd << endl;
		if (send(SockNum, cmd, strlen(cmd), 0)<0) break;

		bytesRead = recv(SockNum, readBuf, 2048, 0);  //get  serverport


		firstLine = readBuf;
		nextLineStart = getLine(firstLine);
		if (bytesRead <= 0) break;

		firstLine = readBuf;
		nextLineStart = getLine(firstLine);
		if (parseResponseCode(firstLine, &responseCode)) break;
		if (responseCode != 200) return -1;

		while (1)
		{
			lineStart = nextLineStart;
			if (lineStart == NULL || lineStart[0] == '\0') break;
			nextLineStart = getLine(lineStart);
			string line = lineStart;
			if (sscanf(lineStart, "Session: %s", buff) == 1)
			{
				const char *buf = getSessionID(buff);
				pCur->fSessionId = new char[strlen(buf)];
				strcpy(pCur->fSessionId, buf);
				if (fLastSessionId != NULL) delete fLastSessionId;
				fLastSessionId = new char[strlen(buf)];
				strcpy(fLastSessionId, buf);
				break;
			}
		}

		if (pCur->fSessionId == NULL) break;
		return 0;
	} while (0);

	return -1;

}


int RTSP_SESSION::a_hex(char *a, unsigned char *hex, unsigned char len)
{
	short i;
	unsigned char aLowbit, aHighbit;
	unsigned char hconval, lconval;

	for (i = 0; i<len; i += 2)
	{
		aHighbit = toupper(a[i]);
		if ((aHighbit >= 'A') && (aHighbit <= 'F'))
			hconval = '7';
		else
			if ((aHighbit >= '0') && (aHighbit <= '9'))
				hconval = '0';
			else
				return -1;
		aLowbit = toupper(a[i + 1]);
		if ((aLowbit >= 'A') && (aLowbit <= 'F'))
			lconval = '7';
		else
			if ((aLowbit >= '0') && (aLowbit <= '9'))
				lconval = '0';
			else
				return -1;
		hex[(i / 2)] = ((aHighbit - hconval) * 16 + (aLowbit - lconval));
	}
	hex[len / 2] = 0x00;
	return 0;
}

char *RTSP_SESSION::getLine(char *startOfLine)
{
	// returns the start of the next line, or NULL if none
	char* ptr;
	for (ptr = startOfLine; *ptr != '\0'; ++ptr)
	{
		if (*ptr == '\r' || *ptr == '\n')
		{
			// We found the end of the line
			*ptr++ = '\0';
			if (*ptr == '\n') ++ptr;
			return ptr;
		}
	}

	return NULL;

}

int RTSP_SESSION::parseResponseCode(char *line, unsigned int *presponseCode)
{

	if (sscanf(line, "%*s%u", presponseCode) != 1)
	{
		fprintf(stderr, "no response code in line\n");
	}
	cout << *presponseCode << endl;
	return 0;
}


int RTSP_SESSION::playMediaSession(int start, int end)
{
	char* cmd = NULL;
	char cmdFmt[] =
		"PLAY %s RTSP/1.0\r\n"
		"CSeq: %d\r\n"
		"Session: %s\r\n"
		"Range: npt=%s-%s\r\n"
		"User-Agent:%s\r\n";
	char startStr[30], endStr[30];

	//unsigned const readBufSize = 2048;
	//char *readBuffer;
	int bytesRead;
	char* firstLine;
	char* nextLineStart;
	unsigned responseCode;
	const int readBufSize = 180;
	char head[56];
	char *readBuffer = (char*)malloc(sizeof(char)*(readBufSize + 1));
	//if (readBuffer == NULL) return -1;
	memset(readBuffer, 0, readBufSize + 1);

	do
	{
		if (fLastSessionId == NULL) break;

		sprintf(startStr, "%d", start);	sprintf(endStr, "%d", end);

		if (start == -1) startStr[0] = '\0'; if (end == -1) endStr[0] = '\0';

		cmd = (char*)malloc(4096); memset(cmd, 0, 4096);

		sprintf(cmd, cmdFmt, fBaseURL, fCSeq++, fLastSessionId, startStr, endStr, UserAgentHeader.c_str());

		cout << cmd << endl;

		if (send(SockNum, cmd, strlen(cmd), 0)<0) return -1;

		bytesRead = recv(SockNum, head, 56, 0);

		bytesRead = recv(SockNum, readBuffer, 180, 0);

		if (bytesRead <= 0) break;



		// Inspect the first line to check whether it's a result code 200
		firstLine = readBuffer;
		nextLineStart = getLine(firstLine);
		if (parseResponseCode(firstLine, &responseCode)) break;
		if (responseCode != 200) return -1;

		//bytesRead = recv(SockNum, readBuffer, 2048, 0);
		free(cmd);
		free(readBuffer);
		return 0;
	} while (0);

	free(cmd);
	free(readBuffer);
	return -1;
}

int RTSP_SESSION::playMediaSessionUDP(int start, int end)
{
	char* cmd = NULL;
	char cmdFmt[] =
		"PLAY %s RTSP/1.0\r\n"
		"CSeq: %d\r\n"
		"Session: %s\r\n"
		"Range: npt=%s-%s\r\n"
		"User-Agent:%s\r\n";
	char startStr[30], endStr[30];
	
	//unsigned const readBufSize = 2048;
	//char *readBuffer;
	int bytesRead;
	char* firstLine;
	char* nextLineStart;
	unsigned responseCode;
	const int readBufSize = 180;
	char head[56];
	char *readBuffer = (char*)malloc(sizeof(char)*(readBufSize + 1));
	//if (readBuffer == NULL) return -1;
	memset(readBuffer, 0, readBufSize + 1);

	do
	{
		if (fLastSessionId == NULL) break;

		sprintf(startStr, "%d", start);	sprintf(endStr, "%d", end);

		if (start == -1) startStr[0] = '\0'; if (end == -1) endStr[0] = '\0';

		cmd = (char*)malloc(4096); memset(cmd, 0, 4096);

		sprintf(cmd, cmdFmt, fBaseURL, fCSeq++, fLastSessionId, startStr, endStr, UserAgentHeader.c_str());

		cout << cmd << endl;

		if (send(SockNum, cmd, strlen(cmd), 0)<0) return -1;

		bytesRead = recv(SockNum, head, 56, 0);

		bytesRead = recv(SockNum, readBuffer, 180, 0);

		if (bytesRead <= 0) break;



		// Inspect the first line to check whether it's a result code 200
		firstLine = readBuffer;
		nextLineStart = getLine(firstLine);
		if (parseResponseCode(firstLine, &responseCode)) break;
		if (responseCode != 200) return -1;

		//bytesRead = recv(SockNum, readBuffer, 2048, 0);
		free(cmd);
		free(readBuffer);
		return 0;
	} while (0);

	free(cmd);
	free(readBuffer);
	return -1;
}

const char *RTSP_SESSION::getSessionID(char *buf)  //some server append timeout behind sessionid
{
	string line = buf;
	if (line.find("timeout") != string::npos)
	{
		int last_pos = line.find_first_of(";timeout");
		string SessionID = line.substr(0, last_pos);
		return SessionID.c_str();
	}
	else
		return line.c_str();
}


int RTSP_SESSION::RTP_ReadHandler(unsigned char *buf, int zMax, int type)
{
	unsigned char c;
	unsigned char streamChannelId;
	unsigned short size;
	unsigned fNextTCPReadSize;
	int result;
	int pos = 0, i;

	do
	{
		i = recv(SockNum, (char *)&c, 1, 0);
		if (i <= 0) return -1;
	} while (c != '$');

	//Channel ID
	if (recv(SockNum, (char *)&streamChannelId, 1, 0) != 1) return -1;

	//streamChannelId = 0  rtp
	//streamChannelId = 1  rtcp
	// Get PackageSize

	if (recv(SockNum, (char *)&size, 2, 0) != 2) return -1;
	fNextTCPReadSize = ntohs(size);

	if ((int)fNextTCPReadSize >= zMax) return -1;

	result = handleRead(buf, fNextTCPReadSize, type);

	return result;
}

int RTSP_SESSION::handleRead(unsigned char *buffer, int Size, int type)
{

	int  i;
	int totalsize = 0;
	int needsize = Size;

	if (type == 1)
	{
		while ((i = recv(SockNum, (char *)buffer + totalsize, needsize, 0)) > 0)
		{
			totalsize += i;
			if (totalsize >= Size) break;
			needsize -= i;
		}
	}
	else
	{
		
	}

	if (totalsize != Size) return -1;
	return totalsize;

}



void RTSP_SESSION::Keep_alive_connect()  //keep alive
{
	char cmdz[5];
	char zSize[2];
	char buf[500];
	unsigned short n;


	if (SockNum == -1) return;

	sprintf(buf, "GET_PARAMETER %s RTSP/1.0\r\nCSeq: %d\r\nUser-Agent:%s\r\nSession:%s", fBaseURL, fCSeq++, UserAgentHeader.c_str(), fLastSessionId);

	n = strlen(buf);

	//cmdz[0] = '$';
	//cmdz[1] = 1;
	//zSize[0] = (char)((n & 0xFF00) >> 8);
	//zSize[1] = (char)(n & 0x00FF);


	////	sprintf(cmdz+4 , "GET_PARAMETER %s RTSP/1.0\r\nCSeq: %d\r\nUser-Agent:%s\r\nSession:%s" , fBaseURL ,fCSeq++, UserAgentHeaderStr,fLastSessionId);

	//n = send(SockNum, cmdz, 1, 0);
	//n = send(SockNum, cmdz + 1, 1, 0);
	//n = send(SockNum, zSize, 2, 0);
	n = send(SockNum, buf, strlen(buf), 0);
	n = n;

}

int RTSP_SESSION::init_UDP(string serverIP, unsigned int serverPort)
{
	int UDPsockfd;
	char buf[2048];
	struct sockaddr_in serveraddr;
	int on = 1;
	int addr_len = sizeof(struct sockaddr_in);
	char sendline[14] = "xxxxxxxxxxxx";
	memset(&serveraddr, 0, sizeof(serveraddr));
	UDPsockfd = socket(AF_INET, SOCK_DGRAM, 0);


	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(6971);
	serveraddr.sin_addr.s_addr = inet_addr(serverIP.c_str());


	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(59097);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	setsockopt(UDPsockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	int ret = bind(UDPsockfd, (struct sockaddr*)&myaddr, sizeof(myaddr));

	

	socklen_t len = sizeof(serveraddr);
	
	ret = sendto(UDPsockfd, sendline, sizeof(sendline), 0,(struct sockaddr*)&serveraddr, sizeof(serveraddr));
	ret = recvfrom(UDPsockfd, buf, 2048, 0, (struct sockaddr*)&serveraddr, &len);


	return UDPsockfd;
	
}