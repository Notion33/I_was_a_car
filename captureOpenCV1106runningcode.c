/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 * All information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

 // edited by Hyundai Autron
 // gcc -I. -I./utils `pkg-config opencv --cflags` -I./include  -c -o captureOpenCV.o captureOpenCV.c
 // gcc -I. -I./utils `pkg-config opencv --cflags` -I./include  -c -o nvthread.o nvthread.c
 // gcc  -o captureOpenCV captureOpenCV.o nvthread.o  -L ./utils -lnvmedia -lnvtestutil_board -lnvtestutil_capture_input -lnvtestutil_i2c -lpthread `pkg-config opencv --libs`

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include <nvcommon.h>
#include <nvmedia.h>

#include <testutil_board.h>
#include <testutil_capture_input.h>

#include "nvthread.h"


#include <highgui.h>
#include <cv.h>
#include <ResTable_720To320.h>
#include <pthread.h>
#include <unistd.h>     // for sleep

////////////////////////////////////////////////////////////////////////////
#include "car_lib.h"    //TY add 6.27
#include <stdbool.h>
////////////////////////////////////////////////////////////////////////////

#define SERVO_CONTROL     // TY add 6.27
#define SPEED_CONTROL              // To servo control(steering & camera position)
#define IMGSAVE
#define LIGHT_BEEP
#define debug
////////////////////////////////////////////////////////////////////////////


#define VIP_BUFFER_SIZE 6
#define VIP_FRAME_TIMEOUT_MS 100
#define VIP_NAME "vip"

#define MESSAGE_PRINTF printf

#define CRC32_POLYNOMIAL 0xEDB88320L

#define RESIZE_WIDTH  320
#define RESIZE_HEIGHT 240

#define whitepx 255

/////////////////////////////로터리에 필요한 #define 입니다
#define CHANNEL1 1
#define CHANNEL4 4
#define WHITEY 200
#define WHITEU 130 //nyc's hold 130 : 오전 11시경 시험 잘됨 //해진 후에는 100이 잘됨

#define BLOCKY 101//장애물 yuv범위
#define BLOCKU 102
#define BLOCKV 133

#define INRANGEMAXY 100// rotary 출발여부 판단부 roi 이 범위안에 들어오면 긍정적
#define INRANGEMINY 0
#define INRANGEMAXX 200
#define INRANGEMINX 0

#define OUTRANGEMAXY 240 //rotary 출발여부 판단부 roi 이 범위 안에 들어오면 위험
#define OUTRANGEMINY 100
#define OUTRANGEMAXX 320
#define OUTRANGEMINX 100


#define SHADOWYMAX 81
#define SHADOWYMIN 26
#define SHADOWUMAX 147
#define SHADOWUMIN 129
#define SHADOWVMAX 127
#define SHADOWVMIN 118
#define RUNNING_RANGE_MIN_X 0
#define RUNNING_RANGE_MAX_X 80
#define RUNNING_RANGE_MIN_Y 100
#define RUNNING_RANGE_MAX_Y 200
#define SHADOW_RANGE_MIN_X 10
#define SHADOW_RANGE_MAX_X 310
#define SHADOW_RANGE_MIN_Y 130
#define SHADOW_RANGE_MAX_Y 210


#define BOX_COLOR 101,255,102,255,133,255
#define SHADOW 0,70,128,147,118,127
//////////////////////////////////////////////

int angle = 1500;
int speed = 100;
int color = 0;
int white_count = 0;
int red_count = 0;

bool turn_left_max = false;         //TY add
bool turn_right_max = false;

FILE* f;

static NvMediaVideoSurface *capSurf = NULL;

pthread_cond_t      cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

int table_298[256];
int table_409[256];
int table_100[256];
int table_208[256];
int table_516[256];

typedef struct
{
	I2cId i2cDevice;

	CaptureInputDeviceId vipDeviceInUse;
	NvMediaVideoCaptureInterfaceFormat vipInputtVideoStd;
	unsigned int vipInputWidth;
	unsigned int vipInputHeight;
	float vipAspectRatio;

	unsigned int vipMixerWidth;
	unsigned int vipMixerHeight;

	NvBool vipDisplayEnabled;
	NvMediaVideoOutputType vipOutputType;
	NvMediaVideoOutputDevice vipOutputDevice[2];
	NvBool vipFileDumpEnabled;
	char * vipOutputFileName;

	unsigned int vipCaptureTime;
	unsigned int vipCaptureCount;
} TestArgs;

typedef struct
{
	NvMediaVideoSurface *surf;
	NvBool last;
} QueueElem;

typedef struct
{
	char *name;

	NvSemaphore *semStart, *semDone;

	NvMediaVideoCapture *capture;
	NvMediaVideoMixer *mixer;
	FILE *fout;

	unsigned int inputWidth;
	unsigned int inputHeight;

	unsigned int timeout;

	NvBool displayEnabled;
	NvBool fileDumpEnabled;

	NvBool timeNotCount;
	unsigned int last;
} CaptureContext;

static NvBool stop = NVMEDIA_FALSE;

static void SignalHandler(int signal)
{
	stop = NVMEDIA_TRUE;
	MESSAGE_PRINTF("%d signal received\n", signal);
}

static void GetTime(NvMediaTime *time)
{
	struct timeval t;

	gettimeofday(&t, NULL);

	time->tv_sec = t.tv_sec;
	time->tv_nsec = t.tv_usec * 1000;
}

static void AddTime(NvMediaTime *time, NvU64 uSec, NvMediaTime *res)
{
	NvU64 t, newTime;

	t = (NvU64)time->tv_sec * 1000000000LL + (NvU64)time->tv_nsec;
	newTime = t + uSec * 1000LL;
	res->tv_sec = newTime / 1000000000LL;
	res->tv_nsec = newTime % 1000000000LL;
}

//static NvS64 SubTime(NvMediaTime *time1, NvMediaTime *time2)
static NvBool SubTime(NvMediaTime *time1, NvMediaTime *time2)
{
	NvS64 t1, t2, delta;

	t1 = (NvS64)time1->tv_sec * 1000000000LL + (NvS64)time1->tv_nsec;
	t2 = (NvS64)time2->tv_sec * 1000000000LL + (NvS64)time2->tv_nsec;
	delta = t1 - t2;

	//    return delta / 1000LL;
	return delta > 0LL;
}


static void DisplayUsage(void)
{
	printf("Usage : nvmedia_capture [options]\n");
	printf("Brief: Displays this help if no arguments are given. Engages the respective capture module whenever a single \'c\' or \'v\' argument is supplied using default values for the missing parameters.\n");
	printf("Options:\n");
	printf("-va <aspect ratio>    VIP aspect ratio (default = 1.78 (16:9))\n");
	printf("-vmr <width>x<height> VIP mixer resolution (default 800x480)\n");
	printf("-vf <file name>       VIP output file name; default = off\n");
	printf("-vt [seconds]         VIP capture duration (default = 10 secs); overridden by -vn; default = off\n");
	printf("-vn [frames]          # VIP frames to be captured (default = 300); default = on if -vt is not used\n");
}

static int ParseOptions(int argc, char *argv[], TestArgs *args)
{
	int i = 1;

	// Set defaults if necessary - TBD
	args->i2cDevice = I2C4;     // i2c chnnel

	args->vipDeviceInUse = AnalogDevices_ADV7182;
	args->vipInputtVideoStd = NVMEDIA_VIDEO_CAPTURE_INTERFACE_FORMAT_VIP_NTSC;
	args->vipInputWidth = 720;
	args->vipInputHeight = 480;
	args->vipAspectRatio = 0.0f;

	args->vipMixerWidth = 800;
	args->vipMixerHeight = 480;

	args->vipDisplayEnabled = NVMEDIA_FALSE;
	args->vipOutputType = NvMediaVideoOutputType_OverlayYUV;
	args->vipOutputDevice[0] = NvMediaVideoOutputDevice_LVDS;
	args->vipFileDumpEnabled = NVMEDIA_FALSE;
	args->vipOutputFileName = NULL;

	args->vipCaptureTime = 0;
	args->vipCaptureCount = 0;



	if (i < argc && argv[i][0] == '-')
	{
		while (i < argc && argv[i][0] == '-')
		{
			if (i > 1 && argv[i][1] == '-')
			{
				MESSAGE_PRINTF("Using basic and custom options together is not supported\n");
				return 0;
			}

			// Get options
			if (!strcmp(argv[i], "-va"))
			{
				if (++i < argc)
				{
					if (sscanf(argv[i], "%f", &args->vipAspectRatio) != 1 || args->vipAspectRatio <= 0.0f) // TBC
					{
						MESSAGE_PRINTF("Bad VIP aspect ratio: %s\n", argv[i]);
						return 0;
					}
				}
				else
				{
					MESSAGE_PRINTF("Missing VIP aspect ratio\n");
					return 0;
				}
			}
			else if (!strcmp(argv[i], "-vmr"))
			{
				if (++i < argc)
				{
					if (sscanf(argv[i], "%ux%u", &args->vipMixerWidth, &args->vipMixerHeight) != 2)
					{
						MESSAGE_PRINTF("Bad VIP mixer resolution: %s\n", argv[i]);
						return 0;
					}
				}
				else
				{
					MESSAGE_PRINTF("Missing VIP mixer resolution\n");
					return 0;
				}
			}
			else if (!strcmp(argv[i], "-vf"))
			{
				args->vipFileDumpEnabled = NVMEDIA_TRUE;
				if (++i < argc)
					args->vipOutputFileName = argv[i];
				else
				{
					MESSAGE_PRINTF("Missing VIP output file name\n");
					return 0;
				}
			}
			else if (!strcmp(argv[i], "-vt"))
			{
				if (++i < argc)
					if (sscanf(argv[i], "%u", &args->vipCaptureTime) != 1)
					{
						MESSAGE_PRINTF("Bad VIP capture duration: %s\n", argv[i]);
						return 0;
					}
			}
			else if (!strcmp(argv[i], "-vn"))
			{
				if (++i < argc)
					if (sscanf(argv[i], "%u", &args->vipCaptureCount) != 1)
					{
						MESSAGE_PRINTF("Bad VIP capture count: %s\n", argv[i]);
						return 0;
					}
			}
			else
			{
				MESSAGE_PRINTF("%s is not a supported option\n", argv[i]);
				return 0;
			}

			i++;
		}
	}

	if (i < argc)
	{
		MESSAGE_PRINTF("%s is not a supported option\n", argv[i]);
		return 0;
	}

	// Check for consistency
	if (i < 2)
	{
		DisplayUsage();
		return 0;
	}


	if (args->vipAspectRatio == 0.0f)
		args->vipAspectRatio = 1.78f;

	if (!args->vipDisplayEnabled && !args->vipFileDumpEnabled)
		args->vipDisplayEnabled = NVMEDIA_TRUE;


	if (!args->vipCaptureTime && !args->vipCaptureCount)
		args->vipCaptureCount = 300;
	else if (args->vipCaptureTime && args->vipCaptureCount)
		args->vipCaptureTime = 0;



	return 1;
}

static int DumpFrame(FILE *fout, NvMediaVideoSurface *surf)
{
	NvMediaVideoSurfaceMap surfMap;
	unsigned int width, height;

	if (NvMediaVideoSurfaceLock(surf, &surfMap) != NVMEDIA_STATUS_OK)
	{
		MESSAGE_PRINTF("NvMediaVideoSurfaceLock() failed in DumpFrame()\n");
		return 0;
	}

	width = surf->width;
	height = surf->height;

	unsigned char *pY[2] = { surfMap.pY, surfMap.pY2 };
	unsigned char *pU[2] = { surfMap.pU, surfMap.pU2 };
	unsigned char *pV[2] = { surfMap.pV, surfMap.pV2 };
	unsigned int pitchY[2] = { surfMap.pitchY, surfMap.pitchY2 };
	unsigned int pitchU[2] = { surfMap.pitchU, surfMap.pitchU2 };
	unsigned int pitchV[2] = { surfMap.pitchV, surfMap.pitchV2 };
	unsigned int i, j;

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < height / 2; j++)
		{
			fwrite(pY[i], width, 1, fout);
			pY[i] += pitchY[i];
		}
		for (j = 0; j < height / 2; j++)
		{
			fwrite(pU[i], width / 2, 1, fout);
			pU[i] += pitchU[i];
		}
		for (j = 0; j < height / 2; j++)
		{
			fwrite(pV[i], width / 2, 1, fout);
			pV[i] += pitchV[i];
		}
	}


	NvMediaVideoSurfaceUnlock(surf);

	return 1;
}


static int Frame2Ipl(IplImage* img, IplImage* imgResult, int color)
{
	//color : 1. 빨간색 2. 노란색 3. 초록색 4.흰*노 mix  defalut. 노란차선검출
	NvMediaVideoSurfaceMap surfMap;
	unsigned int resWidth, resHeight;
	unsigned char y, u, v;
	int num;


	if (NvMediaVideoSurfaceLock(capSurf, &surfMap) != NVMEDIA_STATUS_OK)
	{
		MESSAGE_PRINTF("NvMediaVideoSurfaceLock() failed in Frame2Ipl()\n");
		return 0;
	}

	unsigned char *pY[2] = { surfMap.pY, surfMap.pY2 };
	unsigned char *pU[2] = { surfMap.pU, surfMap.pU2 };
	unsigned char *pV[2] = { surfMap.pV, surfMap.pV2 };
	unsigned int pitchY[2] = { surfMap.pitchY, surfMap.pitchY2 };
	unsigned int pitchU[2] = { surfMap.pitchU, surfMap.pitchU2 };
	unsigned int pitchV[2] = { surfMap.pitchV, surfMap.pitchV2 };
	unsigned int i, j, k, x;
	unsigned int stepY, stepU, stepV;

	unsigned int bin_num = 0;

	resWidth = RESIZE_WIDTH;
	resHeight = RESIZE_HEIGHT;

	// Frame2Ipl
	img->nSize = 112;
	img->ID = 0;
	img->nChannels = 3;
	img->alphaChannel = 0;
	img->depth = IPL_DEPTH_8U;    // 8
	img->colorModel[0] = 'R';
	img->colorModel[1] = 'G';
	img->colorModel[2] = 'B';
	img->channelSeq[0] = 'B';
	img->channelSeq[1] = 'G';
	img->channelSeq[2] = 'R';
	img->dataOrder = 0;
	img->origin = 0;
	img->align = 4;
	img->width = resWidth;
	img->height = resHeight;
	img->imageSize = resHeight*resWidth * 3;
	img->widthStep = resWidth * 3;
	img->BorderMode[0] = 0;
	img->BorderMode[1] = 0;
	img->BorderMode[2] = 0;
	img->BorderMode[3] = 0;
	img->BorderConst[0] = 0;
	img->BorderConst[1] = 0;
	img->BorderConst[2] = 0;
	img->BorderConst[3] = 0;

	stepY = 0;
	stepU = 0;
	stepV = 0;
	i = 0;
	white_count = 0;
	red_count = 0;

	for (j = 0; j < resHeight; j++)
	{
		for (k = 0; k < resWidth; k++)
		{
			x = ResTableX_720To320[k];
			y = pY[i][stepY + x];
			u = pU[i][stepU + x / 2];
			v = pV[i][stepV + x / 2];

			// - 39 , 111 , 51, 241
			num = 3 * k + 3 * resWidth*(j);
			bin_num = j*imgResult->widthStep + k;

			if (y > WHITEY && u > WHITEU) {//흰색
				white_count++;
			}
			if (v > 140) { //빨간색
				red_count++;
			}

			switch (color) {
			case 1:   //  빨간색
				if (v > 140) {
					// 흰색으로
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;

			case 2:   //  노란색
				if (y > 90 && y < 105 && v>146) {
					// 흰색으로
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;

			case 3:   //  초록색
				if (y < 100 && u < 127 && v < 123) {
					// 흰색으로
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;

			case 4:   //  흰*노랑 mix
				if (y > 140) {
					// 흰색으로 -> 실제 흰색&노랑
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;


			default:  //  기본 : 노란 차선검출
				if (u > -39 && u < 120 && v>45 && v < 245) {
					// 흰색으로
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;
			}


			img->imageData[num] = y;
			img->imageData[num + 1] = u;
			img->imageData[num + 2] = v;

		}
		stepY += pitchY[i];
		stepU += pitchU[i];
		stepV += pitchV[i];
	}


	NvMediaVideoSurfaceUnlock(capSurf);

	return 1;
}


static unsigned int CaptureThread(void *params)
{
	int i = 0;
	NvU64 stime, ctime;
	NvMediaTime t1 = { 0 }, t2 = { 0 }, st = { 0 }, ct = { 0 };
	CaptureContext *ctx = (CaptureContext *)params;
	NvMediaVideoSurface *releaseList[4] = { NULL }, **relList;
	NvMediaRect primarySrcRect;
	NvMediaPrimaryVideo primaryVideo;

	primarySrcRect.x0 = 0;
	primarySrcRect.y0 = 0;
	primarySrcRect.x1 = ctx->inputWidth;
	primarySrcRect.y1 = ctx->inputHeight;

	primaryVideo.next = NULL;
	primaryVideo.previous = NULL;
	primaryVideo.previous2 = NULL;
	primaryVideo.srcRect = &primarySrcRect;
	primaryVideo.dstRect = NULL;


	NvSemaphoreDecrement(ctx->semStart, NV_TIMEOUT_INFINITE);

	if (ctx->timeNotCount)
	{
		GetTime(&t1);
		AddTime(&t1, ctx->last * 1000000LL, &t1);
		GetTime(&t2);
		printf("timeNotCount\n");
	}
	GetTime(&st);
	stime = (NvU64)st.tv_sec * 1000000000LL + (NvU64)st.tv_nsec;

	while ((ctx->timeNotCount ? (SubTime(&t1, &t2)) : ((unsigned int)i < ctx->last)) && !stop)
	{
		GetTime(&ct);
		ctime = (NvU64)ct.tv_sec * 1000000000LL + (NvU64)ct.tv_nsec;
		//printf("frame=%3d, time=%llu.%09llu[s] \n", i, (ctime-stime)/1000000000LL, (ctime-stime)%1000000000LL);

		pthread_mutex_lock(&mutex);            // for ControlThread()

		if (!(capSurf = NvMediaVideoCaptureGetFrame(ctx->capture, ctx->timeout)))
		{ // TBD
			MESSAGE_PRINTF("NvMediaVideoCaptureGetFrame() failed in %sThread\n", ctx->name);
			stop = NVMEDIA_TRUE;
			break;
		}

		if (i % 3 == 0)                        // once in three loop = 10 Hz
			pthread_cond_signal(&cond);        // ControlThread() is called

		pthread_mutex_unlock(&mutex);        // for ControlThread()

		primaryVideo.current = capSurf;
		primaryVideo.pictureStructure = NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD;

		if (NVMEDIA_STATUS_OK != NvMediaVideoMixerRender(ctx->mixer, // mixer
			NVMEDIA_OUTPUT_DEVICE_0, // outputDeviceMask
			NULL, // background
			&primaryVideo, // primaryVideo
			NULL, // secondaryVideo
			NULL, // graphics0
			NULL, // graphics1
			releaseList, // releaseList
			NULL)) // timeStamp
		{ // TBD
			MESSAGE_PRINTF("NvMediaVideoMixerRender() failed for the top field in %sThread\n", ctx->name);
			stop = NVMEDIA_TRUE;
		}

		primaryVideo.pictureStructure = NVMEDIA_PICTURE_STRUCTURE_BOTTOM_FIELD;
		if (NVMEDIA_STATUS_OK != NvMediaVideoMixerRender(ctx->mixer, // mixer
			NVMEDIA_OUTPUT_DEVICE_0, // outputDeviceMask
			NULL, // background
			&primaryVideo, // primaryVideo
			NULL, // secondaryVideo
			NULL, // graphics0
			NULL, // graphics1
			releaseList, // releaseList
			NULL)) // timeStamp
		{ // TBD
			MESSAGE_PRINTF("NvMediaVideoMixerRender() failed for the bottom field in %sThread\n", ctx->name);
			stop = NVMEDIA_TRUE;
		}

		if (ctx->fileDumpEnabled)
		{
			if (!DumpFrame(ctx->fout, capSurf))
			{ // TBD
				MESSAGE_PRINTF("DumpFrame() failed in %sThread\n", ctx->name);
				stop = NVMEDIA_TRUE;
			}

			if (!ctx->displayEnabled)
				releaseList[0] = capSurf;
		}

		relList = &releaseList[0];

		while (*relList)
		{
			if (NvMediaVideoCaptureReturnFrame(ctx->capture, *relList) != NVMEDIA_STATUS_OK)
			{ // TBD
				MESSAGE_PRINTF("NvMediaVideoCaptureReturnFrame() failed in %sThread\n", ctx->name);
				stop = NVMEDIA_TRUE;
				break;
			}
			relList++;
		}

		if (ctx->timeNotCount)
			GetTime(&t2);

		i++;
	} // while end

	// Release any left-over frames
//    if(ctx->displayEnabled && capSurf && capSurf->type != NvMediaSurfaceType_YV16x2) // To allow returning frames after breaking out of the while loop in case of error
	if (ctx->displayEnabled && capSurf)
	{
		NvMediaVideoMixerRender(ctx->mixer, // mixer
			NVMEDIA_OUTPUT_DEVICE_0, // outputDeviceMask
			NULL, // background
			NULL, // primaryVideo
			NULL, // secondaryVideo
			NULL, // graphics0
			NULL, // graphics1
			releaseList, // releaseList
			NULL); // timeStamp

		relList = &releaseList[0];

		while (*relList)
		{
			if (NvMediaVideoCaptureReturnFrame(ctx->capture, *relList) != NVMEDIA_STATUS_OK)
				MESSAGE_PRINTF("NvMediaVideoCaptureReturnFrame() failed in %sThread\n", ctx->name);

			relList++;
		}
	}

	NvSemaphoreIncrement(ctx->semDone);
	return 0;
}

static void CheckDisplayDevice(NvMediaVideoOutputDevice deviceType, NvMediaBool *enabled, unsigned int *displayId)
{
	int outputDevices;
	NvMediaVideoOutputDeviceParams *outputParams;
	int i;

	// By default set it as not enabled (initialized)
	*enabled = NVMEDIA_FALSE;
	*displayId = 0;

	// Get the number of devices
	if (NvMediaVideoOutputDevicesQuery(&outputDevices, NULL) != NVMEDIA_STATUS_OK) {
		return;
	}

	// Allocate memory for information for all devices
	outputParams = malloc(outputDevices * sizeof(NvMediaVideoOutputDeviceParams));
	if (!outputParams) {
		return;
	}

	// Get device information for acll devices
	if (NvMediaVideoOutputDevicesQuery(&outputDevices, outputParams) != NVMEDIA_STATUS_OK) {
		free(outputParams);
		return;
	}

	// Find desired device
	for (i = 0; i < outputDevices; i++) {
		if ((outputParams + i)->outputDevice == deviceType) {
			// Return information
			*enabled = (outputParams + i)->enabled;
			*displayId = (outputParams + i)->displayId;
			break;
		}
	}

	// Free information memory
	free(outputParams);
}

//  디버깅 이미지 생성
#ifdef  IMGSAVE
void writeonImage(IplImage* imgResult, char* str_info) {
	char* str = str_info;

	//font
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 0.9, 0.9, 0, 1, CV_AA);
	//cvInitFont(&font, 폰트이름, 1.0, 1.0, 0, 1, CV_AA);

	//textPoint
	CvPoint myPoint = cvPoint(10, 235);

	cvPutText(imgResult, str, myPoint, &font, cvScalar(0, 0, 255, 0));
	//cvPutText(Mat&, string& ,textPoint, &font, cvScalar(255,255,255,0));

}

CvPoint getEndPoint(int angle) {
	CvPoint point;
	//double x=-1, y=-1;
	int len = 208;
	double seta = 90 + (angle - 1500) / 10;

	point.x = (int)(RESIZE_WIDTH / 2 + len*cos(seta * CV_PI / 180.0));
	point.y = (int)(RESIZE_HEIGHT - len*sin(seta * CV_PI / 180.0));

	return point;
}

void drawonImage(IplImage* imgResult, int angle) {
	CvPoint point1, point2;
	point1.x = RESIZE_WIDTH / 2;
	point1.y = RESIZE_HEIGHT - 20;
	point2 = getEndPoint(angle);

	cvLine(imgResult, point1, point2, CV_RGB(255, 255, 0), 2, 8, 0);
}
#endif

int white_line_process(IplImage* imgOrigin){//return 1: stopline, return 2:3way, return 3:nothing

	int i,j,k; //for loop
    int cnt = 0;//number of white line for stop
    
    bool FindWhiteBlock = false;
    bool FindBlack1 = false;
    bool FindBlack2 = false;
    bool FindWhiteLine = false;

	#ifdef IMGSAVE
	char fileName[40];
	IplImage* imgResult1;            // TY add 6.27
	imgResult1 = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
	cvZero(imgResult1);
	static int num = 0;
	for(i = 0;i<240;i++)
		for(j = 0;j<320;j++){
			if(imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU)imgResult1->imageData[i*320+j] = 255;
			else if(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164);
			else imgResult1->imageData[i*320+j] = 127;
		}
	sprintf(fileName, "captureImage/imgResultforWLP%d.png", num);          // TY add 6.27
	num++;
    cvSaveImage(fileName, imgResult1, 0);
    #endif           // TY add 6.27	

    for(i = 80;i<140;i++){//detect whether it is stopline
        for(j = 0;j<120;j++){
            if((imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU)){//whitepixel
                for(k=0; k<200; k++){ //check successive 200 white pixels
                        if(!(imgOrigin->imageData[(i*320+j+k)*3]>WHITEY && imgOrigin->imageData[(i*320+j+k)*3+1]>WHITEU))break;
                        if(k==199)FindWhiteLine = true;
                    }
                    j = j + k;
                    if(FindWhiteLine)break;
            }
        }
        if(FindWhiteLine){cnt++;printf("\n%d",i);}
        else cnt = 0;
        if(cnt==3)return 1;//if whiteline ==3
        FindWhiteLine = false;
    }
    cnt = 0;
    for(i = 130;i<200;i++){//detect whether it is 3way
        for(j = 0;j<60;j++){//find whiteblock
	        if(!FindBlack1&&(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164)){//firstblocknotdetected&&blackpixel
	            for(k=0; k<30; k++){ //check successive 5 white pixels
	                    if(!(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164))break;
	                    if(k==29)FindBlack1 = true;
	                }
	            j = j + k;
	        }
	        else if(FindBlack1&&!FindWhiteBlock&&imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU){//white
                for(k=0; k<10; k++){ 
                        if(!(imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU))break;
                        if(k==9){
                        	FindWhiteBlock = true;
                        	break;
                        }
	                }
	            j = j+k;
	        }
	        else if(FindWhiteBlock&&!FindBlack2&&imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164){//find third black block
	            for(k=0; k<20; k++){ //check successive 5 white pixels
	                if(!(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164))break;
	                if(k==19)FindBlack2 = true;
	                }
	            j = j+k;
	        }
	    }
	    if(FindBlack2)cnt++;
	    else cnt = 0;
	    FindWhiteBlock = false;
	    FindBlack2 = false;
	    FindBlack2 = false;
	    if(cnt>20)return 2;
	}	
	return 0;
}

void emergencyStopRed(){
    // int x = 20, y= 0;
    int width = 280, height = 10;
    int mThreshold = width*height*0.1;

    //적색 px판단
    // int i, j;
    // int count = 0;
    // for(j=y; j<y+height; j++){
    //     for(i=x; i<x+width; i++){
    //         int px = imgColor->imageData[i + j*imgColor->widthStep];
    //         if(px == whitepx){
    //             //TODO
    //             count++;
    //         }
    //     }
    // }
    printf("threshold : %d\n", mThreshold);
    if(red_count > mThreshold){
        //급정지! 대기
        //speed = 0;
        printf("\nStop! Red stop / countpx : %d / %d \n\n",red_count, mThreshold);
        //DesireSpeed_Write(0);   //정지
        speed = 0;

        //curFlag = FLAG_STOP_EMERGENCYRED;
        //isStop = 1;
    }
	// else if(count < mThreshold && curFlag == FLAG_STOP_EMERGENCYRED){
	//
    //     // TODO 3초 대기
    //     curFlag = FLAG_STRAIGHT;
    // }
    // else if(count < width*height*0.05 && isStop == 1){
    //     printf("\n\n GOGOGOGOGOGOGOGO! \n\n");
    //     //출발
    //     //아예 이 함수 플래그 죽이기
    // }
}

//===================================
//  Log file module / NYC
void writeLog(int frameNum){
    //로그 쓸 준비
    fprintf(f, "Frame %d", frameNum);
	// 프레임넘
    //거리센서 로그
    //writeDistanceLog();
    //라인센서 로그
    writeLineSensorLog();
    //줄내리기
    fprintf(f, "\n");
}

void writeDistanceLog(){
    int data;

    printf("Distance : ");
    int i = 0;
    for(i=1; i<7; i++){
        data = DistanceSensor(i);
        fprintf(f, data);
        if(i!=5) fprintf(f, "%d", data);
        printf("0x%04X(%d) ", data, data);
    }
    printf("\n");
}

void writeLineSensorLog(){
    int sensor = LineSensor_Read();        // black:1, white:0
    int s = 0;
	int i = 0;
	char byte = 0x80;

    printf("LineSensor_Read() = ");
	fprintf(f, ", ");
    for(i=0; i<8; i++)
    {
        s = sensor & byte;	//0x80
        if((i % 4) ==0) printf(" ");
        if(s) printf("1");
        else printf("0");
        sensor = sensor << 1;

		if(s) fprintf(f, "1");
        else fprintf(f, "0");
    }
    printf("\n");
    //printf("LineSensor_Read() = %d \n", sensor);
    //fprintf(f, "%d\n", sensor);

}
//  End of Logfile module
//===================================

//===================================
//	선 밟을때 긴급탈출 모듈

int Stop_line() {
	int line_cnt = 0;
	int i;

	for (i = 0; i < 7; i++) {
		if (!(LineSensor_Read() >> i & 0x1)) {
			line_cnt++;
		}
	}
	printf("line_cnt = %d\n", line_cnt);
	if (line_cnt > 5) { //linesensor 5개이상 감지
		return 1;
	}
	return 0;
}


int isLine(){
	int dir = 0;
	int sensor = LineSensor_Read();        // black:1, white:0
	int s = 0;
	int c[8];
	int i=0;

	for(i=0; i<8; i++)
    {
        s = sensor & 0x80;	//0x80
        if(s) c[i]=1;
        else c[i]=0;
        sensor = sensor << 1;
    }

	if( c[4] == 0 ){
		//선을 중앙으로 밟음. 중앙선 무시
		dir = 0;
	} else if( c[1]==0 || c[2]==0 && c[3]==1 ){
		angle = 2000;
		dir = 1;
	} else if( c[5]==1 && c[6]==0 || c[7]==0 ){
		angle = 1000;
		dir = 2;
	}

	//비트연산
	// if(sensor&0xf0 < 0x70){		// 0001 0000 이상
		// angle = 1000; 			// 우회전
		// dir = 1;
	// } else if(sensor&0x87 < 0x07){		// 0000 0010 이하
	// 	angle = 2000;			// 좌회전
	// 	dir = 2;
	// }


	return dir;
}

//	end of lineEscape()
//===================================


////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////  Find_Center : TY 영상처리하여 조향값 찾아내는 알고리즘.
/////////////////////////////////////  << 추후 조향값만 반환하고, 실제조향하는 함수를 따로 분리해주어야함.
/////////////////////////////////////  빈공간에 원형만 선언해둠.
////////////////////////////////////////////////////////////////////////////////////////////
void Find_Center(IplImage* imgResult)		//TY add 6.27
{
	int i=0;
    int j=0;
    int k=0;

    int y_start_line = 150;     //y_start_line과 y_end_line 차는 line_gap의 배수이어야 함.
    int y_end_line = 130;
    int y_high_start_line = 110;
    int y_high_end_line = 90;

    int valid_left_amount = 0;
    int valid_right_amount = 0;
    int valid_high_left_amount = 0;
    int valid_high_right_amount = 0;

    int left_line_start = 0;
    int left_line_end = 0;
    int right_line_start = 0;
    int right_line_end = 0;
    int bottom_line = 200;
    int line_tolerance = 70;
    int low_line_width = 20;
    int high_line_width = 10;
    int curve_speed = 100;       //default : 60
    int straight_speed = 130;    //default : 90

    int line_gap = 5;  //line by line 스캔시, lower line과 upper line의 차이는 line_gap px
    int tolerance = 25; // center pixel +- tolerance px 내에서 라인검출시 for문 종료 용도
    int high_tolerance = 60;
    int angle=1500;
    float low_line_weight = 320; // control angle weight
    float high_line_weight = 80;
    float control_angle = 0;

    int left[240] = {0};
    int right[240] = {imgResult->width-1};
    float left_slope[2] = {0.0};
    float right_slope[2] = {0.0};

    bool continue_turn_left = false;
    bool continue_turn_right = false;

    for(i = y_start_line ; i>y_end_line ; i=i-line_gap){
		if (turn_right_max == true)
			j = imgResult->width - 1;
		else
			j = (imgResult->width) / 2;
        for(; j>0 ; j--){                            //Searching the left line point
                left[y_start_line-i] = j;
                if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                    for( k = 0 ; k < low_line_width ; k++){                     //차선이 line_width만큼 연속으로 나오는지 확인
                        if( j-k <= 0)
                          k = low_line_width - 1;
                        else if(imgResult->imageData[i*imgResult->widthStep + j - k] == 255)
                          continue;
                        break;
                    }
                    if(k = low_line_width - 1){
                      valid_left_amount++;
                      break;
                    }
                }
		}
		if (turn_left_max == true)
			j = 0 ; 
		else
			j = (imgResult->width) / 2 ;
        for(; j<imgResult->width ; j++){             //Searching the right line point
                right[y_start_line-i] = j;
                if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                      for( k = 0 ; k < low_line_width ; k++){                   //차선이 line_width만큼 연속으로 나오는지 확인
                        if( j + k >= imgResult->widthStep)
                          k = low_line_width - 1;
                        else if(imgResult->imageData[i*imgResult->widthStep + j + k] == 255)
                          continue;
                        break;
                    }
                    if(k = low_line_width - 1){
                      valid_right_amount++;
                      break;
                    }
                }
        }
        if(left[y_start_line-i]>((imgResult->width/2)-tolerance)||right[y_start_line-i]<((imgResult->width/2)+tolerance)){     //검출된 차선이 화면중앙부근에 있는경우, 차선검출 종료후 반대방향으로 최대조향 flag set
            if(valid_left_amount >= valid_right_amount && turn_left_max == false){
            	printf("continue_turn_right set!\n");
                continue_turn_right = true;
            }
            else if(valid_right_amount >= valid_left_amount && turn_right_max == false){
            	printf("continue_turn_left set!\n");
                continue_turn_left = true;
            }
            printf("valid_left_amount = %d , valid_right_amount = %d \n",valid_left_amount,valid_right_amount);
            break;
        }
    }

    if (continue_turn_left == false && continue_turn_right == false){          //turn max flag가 false인 경우 1. 직선 2. 과다곡선
        printf("continue_turn_flag_off__1__\n");
        if(turn_left_max == true){                                  //2. 과다곡선인 경우, 차선이 정상검출범위내로 돌아올때까지 턴 유지
            for(i = imgResult->widthStep -1 ; i > (imgResult->width/2) + line_tolerance ; i--){
            	if(imgResult->imageData[y_start_line*imgResult->widthStep + i] == 255){
                	continue_turn_left = false;
                	printf("continue_turn_flag_OFF__overCurve_left__\n");
                    break;
                }
                else if (i == imgResult->width/2 + line_tolerance + 1){
                	printf("continue_turn_flag_ON__overCurve_left__\n");
                	continue_turn_left = true;
                	break;
                }
            }
        }
        else if (turn_right_max == true){
            for(i = 0 ; i < (imgResult->width/2) - line_tolerance ; i++){
            	if(imgResult->imageData[y_start_line*imgResult->widthStep + i] == 255){
                	continue_turn_right = false;
                	printf("continue_turn_flag_OFF__2_right__i:%d\n",i);
                    break;
                }
                else if (i == imgResult->width/2 - line_tolerance - 1){
                	printf("continue_turn_flag_ON__2_right__\n");
					continue_turn_right = true;
                	break;
				}
            }
        }
    }

    if (continue_turn_left == false && continue_turn_right == false){   //1. 직선인 경우, 조향을 위한 좌우측 차선 검출 후 기울기 계산
            printf("continue_turn_flag_all_off__3__\n");
            for(i=0;i<=valid_left_amount;i++){                        //좌측 차선 검출
                if(left[i*line_gap]!=0){
                    left_line_start = y_start_line - i * line_gap;
                    left_line_end = y_start_line - (i + valid_left_amount) * line_gap;
                    break;
                }
            }
            for(i=0;i<=valid_right_amount;i++){                        //우측 차선 검출
                if(right[i*line_gap]!=imgResult->width-1){
                    right_line_start = y_start_line - i * line_gap;
                    right_line_end = y_start_line - (i + valid_right_amount) * line_gap;
                    break;
                }
            }

            printf("\nleft line = ");
            for(i=0;i<valid_left_amount;i++)printf("%d  ",left[i*line_gap]);
            printf("    valid left line = %d\n",valid_left_amount);
            printf("right line = ");
            for(i=0;i<valid_right_amount;i++)printf("%d ",right[i*line_gap]);
            printf("    valid right line = %d\n",valid_right_amount);

            if(valid_left_amount > 1){                                          //좌측 차선 기울기 계산
                left_slope[0] = (float)(left[0] - left[(valid_left_amount-1)*line_gap])/(float)(valid_left_amount*line_gap);
            }
            else left_slope[0] = 0;

            if(valid_right_amount > 1){                                          //우측 차선 기울기 계산
                right_slope[0] = (float)(right[0] - right[(valid_right_amount-1)*line_gap])/(float)(valid_right_amount*line_gap);
            }
            else right_slope[0] = 0;
            
            control_angle = (left_slope[0] + right_slope[0])*low_line_weight;        //차량 조향 기울기 계산

            printf("left_slope : %f ,right_slope : %f   	",left_slope[0],right_slope[0]);
            printf("Control_Angle_low : %f \n\n",control_angle);
        }

    turn_left_max = continue_turn_left;             //현재 프레임에서 최대조향이라고 판단할 경우, 최대조향 전역변수 set.
    turn_right_max = continue_turn_right;

    if(turn_left_max == false && turn_right_max == false && control_angle == 0.0 ){   //아랫쪽차선 검출시도후, 직진주행 하는 경우,
        printf("Does not detected low_lain\n");
        for(i = y_high_start_line ; i>y_high_end_line ; i=i-line_gap){
          for(j=(imgResult->width)/2 ; j>0 ; j--){                            //Searching the left line point
                  left[y_high_start_line-i] = j;
                  if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                      for( k = 0 ; k < high_line_width ; k++){                     //차선이 line_width만큼 연속으로 나오는지 확인
                          if( j-k <= 0)
                            k = high_line_width - 1;
                          else if(imgResult->imageData[i*imgResult->widthStep + j - k] == 255)
                            continue;
                          break;
                      }
                      if(k = high_line_width - 1){
                        valid_high_left_amount++;
                        break;
                      }
                  }
          }
          for(j=(imgResult->width)/2 ; j<imgResult->width ; j++){             //Searching the right line point
                  right[y_high_start_line-i] = j;
                  if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                        for( k = 0 ; k < high_line_width ; k++){                   //차선이 line_width만큼 연속으로 나오는지 확인
                          if( j + k >= imgResult->widthStep)
                            k = high_line_width - 1;
                          else if(imgResult->imageData[i*imgResult->widthStep + j + k] == 255)
                            continue;
                          break;
                      }
                      if(k = high_line_width - 1){
                        valid_high_right_amount++;
                        break;
                      }
                  }
          }
        //   if(left[y_high_start_line-i]>((imgResult->width/2)-high_tolerance)||right[y_high_start_line-i]<((imgResult->width/2)+high_tolerance))     //검출된 차선이 화면중앙부근에 있는경우, 아랫쪽차선까지 올수있도록 무시
        //       break;
        }

        for(i=0;i<=valid_high_left_amount;i++){                        //좌측 차선 검출
            if(left[i*line_gap]!=0){
                left_line_start = y_high_start_line - i * line_gap;
                left_line_end = y_high_start_line - (i + valid_high_left_amount) * line_gap;
                break;
            }
        }
        for(i=0;i<=valid_high_right_amount;i++){                        //우측 차선 검출
            if(right[i*line_gap]!=imgResult->width-1){
                right_line_start = y_high_start_line - i * line_gap;
                right_line_end = y_high_start_line - (i + valid_right_amount) * line_gap;
                break;
            }
        }

      printf("\nleft high line = ");
      for(i=0;i<valid_high_left_amount;i++)printf("%d  ",left[i*line_gap]);
      printf("    valid high left line = %d\n",valid_high_left_amount);
      printf("right high line = ");
      for(i=0;i<valid_high_right_amount;i++)printf("%d ",right[i*line_gap]);
      printf("    valid high right line = %d\n",valid_high_right_amount);

      if(valid_high_left_amount > 1){                                          //좌측 차선 기울기 계산
          left_slope[0] = (float)(left[0] - left[(valid_high_left_amount-1)*line_gap])/(float)(valid_high_left_amount*line_gap);
      }
      else left_slope[0] = 0;

      if(valid_high_right_amount > 1){                                          //우측 차선 기울기 계산
          right_slope[0] = (float)(right[0] - right[(valid_high_right_amount-1)*line_gap])/(float)(valid_high_right_amount*line_gap);
      }
      else right_slope[0] = 0;
      
      control_angle = (left_slope[0] + right_slope[0])*high_line_weight;        //차량 조향 기울기 계산

      printf("left_slope : %f ,right_slope : %f   	",left_slope[0],right_slope[0]);
      printf("Control_Angle_high : %f \n\n",control_angle);
  
      if(abs(control_angle)>100)    //위쪽차선에서 과하게 꺾을경우, 방지 ; 코너에서 인코스로 들어오는걸 방지
        control_angle = 0;

    }


    if (turn_left_max == true)                      //차량 조향각도 판별
        angle = 2000;
    else if (turn_right_max == true)
        angle = 1000;
    else{
        angle = 1500 + control_angle ;                                  // Range : 1000(Right)~1500(default)~2000(Left)
		angle = angle>2000? 2000 : angle<1000 ? 1000 : angle;           // Bounding the angle range
    }
    SteeringServoControl_Write(angle);

    #ifdef SPEED_CONTROL
        if(angle<1200||angle>1800)      //직선코스의 속도와 곡선코스의 속도 다르게 적용
           speed = curve_speed;
        else
           speed = straight_speed;
    #endif

    #ifdef ROI
        for(i=0;i<imgResult->widthStep;i++){
            imgResult->imageData[y_start_line*imgResult->widthStep + i] = 255;
            }
        for(i=0;i<imgResult->widthStep;i++){
            imgResult->imageData[y_end_line*imgResult->widthStep + i] = 255;
			}
			for(i=0;i<imgResult->widthStep;i++){
				imgResult->imageData[y_high_start_line*imgResult->widthStep + i] = 255;
			}
			for(i=0;i<imgResult->widthStep;i++){
				imgResult->imageData[y_high_end_line*imgResult->widthStep + i] = 255;
			}
    #endif

}
void befwhitelinedriving(){//정지선을 보고 정지선에 도달하기 전까지 주행
	int speed = 60;//#define으로 빼야함 
	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;

	IplImage* imgOrigin;
	IplImage* imgResult;            // TY add 6.27

	// cvCreateImage
	imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
	imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
	cvZero(imgResult);          // TY add 6.27
	while (1){
		if(Stop_line())break;
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);

		GetTime(&pt1);
		ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

		Frame2Ipl(imgOrigin, imgResult, color);

		pthread_mutex_unlock(&mutex);

		Find_Center(imgResult);

		DesireSpeed_Write(speed);//조정되어야함
	}
}

void camera(){//테스트용 함수 추후 삭제
    int i,j,k = 0;//for loop

    char fileName[40];
	char fileName1[40];

    IplImage *imgOrigin;
    IplImage *imgResult;
    imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
    imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);
    cvZero(imgResult);
    int num = 0;
	angle = 2400;                       // Range : 600(Right)~1500(default)~2400(Left)
	CameraXServoControl_Write(angle);
	while(true) {   
        	pthread_mutex_lock(&mutex);
        	pthread_cond_wait(&cond, &mutex);
        	Frame2Ipl(imgOrigin, imgResult,color);
        	pthread_mutex_unlock(&mutex);
				for(i = 0;i<240;i++)
					for(j = 0;j<320;j++){
						if(imgOrigin->imageData[(i*320+j)*3]>146 && imgOrigin->imageData[(i*320+j)*3+1]>107&&imgOrigin->imageData[(i*320+j)*3+2]>129)imgResult->imageData[i*320+j] = 255;
					}
				sprintf(fileName, "captureImage/imgOriginrot%d.png", num);
				sprintf(fileName1, "captureImage/imgResultrot%d.png", num);          // TY add 6.27
				num++;
			    cvSaveImage(fileName, imgOrigin, 0);
			    cvSaveImage(fileName1,imgResult,0);
	}
}

int detecttrafficsignal(){//어떻게 할지 논의가 필요 : 신호등인가 로터리인가를 판단해 주는 부분
	//신호등 테스트 시 return 1 주석 풀어주세요
	

	//return 1;

	return 0;
}

// double how_many_that_pixel_in_it_with_that_range(IplImage * imgOrigin,int minY,int maxY,int minU,int maxU, int minV, int maxV, int minRangeX,int maxRangeX, int minRangeY, int maxRangeY){//특정값 이상,미만의 x,y범위에서 yuv입력값 이상,이하에 해당하는 yuv값의 비율을 return합니다. 
// 	int i,j = 0;//루프
// 	double cnt = 0;
// 	for (i = minRangeY; i < maxRangeY;i++)
// 		for(j = minRangeX; j < maxRangeX; j++){
// 			if(imgOrigin->imageData[(i*320+j)*3]>=minY&&imgOrigin->imageData[(i*320+j)*3]<=maxY&&imgOrigin->imageData[(i*320+j)*3+1]>=minU&&imgOrigin->imageData[(i*320+j)*3+1]<=maxU&&imgOrigin->imageData[(i*320+j)*3+2]>=minV&&imgOrigin->imageData[(i*320+j)*3+2]<=maxV)cnt++;
// 		}
// 	return (double)(cnt/((maxRangeX - minRangeX) * (maxRangeY - minRangeY)));
// }


int rotary(){
    int data = 0;//sensor data
    
    bool Departure = false;
    bool IsDetected = false;
    bool whiteblock = false;
    bool CourseOut = false;

    double pixInRange = 0;
    double pixOutRange = 0;
    int cnt = 0;
    int maxcnt = 0;

    double pixnow = 0;//장애물 pixel
    double pixshadow = 0;//장애물 그림자 pixel
    int i,j,k = 0;//for loop
	//////////////////////////////////////////////////
    #ifdef IMGSAVE
    char fileName[40];
	char fileName1[40];	
	char fileName2[40];
	int num = 0;

    IplImage *imgOrigin;
    IplImage *imgResult;
    IplImage *imgResult1;
    
    imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
    imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);
    imgResult1 = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);
    
    cvZero(imgResult);
    cvZero(imgResult1);
    #endif
    ///////////////////////////////////////////////
   	color = 4;//white and yellow
    
    #ifdef debug
    printf(" rotary started :D :D:D:D:D:D:D:D:D:D\n\n");
    #endif

    angle = 2000;                       // Range : 600(Right)~1500(default)~2400(Left)
	CameraXServoControl_Write(angle);
	angle = 1700;
    CameraYServoControl_Write(angle);//range 1800(down)~1200(up)    
    sleep(1);
    while(true) {   
        pixInRange = 0;
        pixOutRange = 0;
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);

        Frame2Ipl(imgOrigin, imgResult,color);

        pthread_mutex_unlock(&mutex);        
		#ifdef IMGSAVE
			cvZero(imgResult1);
			for(i = 0;i<240;i++)
				for(j = 0;j<320;j++){
					if(imgOrigin->imageData[(i*320+j)*3] > BLOCKY && imgOrigin->imageData[(i*320+j)*3+1] > BLOCKU && imgOrigin->imageData[(i*320+j)*3+2] > BLOCKV)imgResult1->imageData[i*320+j] = 255;
				}
			sprintf(fileName, "captureImage/imgOriginforRot%d.png", num);
			sprintf(fileName1, "captureImage/imgResultforRotDriving%d.png", num);          // TY add 6.27
			sprintf(fileName2, "captureImage/imgResultRotBlockDetect%d.png", num);
			num++;
			cvSaveImage(fileName, imgOrigin, 0);
			cvSaveImage(fileName1,imgResult,0);
			cvSaveImage(fileName2,imgResult1,0);
		#endif

	    if(!Departure){
	        pixInRange = 0;
	        pixOutRange = 0;

	        for(i = INRANGEMINY;i<INRANGEMAXY;i++)
	        	for(j = INRANGEMINX;j<INRANGEMAXX;j++)
	        		if(imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3]>=BLOCKY&&imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+1]>=BLOCKU&&imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+2]>=BLOCKV)pixInRange++;
	        pixInRange = (double)(pixInRange/((INRANGEMAXX - INRANGEMINX) * (INRANGEMAXY - INRANGEMINY)));

	        for(i = OUTRANGEMINY;i<OUTRANGEMAXY;i++)
	        	for(j = OUTRANGEMINX;j<OUTRANGEMAXX;j++)
	        		if(imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3]>=BLOCKY&&imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+1]>=BLOCKU&&imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+2]>=BLOCKV)pixOutRange++;
	        pixOutRange = (double)(pixOutRange/((OUTRANGEMAXX - OUTRANGEMINX) * (OUTRANGEMAXY - OUTRANGEMINY)));

	        #ifdef debug
        	printf("waitingnow\n");
	        printf("pixInRange : %10lf\t",pixInRange);
	        printf("pixOutRange : %10lf\n",pixOutRange);
	        #endif

	        if((pixInRange<0.4)&&pixOutRange<0.05){
	            Departure = true;
	            angle = 1500;
	            CameraXServoControl_Write(angle);                       // Range : 600(Right)~1500(default)~2400(Left)
				angle = 1800;
				CameraYServoControl_Write(angle);//range 1800(down)~1200(up)
			}
		}	
		else{//출발 신호 받음 : Departure = True
			#ifdef debug
			printf("running now IsDetected : %d, CourseOut : %d\n\n",IsDetected,CourseOut);
			#endif
			cnt = 0;
		    maxcnt = 0;
			if(!CourseOut){//courseout이 True이고 화면에 장애물 픽셀 안잡히면 탈출(courseout은 우측화면에 흰픽셀 잡힌뒤 사라지면 true)
				for(i = 120;i<220;i++){
	        		for(j = 200;j<310;j++){//find whiteblock
		        		if((imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU)){//firstblocknotdetected&&blackpixel
		            		for(k=0; k<5; k++){ //check successive 5 white pixels
		                    	if(!(imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU))break;
		                    	if(k==4)whiteblock = true;
		                	}
		                	j = j + k;
		                }
		            }
		            if(whiteblock){//흰색 한줄을 찾으면
		            	cnt++;
		            	whiteblock = false;
		            }
		            else if(!IsDetected){//isdetected는 흰색선이다
		            	if(cnt>maxcnt)maxcnt = cnt;//탈출 신호로 인식하는 우측 흰색 선
		            	if(maxcnt>10){
		            		IsDetected = true;
		            		#ifdef IMGSAVE
		            		sprintf(fileName1, "captureImage/error_signal.png");
		            		cvSaveImage(fileName1, imgOrigin, 0);
		            		#endif
		            	}
		            	cnt = 0;	
		            }		            	
		        }			            		            
		    }		        
		    if(IsDetected&&maxcnt<6)CourseOut = true;
		    pixnow = 0;
		    pixshadow = 0; 
		    for(i = RUNNING_RANGE_MIN_Y;i<RUNNING_RANGE_MAX_Y;i++)
		    	for(j = RUNNING_RANGE_MIN_X;j<RUNNING_RANGE_MAX_X;j++)
		    		if(imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3]>=BLOCKY&&imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+1]>=BLOCKU&&imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+2]>=BLOCKV)pixnow++;		    	
		    pixnow = (double)pixnow/((RUNNING_RANGE_MAX_Y - RUNNING_RANGE_MIN_Y)*(RUNNING_RANGE_MAX_X - RUNNING_RANGE_MIN_X));

		    for(i = SHADOW_RANGE_MIN_Y;i<SHADOW_RANGE_MAX_Y;i++)
		    	for(j = SHADOW_RANGE_MIN_X;j<SHADOW_RANGE_MAX_X;j++)
		    		if(imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3] >= SHADOWYMIN && imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3] <= SHADOWYMAX && imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+1] >= SHADOWUMIN && imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+1] <= SHADOWUMAX && imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+2] >= SHADOWVMIN && imgOrigin->imageData[(i*RESIZE_WIDTH+j)*3+2]<=SHADOWVMAX)pixshadow++;
		    pixshadow = (double)pixshadow/((SHADOW_RANGE_MAX_X - SHADOW_RANGE_MIN_X)*(SHADOW_RANGE_MAX_Y - SHADOW_RANGE_MIN_Y));
		    
		    data = 0;
			
			for(i = 0;i<40;i++){
				data = data + DistanceSensor(CHANNEL1);
				usleep(10);
				}
			data = data/40;
			#ifdef debug
			printf("pixnow : %lf\n",pixnow);
		    printf("pixshadow : %lf\n",pixshadow);		    
			printf("sensor : %d\n",data);
			#endif

	        if(pixnow>0.1)speed = 0;
	        else if(data>2000)speed = 0;
	        else if(pixshadow>0.3)speed = 0;
			else if(CourseOut){
				color = 0;
			    return 0;
			}
			else {
				Find_Center(imgResult);			 
				speed = speed - 30;
			}
			DesireSpeed_Write(speed);
		}  		
	}
}

void trafficlight(){
	return 0;
}

void ControlThread(void *unused){
	
	int i = 0;
	int flag = 0;
	int line = 0;
	char fileName[40];
	char fileName1[40];         // TY add 6.27
	char fileName_color[40];         // NYC add 8.25
	//char fileName2[30];           // TY add 6.27
	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;

	IplImage* imgOrigin;
	IplImage* imgResult;            // TY add 6.27

	// cvCreateImage
	imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
	imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27

	cvZero(imgResult);          // TY add 6.27

	CarLight_Write(ALL_OFF);
	angle = 1500;                       // Range : 600(Right)~1500(default)~2400(Left)
    CameraXServoControl_Write(angle);
    angle = 1800;                       // Range : 1200(Up)~1500(default)~1800(Down)
    CameraYServoControl_Write(angle);
	while (1)
	{
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);

		GetTime(&pt1);
		ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

		Frame2Ipl(imgOrigin, imgResult, color);

		pthread_mutex_unlock(&mutex);


		// TODO : control steering angle based on captured image ---------------

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////TY.만약 IMGSAVE(26번째줄)가 정의되어있으면 imgOrigin.png , imgResult.png 파일을 captureImage폴더로 저장.
//
		//긴급선회모듈
		int line = isLine();
		/*
			긴급 탈출은 적외선 값을 읽어서 독자 쓰레드를 파야 할지도 모른다고 코멘트 주셨습니다.
			주차는 독자 로직으로 해야할것 같다고 동재선배님이 코멘트 주셨습니다.
		*/
		if(line == 1 || line == 2) angle = 1500 + 500 * (3 - 2 * line);
		//else if (red_count > 280*10*0.4){//TODO : Threashold
		//	printf("red_count!\n\n");
			/*
				TODO: 추후에 논의 후 로직 복붙
			*/
			//emergencyStopRed();//동재 선배님께서 함수화 시키지 말고 그냥 복붙하는게 나을 거라고 코멘트 주셨습니다.
			//돌발정지 모듈
			//////////////////////////////
		//	}
		else if (white_count > 400) {//TODO : Threashold
			///////////////////////////////////
			printf("whiteLine : %d\n", white_count);
			switch(white_line_process(imgOrigin)){//1:stopline2:3way 3:none
				case 1:{	
					printf("stopline detected\n\n");
					Alarm_Write(ON);
					DesireSpeed_Write(0);
					sleep(1);
					Alarm_Write(OFF);
					befwhitelinedriving(); 					//정지선 검출전 주행 정지선 밟으면 return

					printf("sensor detect stopline\n\n");
					DesireSpeed_Write(0);
					if(detecttrafficsignal(imgOrigin)){ // 신호등인지 로터리인지 판단 신호등 1 로터리 0
						printf("trafficlight module\n\n");
						trafficlight();
					}
					else{
						rotary();					
					}
				}
				case 2:{
					printf("3way detected\n\n");
					DesireSpeed_Write(0);
					Alarm_Write(ON);
					sleep(1);
					Alarm_Write(OFF);
					/*
							3차선 구간
										*/
				}
				default:
				{
					printf("nothing new\n");
				Find_Center(imgResult);
				}
			}
		}
		else{
		printf("\n\nFind_Center!!\n\n");
		Find_Center(imgResult);
		}
		DesireSpeed_Write(speed);
		//===================================
		//  LOG 파일 작성
        writeLog(i);
        //===================================
		// 조향과 속도처리는 한 프레임당 마지막에 한번에 처리
		


#ifdef IMGSAVE
		sprintf(fileName, "captureImage/imgOrigin%d.png", i);
		sprintf(fileName1, "captureImage/imgResult%d.png", i);          // TY add 6.27
		//sprintf(fileName_color, "captureImage/imgColor%d.png", i);          // NYC add 8.25
		//sprintf(fileName2, "captureImage/imgCenter%d.png", i);            // TY add 6.27


		cvSaveImage(fileName, imgOrigin, 0);
		cvSaveImage(fileName1, imgResult, 0);           // TY add 6.27
		//cvSaveImage(fileName_color, imgColor, 0);       // NYC add 8.25
		//cvSaveImage(fileName2, imgCenter, 0);         // TY add 6.27

		//  디버그 이미지 생성
		char str_info[50];
		sprintf(str_info, "[Image %d]  Angle : %d, Speed : %d", i, angle, speed);
		writeonImage(imgResult, str_info);
		drawonImage(imgResult, angle);
		sprintf(fileName1, "DebugImage/imgDebug%d.png", i);
		cvSaveImage(fileName1, imgResult, 0);

#endif

		//TY설명 내용
		//imgCenter는 차선검출 및 조향처리 결과를 확인하기위해 이미지로 출력할 경우 사용할 예정.
		//imgCenter는 아직 구현 안되어있으며 필요시 아래의 코드 주석처리 해제시 사용가능
		//char fileName2[30] , IplImage* imgCenter, imgCenter = cvCreateImage(cvGetSize(imgOrigin),
		//IPL_DEPTH_8U, 1), cvZero(imgCenter), sprintf(fileName2, "captureImage/imgCenter%d.png", i), cvSaveImage(fileName2, imgCenter, 0)
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// ---------------------------------------------------------------------

		GetTime(&pt2);
		ptime2 = (NvU64)pt2.tv_sec * 1000000000LL + (NvU64)pt2.tv_nsec;
		printf("--------------------------------operation time=%llu.%09llu[s]\n", (ptime2 - ptime1) / 1000000000LL, (ptime2 - ptime1) % 1000000000LL);


		i++;
	}
}

int main(int argc, char *argv[])
{
	int err = -1;
	TestArgs testArgs;

	//////////////////////////////// TY add 6.27
	unsigned char status;         // To using CAR Control
	//short speed;
	unsigned char gain;
	int position, position_now;
	//short angle;
	int channel;
	int data;
	char sensor;
	int i, j;
	int tol;
	char byte = 0x80;

	f = fopen("captureImage/sensorlog.txt","w");

	////////////////////////////////

	CaptureInputHandle handle;

	NvMediaVideoCapture *vipCapture = NULL;
	NvMediaDevice *device = NULL;
	NvMediaVideoMixer *vipMixer = NULL;
	NvMediaVideoOutput *vipOutput[2] = { NULL, NULL };
	NvMediaVideoOutput *nullOutputList[1] = { NULL };
	FILE *vipFile = NULL;

	NvSemaphore *vipStartSem = NULL, *vipDoneSem = NULL;
	NvThread *vipThread = NULL;

	CaptureContext vipCtx;
	NvMediaBool deviceEnabled = NVMEDIA_FALSE;
	unsigned int displayId;

	pthread_t cntThread;

	signal(SIGINT, SignalHandler);

	memset(&testArgs, 0, sizeof(TestArgs));
	if (!ParseOptions(argc, argv, &testArgs))
		return -1;


#ifdef SERVO_CONTROL                                    //TY add 6.27
	// 3. servo control ----------------------------------------------------------
	printf("0. Car initializing \n");
	CarControlInit();
	printf("\n\n 0.1 servo control\n");
	angle = 1500;                       // Range : 600(Right)~1200(default)~1800(Left)
	CameraXServoControl_Write(angle);
	angle = 1800;                       // Range : 1200(Up)~1500(default)~1800(Down)
	CameraYServoControl_Write(angle);
	angle = 1500;
	SteeringServoControl_Write(angle);

#endif

#ifdef SPEED_CONTROL
	// 2. speed control ---------------------------------------------------------- TY
	printf("\n\nspeed control\n");
	PositionControlOnOff_Write(UNCONTROL); // 엔코더 안쓰고 주행할 경우 UNCONTROL세팅
										   //control on/off
	SpeedControlOnOff_Write(CONTROL);
	//speed controller gain set            // PID range : 1~50 default : 20
	//P-gain
	gain = 20;
	SpeedPIDProportional_Write(gain);
	//I-gain
	gain = 20;
	SpeedPIDIntegral_Write(gain);
	//D-gain
	gain = 20;
	SpeedPIDDifferential_Write(gain);
	//speed set
	speed = 0;
	DesireSpeed_Write(speed);
#endif

#ifdef LIGHT_BEEP
	//0. light and beep Control --------------------------------------------------
	CarLight_Write(ALL_OFF);
	usleep(1000000);
#endif

	printf("1. Create NvMedia capture \n");
	// Create NvMedia capture(s)
	switch (testArgs.vipDeviceInUse)
	{
	case AnalogDevices_ADV7180:
		break;
	case AnalogDevices_ADV7182:
	{
		CaptureInputConfigParams params;

		params.width = testArgs.vipInputWidth;
		params.height = testArgs.vipInputHeight;
		params.vip.std = testArgs.vipInputtVideoStd;

		if (testutil_capture_input_open(testArgs.i2cDevice, testArgs.vipDeviceInUse, NVMEDIA_TRUE, &handle) < 0)
		{
			MESSAGE_PRINTF("Failed to open VIP device\n");
			goto fail;
		}

		if (testutil_capture_input_configure(handle, &params) < 0)
		{
			MESSAGE_PRINTF("Failed to configure VIP device\n");
			goto fail;
		}

		break;
	}
	default:
		MESSAGE_PRINTF("Bad VIP device\n");
		goto fail;
	}


	if (!(vipCapture = NvMediaVideoCaptureCreate(testArgs.vipInputtVideoStd, // interfaceFormat
		NULL, // settings
		VIP_BUFFER_SIZE)))// numBuffers
	{
		MESSAGE_PRINTF("NvMediaVideoCaptureCreate() failed for vipCapture\n");
		goto fail;
	}


	printf("2. Create NvMedia device \n");
	// Create NvMedia device
	if (!(device = NvMediaDeviceCreate()))
	{
		MESSAGE_PRINTF("NvMediaDeviceCreate() failed\n");
		goto fail;
	}

	printf("3. Create NvMedia mixer(s) and output(s) and bind them \n");
	// Create NvMedia mixer(s) and output(s) and bind them
	unsigned int features = 0;


	features |= NVMEDIA_VIDEO_MIXER_FEATURE_VIDEO_SURFACE_TYPE_YV16X2;
	features |= NVMEDIA_VIDEO_MIXER_FEATURE_PRIMARY_VIDEO_DEINTERLACING; // Bob the 16x2 format by default
	if (testArgs.vipOutputType != NvMediaVideoOutputType_OverlayYUV)
		features |= NVMEDIA_VIDEO_MIXER_FEATURE_DVD_MIXING_MODE;

	if (!(vipMixer = NvMediaVideoMixerCreate(device, // device
		testArgs.vipMixerWidth, // mixerWidth
		testArgs.vipMixerHeight, // mixerHeight
		testArgs.vipAspectRatio, //sourceAspectRatio
		testArgs.vipInputWidth, // primaryVideoWidth
		testArgs.vipInputHeight, // primaryVideoHeight
		0, // secondaryVideoWidth
		0, // secondaryVideoHeight
		0, // graphics0Width
		0, // graphics0Height
		0, // graphics1Width
		0, // graphics1Height
		features, // features
		nullOutputList))) // outputList
	{
		MESSAGE_PRINTF("NvMediaVideoMixerCreate() failed for vipMixer\n");
		goto fail;
	}

	printf("4. Check that the device is enabled (initialized) \n");
	// Check that the device is enabled (initialized)
	CheckDisplayDevice(
		testArgs.vipOutputDevice[0],
		&deviceEnabled,
		&displayId);

	if ((vipOutput[0] = NvMediaVideoOutputCreate(testArgs.vipOutputType, // outputType
		testArgs.vipOutputDevice[0], // outputDevice
		NULL, // outputPreference
		deviceEnabled, // alreadyCreated
		displayId, // displayId
		NULL))) // displayHandle
	{
		if (NvMediaVideoMixerBindOutput(vipMixer, vipOutput[0], NVMEDIA_OUTPUT_DEVICE_0) != NVMEDIA_STATUS_OK)
		{
			MESSAGE_PRINTF("Failed to bind VIP output to mixer\n");
			goto fail;
		}
	}
	else
	{
		MESSAGE_PRINTF("NvMediaVideoOutputCreate() failed for vipOutput\n");
		goto fail;
	}



	printf("5. Open output file(s) \n");
	// Open output file(s)
	if (testArgs.vipFileDumpEnabled)
	{
		vipFile = fopen(testArgs.vipOutputFileName, "w");
		if (!vipFile || ferror(vipFile))
		{
			MESSAGE_PRINTF("Error opening output file for VIP\n");
			goto fail;
		}
	}

	printf("6. Create vip pool(s), queue(s), fetch threads and stream start/done semaphores \n");
	// Create vip pool(s), queue(s), fetch threads and stream start/done semaphores
	if (NvSemaphoreCreate(&vipStartSem, 0, 1) != RESULT_OK)
	{
		MESSAGE_PRINTF("NvSemaphoreCreate() failed for vipStartSem\n");
		goto fail;
	}

	if (NvSemaphoreCreate(&vipDoneSem, 0, 1) != RESULT_OK)
	{
		MESSAGE_PRINTF("NvSemaphoreCreate() failed for vipDoneSem\n");
		goto fail;
	}

	vipCtx.name = VIP_NAME;

	vipCtx.semStart = vipStartSem;
	vipCtx.semDone = vipDoneSem;

	vipCtx.capture = vipCapture;
	vipCtx.mixer = vipMixer;
	vipCtx.fout = vipFile;

	vipCtx.inputWidth = testArgs.vipInputWidth;
	vipCtx.inputHeight = testArgs.vipInputHeight;

	vipCtx.timeout = VIP_FRAME_TIMEOUT_MS;

	vipCtx.displayEnabled = testArgs.vipDisplayEnabled;
	vipCtx.fileDumpEnabled = testArgs.vipFileDumpEnabled;

	if (testArgs.vipCaptureTime)
	{
		vipCtx.timeNotCount = NVMEDIA_TRUE;
		vipCtx.last = testArgs.vipCaptureTime;
	}
	else
	{
		vipCtx.timeNotCount = NVMEDIA_FALSE;
		vipCtx.last = testArgs.vipCaptureCount;
	}


	if (NvThreadCreate(&vipThread, CaptureThread, &vipCtx, NV_THREAD_PRIORITY_NORMAL) != RESULT_OK)
	{
		MESSAGE_PRINTF("NvThreadCreate() failed for vipThread\n");
		goto fail;
	}

	printf("wait for ADV7182 ... one second\n");
	sleep(1);

	printf("7. Kickoff \n");
	// Kickoff
	NvMediaVideoCaptureStart(vipCapture);
	NvSemaphoreIncrement(vipStartSem);

	printf("8. Control Thread\n");
	pthread_create(&cntThread, NULL, &ControlThread, NULL);

	printf("9. Wait for completion \n");
	// Wait for completion
	NvSemaphoreDecrement(vipDoneSem, NV_TIMEOUT_INFINITE);
	err = 0;

	#ifdef SPEED_CONTROL    //TY ADD
		speed = 0;
		DesireSpeed_Write(speed);
	#endif

	#ifdef LIGHT_BEEP
		CarLight_Write(ALL_OFF);
		usleep(1000000);
	#endif

	#ifdef SERVO_CONTROL
		angle = 1500;
		SteeringServoControl_Write(angle);
		CameraXServoControl_Write(angle);
		CameraYServoControl_Write(angle);
	#endif

	fclose(f);


fail: // Run down sequence
	// Destroy vip threads and stream start/done semaphores
	if (vipThread)
		NvThreadDestroy(vipThread);
	if (vipDoneSem)
		NvSemaphoreDestroy(vipDoneSem);
	if (vipStartSem)
		NvSemaphoreDestroy(vipStartSem);

	printf("10. Close output file(s) \n");
	// Close output file(s)
	if (vipFile)
		fclose(vipFile);

	// Unbind NvMedia mixer(s) and output(s) and destroy them
	if (vipOutput[0])
	{
		NvMediaVideoMixerUnbindOutput(vipMixer, vipOutput[0], NULL);
		NvMediaVideoOutputDestroy(vipOutput[0]);
	}
	if (vipOutput[1])
	{
		NvMediaVideoMixerUnbindOutput(vipMixer, vipOutput[1], NULL);
		NvMediaVideoOutputDestroy(vipOutput[1]);
	}
	if (vipMixer)
		NvMediaVideoMixerDestroy(vipMixer);


	// Destroy NvMedia device
	if (device)
		NvMediaDeviceDestroy(device);

	// Destroy NvMedia capture(s)
	if (vipCapture)
	{
		NvMediaVideoCaptureDestroy(vipCapture);

		// Reset VIP settings of the board
		switch (testArgs.vipDeviceInUse)
		{
		case AnalogDevices_ADV7180: // TBD
			break;
		case AnalogDevices_ADV7182: // TBD
			//testutil_capture_input_close(handle);
			break;
		default:
			break;
		}
	}

	return err;
}