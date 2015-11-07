#include <iostream>
#include "RTSP_Parser.h"
#

using namespace std;

//ipc url rtsp://192.168.0.80
//port 8557
//file Streaming/channels/2?videoCodecType=H.264
int main(int argc, char *argv[])
{
	RTSP_SESSION rtsp;

	rtsp.init_TCP("192.168.0.116", "80", "/2.264");
	//rtsp.init_TCP("192.168.1.128", "8557", "/1");
	//rtsp.init_TCP("192.168.10.235", "8000", "/1");
	if (0 == rtsp.getOptions())
		if (0 == rtsp.getDescribe())
			if (0 == rtsp.setupSDP())
				if(0 == rtsp.init_UDP())
				{
					rtsp.playMedia();
					rtsp.readRTP();
					return 0;
				}		
}