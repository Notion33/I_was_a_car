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
#define SPEED_CONTROL     // To servo control(steering & camera position)
//#define IMGSAVE1

#define straight_speed 230
#define curve_speed 120

//#define IMGSAVE
//#define LIGHT_BEEP
//#define debug
////////////////////////////////////////////////////////////////////////////


#define VIP_BUFFER_SIZE 6
#define VIP_FRAME_TIMEOUT_MS 100
#define VIP_NAME "vip"

#define MESSAGE_PRINTF printf

#define CRC32_POLYNOMIAL 0xEDB88320L

#define RESIZE_WIDTH  320
#define RESIZE_HEIGHT 240

#define whitepx 255

/////////////////////////////주차에 필요한 #define 입니다
#define TRUE 1
#define FALSE 0
#define LEFT 5
#define RIGHT 3

//주차용 변수 이아래 추가한사람 : 태연
int channel_leftNow = 0;
int channel_leftPrev = 0;
int channel_rightNow = 0;
int channel_rightPrev = 0;

int difference_left = 0;
int difference_right = 0;

int first_left_detect = FALSE;
int second_left_detect = FALSE;
int third_left_detect = FALSE;

int first_right_detect = FALSE;
int second_right_detect = FALSE;
int third_right_detect = FALSE;

int parking_space = 0;
int encoder_speed = 50;

bool distance_warmming = FALSE; // distanceThread가 DistanceValue배열을 모두 채웠는지 확인용 flag. 처음시작시 배열이 모두 0이기에 잘못사용되는걸 막기위함.

/////////////////////////////로터리에 필요한 #define 입니다
#define CHANNEL1 1
#define CHANNEL4 4
#define WHITEY 230//white_line_process용도
#define WHITEU 130 //nyc's hold 130 : 오전 11시경 시험 잘됨 //해진 후에는 100이 잘됨

#define OUT_LINE_Y 212//로터리 탈출시 out line 범위
#define OUT_LINE_U 116
#define OUT_LINE_V 108

#define OUTRANGEMAXY 200 //rotary 출발여부 판단부 roi 이 범위 안에 들어오면 위험
#define OUTRANGEMINY 80
#define OUTRANGEMAXX 320
#define OUTRANGEMINX 0

#define SHADOWYMAX 32
#define SHADOWYMIN 19
#define SHADOWUMAX 255
#define SHADOWUMIN 0
#define SHADOWVMAX 255
#define SHADOWVMIN 0

#define SHADOW_RANGE_MIN_X 0//그림자 ROI
#define SHADOW_RANGE_MAX_X 90
#define SHADOW_RANGE_MIN_Y 100
#define SHADOW_RANGE_MAX_Y 220

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

int DistanceValue[4][15] = {0};
bool isOverLine = false;
bool emergencyReturnFlag = true;
bool distanceFlag = true;

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
	//color : 1. 빨간색 2. 노란색 3. 초록색 4.흰*노 mix 5.검은색  defalut. 노란차선검출

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

			if (y > WHITEY && u > WHITEU && j > 40) {//흰색
				white_count++;
			}

			if (v > 165 && j<50) { //빨간색
				red_count++;
			}

			switch (color) {
			case 1:   //  빨간색
				if (v > 165) {
					// 흰색으로
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;

			case 2:   //  노란색
				if (y > 90 && y<105&& v>146) {
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
				if (y > 128) {
					// 흰색으로 -> 실제 흰색&노랑
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;

			case 5: //검은색
				if (y > 35 && y < 50 && u>125) {
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					imgResult->imageData[bin_num] = (char)0;
				}
				break;

			case 6:   //  장애물 창환
				if (y > 150 && u >135 && u<180 && v>112 && v<133) {
					// 흰색으로 -> 실제 흰색&노랑
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;
			
			case 7: //그림자
				if (y > 19 && y< 32 ) {
					// 흰색으로 -> 실제 흰색&노랑
					imgResult->imageData[bin_num] = (char)255;
				}
				else {
					// 검정색으로
					imgResult->imageData[bin_num] = (char)0;
				}
				break;
			default:  //  기본 : 노란 차선검출
				if (y > 96 && u > 43 && u < 89 && v < 143) {
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
	printf("=================white_count = %d=========================\n", white_count);

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
#ifdef  IMGSAVE

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


///////////////////////////////////////////////////////////////////
//////white_count에 대한 모듈들/////////////////////////////////
////////////////////////////////////////////////////////////////
/////////////////white_count - 3way,정지선 구분 알고리즘
/*int white_line_process(IplImage* imgOrigin) {//return 1: stopline, return 2:3way, return 3:nothing

	int i, j, k; //for loop
	int cnt = 0;//number of white line for stop
	int reversecnt = 0;
	bool distinct = false;

	int temp = 0;

	bool FindWhiteBlock = false;
	bool FindBlack1 = false;
	bool FindBlack2 = false;
	bool FindWhiteLine = false;
	bool Findupper = false;

#ifdef IMGSAVE
	char fileName[40];
	IplImage* imgResult1;            // TY add 6.27
	imgResult1 = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
	cvZero(imgResult1);
	static int num = 0;
	for (i = 0; i<240; i++)
		for (j = 0; j<320; j++) {
			if (imgOrigin->imageData[(i * 320 + j) * 3]>WHITEY && imgOrigin->imageData[(i * 320 + j) * 3 + 1]>WHITEU)imgResult1->imageData[i * 320 + j] = 255;
			else if (imgOrigin->imageData[(i * 320 + j) * 3]>22 && imgOrigin->imageData[(i * 320 + j) * 3]<164);
			else imgResult1->imageData[i * 320 + j] = 127;
		}
	sprintf(fileName, "captureImage/imgResultforWLP%d.png", num);          // TY add 6.27
	num++;
	cvSaveImage(fileName, imgResult1, 0);
#endif           // TY add 6.27   
	printf("==================white_line_process============================\n");
	printf("white count = %d\n", white_count);

	for (i = 100; i<210; i++) {//detect whether it is stopline
		for (j = 0; j<120; j++) {
			if ((imgOrigin->imageData[(i * 320 + j) * 3]>WHITEY && imgOrigin->imageData[(i * 320 + j) * 3 + 1]>WHITEU)) {//whitepixel
				for (k = 0; k<200; k += 5)
				{
					if (!(imgOrigin->imageData[(i * 320 + j + k) * 3]>WHITEY && imgOrigin->imageData[(i * 320 + j + k) * 3 + 1]>WHITEU))break;
				}
				if (k >= 199)FindWhiteLine = true;
				j = j + k;
				if (FindWhiteLine)break;
			}
		}
		if (FindWhiteLine) { cnt++; }
		else { cnt = 0; }
		if (cnt >= 2)
			return 1;//if whiteline ==3
		FindWhiteLine = false;
	}
	cnt = 0;
	for (i = 40; i<220; i++) {//detect whether it is 3way//진입 긴구간 없다는 가정하에 i 130~200, j 0 ~ 60 
		if ((!Findupper) || (Findupper&&distinct)) {
			for (j = 0; j<280; j += 3) {//find whiteblock
				if (!FindBlack1 && (imgOrigin->imageData[(i * 320 + j) * 3]>22 && imgOrigin->imageData[(i * 320 + j) * 3]<164)) {//firstblocknotdetected&&blackpixel
					for (k = 0; k<5; k++) //check successive 5 white pixels
						if (!(imgOrigin->imageData[(i * 320 + j) * 3]>22 && imgOrigin->imageData[(i * 320 + j) * 3]<164))break;
					if (k == 4)	FindBlack1 = true;
					j = j + k;
				}
				else if (FindBlack1 && !FindWhiteBlock&&imgOrigin->imageData[(i * 320 + j) * 3]>WHITEY && imgOrigin->imageData[(i * 320 + j) * 3 + 1]>WHITEU) {//white
					for (k = 0; k<5; k++)
						if (!(imgOrigin->imageData[(i * 320 + j) * 3]>WHITEY && imgOrigin->imageData[(i * 320 + j) * 3 + 1]>WHITEU))break;
					if (k == 4)FindWhiteBlock = true;
					j = j + k;
				}
				else if (FindWhiteBlock && !FindBlack2&&imgOrigin->imageData[(i * 320 + j) * 3]>22 && imgOrigin->imageData[(i * 320 + j) * 3]<164) {//find third black block
					for (k = 0; k<5; k++) //check successive 5 white pixels
						if (!(imgOrigin->imageData[(i * 320 + j) * 3]>22 && imgOrigin->imageData[(i * 320 + j) * 3]<164))break;
					if (k == 4) {
						FindBlack2 = true;
						break;
					}
					j = j + k;
				}
			}
			if (FindBlack2) cnt++;
			else cnt = 0;
			FindWhiteBlock = false;
			FindBlack1 = false;
			FindBlack2 = false;
			if (!Findupper && cnt == 5) {
				Findupper = true;
				cnt = 0;
			}
			if (distinct && cnt == 8) return 2;
		}
		else if (Findupper && !distinct) {
			temp = 0;
			for (k = 40; k<280; k++)if (imgOrigin->imageData[(i * 320 + k) * 3]>22 && imgOrigin->imageData[(i * 320 + k) * 3]<164)temp++;
			if (temp > 200) {
				reversecnt++;
			}
			else {
				reversecnt = 0;
				i += 5;
			}
			if (reversecnt == 2)
				distinct = true;
		}
	}
	return 0;
}*/
int white_line_process(IplImage* imgOrigin){//return 1: stopline, return 2:3way, return 3:nothing

	int i,j,k; //for loop
    int cnt = 0;//number of white line for stop
    int reversecnt = 0;
    bool distinct = false;

    int temp = 0;

    bool FindWhiteBlock = false;
    bool FindBlack1 = false;
    bool FindBlack2 = false;
    bool FindWhiteLine = false;
    bool Findupper = false;
	printf("white_line_process\n");
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

    for(i = 80;i<210;i++){//detect whether it is stopline
		//printf("stopline whiteline\n");
        for(j = 0;j<120;j++){
            if((imgOrigin->imageData[(i*320+j)*3]>WHITEY && imgOrigin->imageData[(i*320+j)*3+1]>WHITEU)){//whitepixel
                for(k=0; k<200; k++){ //check successive 200 white pixels
                        if(!(imgOrigin->imageData[(i*320+j+k)*3]>WHITEY && imgOrigin->imageData[(i*320+j+k)*3+1]>WHITEU))break;
                        if(k>=199)FindWhiteLine = true;
                    }
                    j = j + k;
                    if(FindWhiteLine)break;
            }
        }
        if(FindWhiteLine){cnt++;}//printf("\n%d",i);
        else cnt = 0;
        if(cnt>=1)return 1;//if whiteline ==3
        FindWhiteLine = false;
    }
    cnt = 0;
    for(i = 20;i<180;i++){//detect whether it is 3way//진입 긴구간 없다는 가정하에 i 130~200, j 0 ~ 60 
		//printf("3way whiteline\n");
        for(j = 0;j<280;j++){//find whiteblock
	        if(!FindBlack1&&(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164)){//firstblocknotdetected&&blackpixel
	            for(k=0; k<10; k++){ //check successive 5 white pixels
	                    if(!(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164))break;
	                    if(k==9)FindBlack1 = true;
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
	            for(k=0; k<10; k++){ //check successive 5 white pixels
	                if(!(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164))break;
	                if(k==9)FindBlack2 = true;
	                }
	            j = j+k;
	        }
	    }
	    if(FindBlack2)cnt++;
	    else cnt = 0;
	    FindWhiteBlock = false;
	    FindBlack2 = false;
	    FindBlack2 = false;
	    if(!Findupper && cnt==10)Findupper = true;
	    if(Findupper && !distinct){
	    	temp = 0;
	    	for(k = 0;k<320;k++)if(imgOrigin->imageData[(i*320+k)*3]>22 && imgOrigin->imageData[(i*320+k)*3]<164)temp++;
	    	if(temp>280)reversecnt++;
	    	else reversecnt = 0;
	    	i++;
	    	if(reversecnt==5)distinct = true;
	    }
	    if(distinct && cnt==10) return 2;
	}	
	return 0;
}
////////////////////////////////////////////////////////////////////
///////////////신호등&&&회전 교차로/////////////////////////////////
///////////////////////////////////////////////////////////////////

//정지선 구분!!
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

//정지선을 보고 정지선에 도달하기 전까지 주행
int detectStop(IplImage* imgResult) {
	int x, y;
	static int stop_camera = 0;

	printf("detectStop!!\n");

	/*	if (Stop_line() == 1) {
	speed = 0;
	printf("stop!!!!!!\n");
	return 1;
	}
	else {
	Find_Center(imgResult);
	speed = 50;
	return 0;
	}
	*/
	if ((Stop_line() == 1&&stop_camera>1) || stop_camera >= 3) { //stop_camera 의 계수 test따라 변경(최적화 만들기)
		if (Stop_line() != 1 && stop_camera > 4) {
			while (EncoderCounter_Read() < 300) {}
		}
		speed = 0;
		stop_camera = 0;
		printf("stop!!!!!!\n");
		return 1;
	}
	/*	if (Stop_line() == 1 || stop_camera > 200) { ////Stop_line()으로 할경우 안 멈출때 많음 stop_camera 갯수 높이자 test따라 변경(최적화 만들기)
	speed = 0;
	stop_camera = 0;
	printf("stop!!!!!!\n");
	return 1;
	}
	*/
	else {
		Find_Center(imgResult);
		speed = 50;

		printf("==============white count =%d====================\n", white_count);
		if (white_count > 650) {//TODO 5cm에 대한 pixel수 잡히는 정도 측정
			Alarm_Write(ON);
			usleep(1000);
			stop_camera++;
			EncoderCounter_Write(0);
			printf("==================stop camera = %d==========================\n", stop_camera);
		}
		return 0;
	}
}

//카메라를 들어 회전교차로인지 신호등인지 구분
int detect_signal(IplImage* imgResult) {//return 1 : 신호등  return 2 회전교차로 //TODO : test 후 roi잡기
	int x, y;
	static int num = 0;
	static int pass = 0;
	//static int i = 0;
	//IplImage* canny_img;
	CvMemStorage* storage;
	CvSeq* seqCircle;
	float* circle;

	char fileName1[40];
	char fileName2[40];



	printf("detect_signal\n");
	pass++;

	//	canny_img = cvCreateImage(cvGetSize(imgResult), IPL_DEPTH_8U, 1);
	//	cvCanny(imgResult, canny_img, 50, 300, 3);

	storage = cvCreateMemStorage(0);
	seqCircle = cvHoughCircles(imgResult, storage, CV_HOUGH_GRADIENT, 1, 10, 200, 9, 5, 30);
	//(이미지,검출된 원의 메모리,CV_HOUGH_GRADIENT,해상도,원의중심사이의 최소거리,canny임계값,원판단허프변환,최소,최대반지름)
	printf("seqLines->total = %d\n", seqCircle->total);

	for (x = 0; x < seqCircle->total; x++) {

		circle = (float*)cvGetSeqElem(seqCircle, x);
		printf("x =%d, y= %d, r =%d\n", cvRound(circle[0]), cvRound(circle[1]), cvRound(circle[2]));
		cvCircle(imgResult, cvPoint(cvRound(circle[0]), cvRound(circle[1])), cvRound(circle[2]), CV_RGB(0, 100, 0), 1, 8, 0);
		//원을 그릴 이미지,원의중심좌표,원의 반지름좌표,원의반지름길이,원의 색깔,원의경계선두께,원의경계선 종류,shift)
	}


	#ifdef IMGSAVE1
		sprintf(fileName1, "captureImage/signal_detectResult%d.png", num);
		//sprintf(fileName2, "captureImage/signal_detectCanny%d.png", num);
		num++;
		cvSaveImage(fileName1, imgResult, 0);
		//cvSaveImage(fileName2, canny_img, 0);
	#endif
	if (pass > 2) {

		if (seqCircle->total > 0) {
			printf("traffic light!!!!!!!!!!!!!1\n");
			//color = 1;//check black
			return 1;
		}
		else {
			printf("rotary!!!!!!!!!!!!!!!!!!!!\n");
			//CameraYServoControl_Write(1800);
			color = 0;
			return 2;
		}
	}
	/*if (red_count > 200) {
	printf("신호등\n");
	return 1;
	}
	else {
	printf("회전 교차로\n");
	CameraYServoControl_Write(1800);
	color = 0;
	return 2;
	}*/
	return 0;
}

//신호등 알고리즘
int Traffic_Light(IplImage* imgResult) {//TODO cvHoughCircle matadata test  //TODO : imgResult roi잡기
	int x, y;
	//int white_count = 0;	
	//int start_x = 0, end_x = 320;
	//int start_y = 0, end_y = 240;
	int i;
	//IplImage* canny_img;
	CvMemStorage* storage;
	CvSeq* seqCircle;
	float* circle;
	int cx, cy, radius;
	static int red_x = 0, yellow_x = 0, red_y =0, yellow_y=0;
	//int green = 0;
	//float err = 0.1;
	int side = 0, side_y = 0, leftgreen = 0, rightgreen = 0;
	int leftgreen_cnt = 0, rightgreen_cnt = 0;
	static int num = 0;

	char fileName1[40];
	char fileName2[40];

	printf("traffic\n");
	//canny_img = cvCreateImage(cvGetSize(imgResult), IPL_DEPTH_8U, 1);
	//cvCanny(imgResult, canny_img, 50, 200, 3);
	printf("color = %d\n", color);

	if(color == 1 || color ==2){
		storage = cvCreateMemStorage(0);
		seqCircle = cvHoughCircles(imgResult, storage, CV_HOUGH_GRADIENT, 1, 10, 200, 9, 5, 30);
		//(이미지,검출된 원의 메모리,CV_HOUGH_GRADIENT,해상도,원의중심사이의 최소거리,canny임계값,원판단허프변환,최소,최대반지름)
		printf("seqCircle->total = %d\n", seqCircle->total);

		for (i = 0; i < seqCircle->total; i++) {
			circle = (float*)cvGetSeqElem(seqCircle, i);
			cx = cvRound(circle[0]);
			cy = cvRound(circle[1]);
			radius = cvRound(circle[2]);

			if (color == 1){
				red_x += cx;
				red_y += cy;
			}	
			else if (color == 2) {
				yellow_x += cx;
				yellow_y += cy;
			}
			printf("cx =%d  cy =%d  radius =%d\n", cx, cy, radius);
			cvCircle(imgResult, cvPoint(cvRound(circle[0]), cvRound(circle[1])), cvRound(circle[2]), CV_RGB(0, 100, 0), 1, 8, 0);
		}

		if (seqCircle->total > 0) {
			if (color == 1) {
				red_x /= seqCircle->total;
				red_y /= seqCircle->total;
				printf("red_x = %d, red_y = %d\n", red_x, red_y);
			}
			else if (color == 2) {
				yellow_x /= seqCircle->total;
				yellow_y /= seqCircle->total;
				printf("yellow_x = %d, yellow_y = %d\n", yellow_x, yellow_y);
			}
		}
	}

	else if (color == 3) {
		side = yellow_x - red_x;
		side_y = (yellow_y + yellow_y) / 2;
		leftgreen = yellow_x + side;
		rightgreen = yellow_x + side * 2;

		printf("side_y = %d\n", side_y);
		printf("red_x : %d  yellow_x : %d\n", red_x, yellow_x);
		printf("left : %d   right :%d\n", leftgreen, rightgreen);

		if(leftgreen>0&&rightgreen>0&&side_y){
			for (x = leftgreen - 15; x <= leftgreen + 15; x++) {
				for (y = side_y - 10; y <= side_y + 10; y++) {
					if (imgResult->imageData[y*(imgResult->widthStep) + x] == 255) {
						leftgreen_cnt++;
					}
				}
			}

			for (x = rightgreen - 15; x <= rightgreen + 15; x++) {
				for (y = side_y - 10; y <= side_y + 10; y++) {
					if (imgResult->imageData[y*(imgResult->widthStep) + x] == 255) {
						rightgreen_cnt++;
					}
				}
			}
		}
		

		if (leftgreen_cnt >= 100) {
			return 1;
		}
		else if (rightgreen_cnt >= 100) {
			return 2;
		}
		/*if (green <= leftgreen + 15 && green >= leftgreen - 20)
			return 1;//left
		else if (green <= rightgreen + 20 && green >= rightgreen - 15)
			return 2;//right
		*/
	}

	if (seqCircle->total > 0 && color < 3) {
		color++;
	}

	#ifdef IMGSAVE1
		sprintf(fileName1, "captureImage/imgResultcany%d.png", num);
		//sprintf(fileName2, "captureImage/imgCannylight%d.png", num);
		num++;
		cvSaveImage(fileName1, imgResult, 0);
		//cvSaveImage(fileName2, canny_img, 0);
	#endif

	/*for (x = start_x; x < end_x; x++) {
	for (y = start_y; y < end_y; y++) {
	//result_img->imageData[y*(result_img->widthStep) + x] = 255;
	if (imgResult->imageData[y*(imgResult->widthStep) + x] == 255) {
	white_count++;
	}
	}
	}
	printf("white count %d", white_count);
	if (color == 3 && white_count < 200) {
	printf("회전교차로\n");
	return 3;
	}
	if (color == 5) {
	if (white_count > 100 && white_count < 250) {
	return 1;
	}
	else if (white_count >= 250) {
	return 2;
	}
	}
	else if (white_count > 500) {
	color++;
	}*/
	//cvWaitKey(200);
	return 0;
}
int AfterTraffic(int traffic, IplImage* imgResult) {
	int x = 0, y = 0;
	int end_x = imgResult->width, end_y = imgResult->height;
	int white_count = 0;
	static int front = 0;
	printf("After Traffic\n");

	CameraYServoControl_Write(1800);

	if (color == 3 && speed == 0) {
		color = 4;
		angle = 1500;
		speed = 50;
		EncoderCounter_Write(0);
		if (traffic == 1)//left
			return 1;
		else if (traffic == 2)//right
			return 2;
	}

	while (angle == 1500 && EncoderCounter_Read() <= 4500) {
	}
	if (angle == 1500) {
		if (traffic == 1) {//left
			angle = 2000;
			EncoderCounter_Write(0);
			return 1;
		}
		else if (traffic == 2) {//right
			angle = 1000;
			EncoderCounter_Write(0);
			return 2;
		}
	}
	else {
		while (EncoderCounter_Read() <= 8050) {}
		speed = 0;
		return 3;
	}



	//////////////////////////////////////////////////////////////////////////////////
	/*usleep(2200000);
	SteeringServoControl_Write(2000);
	usleep(3500000);
	SteeringServoControl_Write(1500);
	usleep(4000000);
	DesireSpeed_Write(0);
	sleep(100);*/
	//////////////////////////////////////////////////////////////////////////////////

}

int endMission(IplImage* imgResult) {
	int x, y;
	int left_w = 0, right_w = 0;
	int left_cnt = 0, right_cnt = 0;
	int white_cnt = 0, black_cnt = 0;
	float weight = 5, weight_bi = 1;
	static int stop = 0;

	printf("end Mission\n");
	speed = 50;
	for (y = 150; y < imgResult->height - 40; y++) {
		for (x = 0; x < 160; x++) {
			if (imgResult->imageData[y*(imgResult->widthStep) + (320 - x)] == 255) {
				right_w += 320 - x;
				right_cnt++;
			}
			if (imgResult->imageData[y*(imgResult->widthStep) + x] == 255) {
				left_w += x;
				left_cnt++;
			}
		}
		if (right_cnt >= 10 && left_cnt >= 10) {
			left_w = left_w / left_cnt;
			right_w = right_w / right_cnt;
			printf("left_w = %d, right_w = %d\n", left_w, right_w);
			break;
		}
	}
	if (right_cnt >= 10 && left_cnt < 10) {
		right_w = right_w / right_cnt;
		printf("right_w = %d\n", right_w);
		angle = 1500 + (right_w - 160)*weight_bi;
	}
	else if (left_cnt >= 10 && right_cnt < 10) {
		left_w = left_w / left_cnt;
		printf("left_w = %d\n", left_w);
		angle = 1500 - (160 - left_w)*weight_bi;
	}
	else if (right_cnt >= 10 && left_cnt >= 10) {
		if ((160 - left_w) > (right_w - 160))
			angle = 1500 + ((160 - left_w) - (right_w - 160)) *weight;
		else if ((160 - left_w) < (right_w - 160))
			angle = 1500 - ((right_w - 160) - (160 - left_w))*weight;
	}
	else
		angle = 1500;

	for (x = 0; x < imgResult->width; x++) {
		for (y = 150; y < imgResult->height - 40; y++) {
			//result_img->imageData[y*(result_img->widthStep) + x] = 255;
			if (stop < 5 && imgResult->imageData[y*(imgResult->widthStep) + x] == 255) {
				white_cnt++;
			}
			else if (stop >= 5 && imgResult->imageData[y*(imgResult->widthStep) + x] == 0) {
				black_cnt++;
			}
		}
	}
	printf("white_cnt : %d\n", white_cnt);
	printf("black_cnt : %d\n", black_cnt);
	if (stop < 5 && white_cnt > 7000)
		stop++;
	if (stop >= 5 && black_cnt > 9000) {
		speed = 0;
		return 5;
	}

	printf("stop = %d\n", stop);
	if (angle >= 2000)
		angle = 2000;
	if (angle <= 1000)
		angle = 1000;
	printf("angle = %d\n", angle);
	return 3;
}

int rotary() {
	int data = 0;//sensor data
	int databack = 0;

	bool Departure = false;
	bool IsDetected = false;
	bool whiteblock = false;
	bool CourseOut = false;

	double pixOutRange = 0;
	int cnt = 0;
	int maxcnt = 0;

	double pixshadow = 0;//장애물 그림자 pixel
	int i, j, k = 0;//for loop
					//////////////////////////////////////////////////
	#ifdef IMGSAVE
		char fileName[60];
		char fileName1[60];
		char fileName2[60];
		int num = 0;
	#endif
	IplImage *imgOrigin;
	IplImage *imgResult;
	IplImage *imgResult1;

	imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
	imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);
	imgResult1 = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);

	//color = 5;//rotary frame2ipl

	cvZero(imgResult);
	cvZero(imgResult1);

	///////////////////////////////////////////////


	#ifdef debug
		printf(" rotary started :D :D:D:D:D:D:D:D:D:D\n\n");
	#endif

	angle = 2000;                       // Range : 600(Right)~1500(default)~2400(Left)
	CameraXServoControl_Write(angle);
	angle = 1700;
	CameraYServoControl_Write(angle);//range 1800(down)~1200(up)    
	sleep(2);
	while (true) {
		pixOutRange = 0;
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);

		Frame2Ipl(imgOrigin, imgResult, 0);

		pthread_mutex_unlock(&mutex);
	#ifdef IMGSAVE
			cvZero(imgResult1);
			for (i = 0; i<240; i++)
				for (j = 0; j<320; j++) {
					if (imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3] >= SHADOWYMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3] <= SHADOWYMAX && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 1] >= SHADOWUMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 1] <= SHADOWUMAX && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 2] >= SHADOWVMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 2] <= SHADOWVMAX)imgResult1->imageData[i * 320 + j] = 255;
				}
			sprintf(fileName, "captureImage/imgOriginforRot%d.png", num);
			sprintf(fileName1, "captureImage/imgResultforRotDriving%d.png", num);          // TY add 6.27
			sprintf(fileName2, "captureImage/imgResultRotBlockDetect%d.png", num);
			num++;
			cvSaveImage(fileName, imgOrigin, 0);
			cvSaveImage(fileName1, imgResult, 0);
			cvSaveImage(fileName2, imgResult1, 0);
	#endif
		if (!Departure) {
			pixOutRange = 0;

			for (i = OUTRANGEMINY; i<OUTRANGEMAXY; i++)
				for (j = OUTRANGEMINX; j<OUTRANGEMAXX; j++)
					if (imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3] >= SHADOWYMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3] <= SHADOWYMAX && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 1] >= SHADOWUMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 1] <= SHADOWUMAX && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 2] >= SHADOWVMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 2] <= SHADOWVMAX)pixOutRange++;
			pixOutRange = (double)(pixOutRange / ((OUTRANGEMAXX - OUTRANGEMINX) * (OUTRANGEMAXY - OUTRANGEMINY)));

	#ifdef debug
				printf("waitingnow\n");
				printf("pixInRange : %10lf\t", pixInRange);
				printf("pixOutRange : %10lf\n", pixOutRange);
	#endif

			if (pixOutRange<0.003) {
				Departure = true;
				// Alarm_Write(ON);
				// usleep(500);
				// Alarm_Write(OFF);
				angle = 1600;
				CameraXServoControl_Write(angle);                       // Range : 600(Right)~1500(default)~2400(Left)
				angle = 1800;
				CameraYServoControl_Write(angle);//range 1800(down)~1200(up)
				usleep(100);
			}
		}
		else {//출발 신호 받음 : Departure = True
	#ifdef debug
				printf("running now IsDetected : %d, CourseOut : %d\n\n", IsDetected, CourseOut);
	#endif
			cnt = 0;
			maxcnt = 0;
			if (!CourseOut) {//courseout이 True이고 화면에 장애물 픽셀 안잡히면 탈출(courseout은 우측화면에 흰픽셀 잡힌뒤 사라지면 true)
				printf("not yet course out\n");
				for (i = 40; i<220; i++) {
					for (j = 200; j<310; j++) {//find whiteblock
						if ((imgOrigin->imageData[(i * 320 + j) * 3]>OUT_LINE_Y && imgOrigin->imageData[(i * 320 + j) * 3 + 1]>OUT_LINE_U && imgOrigin->imageData[(i * 320 + j) * 3 + 2]>OUT_LINE_V)) {//firstblocknotdetected&&blackpixel
							for (k = 0; k<5; k++) { //check successive 5 white pixels
								if (!(imgOrigin->imageData[(i * 320 + j) * 3]>OUT_LINE_Y && imgOrigin->imageData[(i * 320 + j) * 3 + 1]>OUT_LINE_U && imgOrigin->imageData[(i * 320 + j) * 3 + 2]>OUT_LINE_V))break;
								if (k == 4)whiteblock = true;
							}
							j = j + k;
						}
					}
					if (whiteblock) {//흰색 한줄을 찾으면
						cnt++;
						whiteblock = false;
					}
					else {
						if (cnt>maxcnt)maxcnt = cnt;
						cnt = 0;
					}
					if (!IsDetected) {//isdetected는 흰색선이다
									  //탈출 신호로 인식하는 우측 흰색 선
						if (maxcnt>12) {
							IsDetected = true;
							break;
	#ifdef IMGSAVE
								sprintf(fileName1, "captureImage/error_signal.png");
								cvSaveImage(fileName1, imgOrigin, 0);
	#endif
						}
					}
				}
			}
			if (IsDetected/*&&maxcnt<8*/)
				CourseOut = true;
			
			pixshadow = 0;

			for (i = SHADOW_RANGE_MIN_Y; i<SHADOW_RANGE_MAX_Y; i++)
				for (j = SHADOW_RANGE_MIN_X; j<SHADOW_RANGE_MAX_X; j++)
					if (imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3] >= SHADOWYMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3] <= SHADOWYMAX && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 1] >= SHADOWUMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 1] <= SHADOWUMAX && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 2] >= SHADOWVMIN && imgOrigin->imageData[(i*RESIZE_WIDTH + j) * 3 + 2] <= SHADOWVMAX)pixshadow++;
			pixshadow = (double)pixshadow / ((SHADOW_RANGE_MAX_X - SHADOW_RANGE_MIN_X)*(SHADOW_RANGE_MAX_Y - SHADOW_RANGE_MIN_Y));

			data = 0;
			databack = 0;
			for (i = 0; i<10; i++) {
				data = data + DistanceSensor(CHANNEL1);
				//usleep(10);
			}
			data = data / 10;

			for (i = 0; i<10; i++) {
				printf("databack before= %d\n", databack);
				databack = databack + DistanceSensor(CHANNEL4);
				printf("databack after = %d\n",databack);
				//usleep(10);
			}
			databack = databack / 10;
			printf("databack = %d\n", databack);
			if (databack>1000) {
				printf("\n\nbackbackbackbackbackbackbackback\n\n");
				CameraXServoControl_Write(1500); 
				Alarm_Write(ON);
				sleep(1);
				Alarm_Write(OFF);
				return 0;
			}
	#ifdef debug
				printf("pixshadow : %lf\n", pixshadow);
				printf("sensor : %d\n", data);
	#endif

			if (data>1000) { 
				speed = 0; 
				printf("sensor condition!\n\n"); 
			}
			else if (CourseOut && (pixshadow<0.05)) {
				CameraXServoControl_Write(1500);
				return 0; 
			}
			else {
				printf("\========================================\n");
				Find_Center(imgResult);
				SteeringServoControl_Write(angle);
				speed = 60;
				speed = speed * (1 - pixshadow * 10);
				printf("pixshadow = %lf\n", pixshadow);
				if (speed<0)
					speed = 0;
			}
			DesireSpeed_Write(speed);
		}
	}
}

//3way 알고리즘
int detect_obstacle2(IplImage* imgResult) { //resultimage 입력받아서, 처리만
	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;

	int i, j, k;

	int left_obj = 0;
	int right_obj = 0;
	int center_obj = 0;

	int desti_lane = 0;

	typedef struct small { // 최대 최소 찾기위한 구조
		int value;
		char name;
	}Small;

	Small smaller1, smaller2, smallest;

	int resultname[40];
	char str_info[50];

	unsigned char status;
	unsigned char gain;

	int channel;
	int data = 0;
	char sensor;
	int tol;
	char byte = 0x80;
	int flag = 0;
	int escape = 0;
	//CarControlInit();

	CvPoint startROI, endROI, scanbound, onethird, twothird;

	startROI.x = 0;	startROI.y = 120;
	//(136,120)  (252,149) // startROI

	onethird.x = 80; onethird.y = 120;

	twothird.x = 240; twothird.y = 200;

	endROI.x = 320; endROI.y = 200;
	// end ROI 

	for (i = startROI.y; i<endROI.y; i++) {
		for (j = startROI.x; j< onethird.x; j++) {
			int px = imgResult->imageData[i*imgResult->widthStep + j];
			if (px == 255) {
				left_obj++;
			}
		}

		for (j = onethird.x; j<twothird.x; j++) {
			int px = imgResult->imageData[i*imgResult->widthStep + j];
			if (px == 255) { //px <130 && px>120
				center_obj++;
			}
		}
		for (j = twothird.x; j< endROI.x; j++) {
			int px = imgResult->imageData[i*imgResult->widthStep + j];
			if (px == 255) {
				right_obj++;
			}
		}
	}

	printf("\n left_obj = %d, center_obj= %d, right_obj= %d \n", left_obj, center_obj, right_obj);

	//==========find smallest========
	if (left_obj <= center_obj) {
		smaller1.value = left_obj;
		smaller1.name = 'L';
	}
	else {
		smaller1.value = center_obj;
		smaller1.name = 'C';
	}

	if (center_obj <= right_obj) {
		smaller2.value = center_obj;
		smaller2.name = 'C';
	}
	else {
		smaller2.value = right_obj;
		smaller2.name = 'R';
	}

	if (smaller1.value <= smaller2.value) {
		smallest.name = smaller1.name;
		smallest.value = smaller1.value;
	}
	else {
		smallest.name = smaller2.name;
		smallest.value = smaller2.value;
	}

	printf(" smallest.name :  %c / smallest.value : %d\n", smallest.name, smallest.value);

	cvRectangle(imgResult, onethird, twothird, CV_RGB(255, 255, 255), 1, 8, 0);
	cvRectangle(imgResult, startROI, endROI, CV_RGB(255, 255, 255), 1, 8, 0); // ROI boundary

	sprintf(resultname, "imgsaved/after_obs_result.png");

	cvSaveImage(resultname, imgResult, 0);

	if (smallest.name == 'L') {
		printf("\n SMALLEST is LEFT \n");
		return -1;
	}
	else if (smallest.name == 'R') {
		printf("\n SMALLEST is RIGHT \n");
		return 1;
	}
	else if (smallest.name == 'C') {
		printf("\n SMALLEST is Center \n");
		return 4;
	}

	else {
		printf("Something is wrong with Smallest value \n");
		return 4;
	}
}

int find_center_in_3way() {

	char orgName[40];
	char result_wy[40];
	char result_obs[40];
	char result_white[40];
	char str[128];
		

	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;

	int num = 0;
	int i = 0;
	int j = 0;
	int data = 0;
	int channel = 1;
	int cc = 0;

	int white_on_right = 0;
	//int left_white_count = 0;
	//int right_white_count = 0;

	bool center_of_3way = false;
	bool middle_of_3way = false;
	bool mode2_out_flag = false;

	int desti_lane = 0;
	int detect_object = 0;

	int tempval = 0; //1114 창환
	int iririr = 0;      //1114
	int enc_val = 0;  //1114
	int enc_cal = 0; //1114

	bool center_flag = false;
	int no_obs = 0;


	IplImage* imgOrigin;
	IplImage* imgResWY;            // TY add 6.27
	IplImage* imgResOBS;            // TY add 6.27
	IplImage* imgReswhite;            // TY add 6.27
	CvFont font;
		

								  // cvCreateImage
	imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
	imgResWY = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
	imgResOBS = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
	imgReswhite = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27

	cvZero(imgResWY);          // TY add 6.27
	cvZero(imgResOBS);
	cvZero(imgReswhite);

	/*   when cannot be detected with binary image(when I have to use white, gray, black)
	for(i = 50;i<200;i++){
	for(j=160; j<320; j++){
	if(imgOrigin->imageData[(i*320+j)*3]>200 && imgOrigin->imageData[(i*320+j)*3+1]>100) {
	imgResult->imageData[i*320+j] = 255;
	new_white_count ++;   //white pixel in right
	}
	else if(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164); //black default
	else imgResult->imageData[i*320+j] = 127;
	}

	}*/

	PositionControlOnOff_Write(UNCONTROL);
	EncoderCounter_Write(0); // 인코더 초기화

	while (1)
	{
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);

		GetTime(&pt1);
		ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

		Frame2Ipl(imgOrigin, imgResWY, 4); //wymix

		pthread_mutex_unlock(&mutex);

		//printf("Find_center in 3way\n\n");

	/*	mode2

	if white line process. -> go to 3_way
		if mode2 on, 
		white pixcel 이 오른쪽으로 갈때까지 좌/우회전 조향
		if 오른쪽 일정범위 넘기면 플래그 지움
	*/
	/*if(mode2_out_flag == false){
		printf("mode_out_flag = false \n");
			
			for (i = 50; i<200; i++) { // y location from 50 to 200 (0<y<240)
				for (j = 160; j<320; j++) { // x location from 150 to 320 (0<x <320)
					if (imgResWY->imageData[i*imgResWY->widthStep + j] == 255) //if white 
						white_on_right++;	//white pixel in right
				}
			}

		// sprintf(str, "Image %d  whiteonright : %d", num, white_on_right);
		// writeonImage(imgResWY, str);
		
		// sprintf(imgResWY, "imgsaved/imgResWY_%d.png", num);          // TY add 6.27
		// num++;
		//  cvSaveImage(imgResWY, imgOrigin, 0);
			
			if(white_on_right<1800){
				printf("white_on_right = %d\n",white_on_right);
			
				SteeringServoControl_Write(2000);
				DesireSpeed_Write(80); // turn left
				white_on_right = 0;
				continue;
			}
			else {
				printf("white_on_right = %d\n",white_on_right);
				DesireSpeed_Write(0);
				Alarm_Write(ON);
				sleep(1);
				Alarm_Write(OFF);
				mode2_out_flag = true;
				continue;
			}
		}	
*/
		Find_Center(imgResWY);

		DesireSpeed_Write(70);
		SteeringServoControl_Write(angle);

	//	sprintf(result_wy, "imgsaved/wymix_%d.png", num);          // TY add 6.27
	//	cvSaveImage(result_wy, imgResWY, 0);
		num++;

		if (center_flag == false) {
			enc_val = EncoderCounter_Read();

			if (enc_val < 20000) {
				for (iririr = 0; iririr<30; iririr++) {
					tempval += DistanceSensor(1);
				}
				tempval = tempval / 30;
				printf("\n ======= Distance %d =====\n", tempval);

				if (tempval > 400) {
					DesireSpeed_Write(0);
					sleep(1);
					Alarm_Write(ON);
					usleep(300000);
					Alarm_Write(OFF);
					usleep(300000);
					Alarm_Write(ON);
					usleep(300000);
					Alarm_Write(OFF);

					SteeringServoControl_Write(1500);
					usleep(10000);

					//   PositionControlOnOff_Write(UNCONTROL); 
					EncoderCounter_Write(0); // 인코더 초기화
					usleep(10000);
					DesireSpeed_Write(-50);

					enc_cal = (-15)* tempval;
					printf("enc_cal = %d\n ", enc_cal);

					while (enc_val > enc_cal) {
						enc_val = EncoderCounter_Read();

						printf("enc_val = %d \n", enc_val);
					}


					/*
					인코더 돌리면서, >= 특정값 이면 breka;
					거리 돌리면서 >= 특정값 이면 break;

					if(){
					return 2;

					}
					*/

					//while(EncoderCounter_Read() <= ){}

					// DesireSpeed_Write(-30);
					// usleep(100);
					// DesireSpeed_Write(-70);
					// sleep(1);

					DesireSpeed_Write(0);
					usleep(1000);

					CameraYServoControl_Write(1600);    //camera heading up
					sleep(1);

					pthread_mutex_lock(&mutex);
					pthread_cond_wait(&cond, &mutex);

					GetTime(&pt1);
					ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

					pthread_mutex_unlock(&mutex);
					//====================================================
					Frame2Ipl(imgOrigin, imgResOBS, 7);

					desti_lane = detect_obstacle2(imgResOBS);

					sprintf(orgName, "imgsaved/orgName.png", num);          // TY add 6.27
					sprintf(result_obs, "imgsaved/result_obs.png", num);          // TY add 6.27

					num++;

					cvSaveImage(orgName, imgOrigin, 0);
					cvSaveImage(result_obs, imgResOBS, 0);
					break;
				}
			}
			else center_flag = true;
		}

		else { //center_flag ==true
			no_obs = white_line_process(imgOrigin);
			if (no_obs == 1) return 0;
		}
	}
	return desti_lane;
}

int check_3way()
{ 
   int channel_leftNow = 0;
   int channel_leftPrev = 0;
   int channel_rightNow = 0;
   int channel_rightPrev = 0;
   
   int difference_left = 0;
   int difference_right = 0;

   channel_leftNow = filteredIR(LEFT);
   channel_rightNow = filteredIR(RIGHT);
   difference_left = channel_leftNow - channel_leftPrev;
   difference_right = channel_rightNow - channel_rightPrev;
   channel_leftPrev = channel_leftNow;
   channel_rightPrev = channel_rightNow;
   printf("difference_left = %d\n", difference_left);
   printf("difference_right = %d\n", difference_right);
   
   if(difference_left > 120 && difference_left < 250) return 1;
   else if (difference_left > - 300 && difference_left < - 120) return 2;

   if (difference_right > 120 && difference_right < 250) return 3;
   else if (difference_right > -300 && difference_right < -120) return 4;

}

int Threeway_hardcoding(IplImage* imgResult, int turn)
{
   int tw_speed;
   int tw_straight_speed = 115;
   int tw_curve_speed = 65;
   static int flag_sensor = 0;
   //static bool Detector_YL = false;

   int data = 0;
   int channel;
   //int angle;
   int tw_angle;
   int weight_tw = 100;//곡선 encoder weight
   int weight_tw1 = 80;//직진 encoder weight
   int t = turn;
   static int flag_tw = 1;//3차선 전용 flag
   static int flag_YL = 0;//3차선 노란색 검출 flag

   int x = 0, y = 0; //x 축 및 y 축
   int height_y = 240; // frame2IPL y축 
   int width_x = 320; // frame2IPL x축 
   int YELLOW_TW = 0; // 3차선 노란선 검출용 변수
   int Check_YL = height_y * width_x / 9 * 0.1; // 9분의 1 ROI 를 기준으로 10퍼센트이상 노란선 검출 시 멈추게 해주는 check 변수   
   //853
   unsigned char status;
   unsigned char gain;
   int position_now = 0;
   int position_zero = 0;
   int tol;

   bool FindLine = false;

   //int tempval = 0;
   //int iririr = 0;

   color = 4;//주행 코드를 위한 threshhold 값 변경

           //총 픽셀은 320 *240 = 76800
           //총 픽셀은 320 *240 = 76800
   for (y = height_y / 3; y <= height_y / 3 * 2; y++)
   {
      for (x = width_x / 3; x <= width_x / 3 * 2; x++)
      {
         if (imgResult->imageData[y * width_x + x] == whitepx)//Find white pixels
         {
            YELLOW_TW++;
         }
      }
   }

   printf("YELLOW_TW = %d\n", YELLOW_TW);
   //947



   //CarControlInit();
   //SpeedControlOnOff_Write(CONTROL);
   //PositionControlOnOff_Write(UNCONTROL);
   //DesireSpeed_Write(tw_straight_speed);

   //gain = 20;
   //PositionProportionPoint_Write(gain);

   position_now = EncoderCounter_Read();      //EncoderCounter_Read()값은 다른함수에서 받아올예정.
                                    //따라서 바로 받아와서 쓸 수 있도록 position_now로 사용.


   if (flag_tw == 1)
   {
      EncoderCounter_Write(position_zero);
      tw_angle = 1500;//직진
      angle = tw_angle;
      //speed = tw_straight_speed;
      speed = 70;
      flag_tw++;
   }

   else if (flag_tw == 2)
   {
      if (position_now > 10 * weight_tw1)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 3)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500 - t * 430;//오른쪽 조향 1100, 왼쪽 조향 1900; 이것은 t값에 따라 바뀜!! 아래 설명 생략
      angle = tw_angle;
      speed = tw_curve_speed;

      flag_tw++;
   }

   else if (flag_tw == 4)
   {
      if (position_now >  45 * weight_tw)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 5)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;//직진
      angle = tw_angle;
      speed = tw_straight_speed;

      flag_tw++;
   }

   else if (flag_tw == 6)
   {
      if (position_now > 30 * weight_tw1)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 7)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500 + t * 430;//왼쪽 조향  1900, 오른쪽 조향 1100; 위와 똑같은 이론
      angle = tw_angle;
      speed = tw_curve_speed;

      flag_tw++;
   }

   else if (flag_tw == 8)
   {
      if (position_now > 45 * weight_tw)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 9)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;//직진
      angle = tw_angle;
      speed = tw_straight_speed;
      flag_tw++;
   }




   else if (flag_tw == 10)
   {
      if (YELLOW_TW > Check_YL && position_now > 100 * weight_tw1)
      {
      #ifdef DEBUG_TW
         printf("스톱!\n"); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
      #endif
         printf("==position now = %d==\n",position_now);
         flag_YL++;
         flag_tw = 0;
         DesireSpeed_Write(0);
         Alarm_Write(ON);
         usleep(2000000);
      }

      
      else if (t == -1 || t == 1) //flag_sensor = 0 ;
      {
      if (t == 1){
         if (flag_sensor = 1)
         {
            printf("flag_sensor = %d\n", flag_sensor);         
            if (check_3way() == 2)
            {
               flag_sensor++;
               flag_tw++;
               Alarm_Write(ON);
            }
         }
         else if(flag_sensor==0)
         {
            printf("flag_sensor = %d\n", flag_sensor);
            if (check_3way() == 1) //12cm ~ 25cm 거리 센서 측정
            flag_sensor++;
            Alarm_Write(ON);
         }
      }
      else if (t== -1){
         
         if (flag_sensor = 1)
         {
            printf("flag_sensor = %d\n", flag_sensor);         
            if (check_3way() == 4)
            {
               flag_sensor++;
               flag_tw++;
               Alarm_Write(ON);
            }
         }
         else if(flag_sensor==0)
         {
            printf("flag_sensor = %d\n", flag_sensor);
            if (check_3way() == 3)
            flag_sensor++;
            Alarm_Write(ON);
         }
      }
   }

         printf("Turn Turn HyunDAE~~~~!!!\n");
         Find_Center(imgResult);
         printf("angle : %d, speed : %d\n", angle, speed);
         speed = tw_straight_speed;

   }

   else if (flag_tw == 11)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500 + t * 450;//왼쪽 조향  1900, 오른쪽 조향 1100; 위와 똑같은 이론
      angle = tw_angle;
      speed = tw_curve_speed;

      flag_tw++;
   }

   else if (flag_tw == 12)
   {
      if (position_now > 45 * weight_tw)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 13)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;//직진
      angle = tw_angle;
      speed = tw_straight_speed;

      flag_tw++;
   }

   else if (flag_tw == 14)
   {
      if (position_now > 35 * weight_tw1)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 15)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500 - t * 450;//오른쪽 조향 1100, 왼쪽 조향 1900; 이것은 t값에 따라 바뀜!! 아래 설명 생략
      angle = tw_angle;
      speed = tw_curve_speed;

      flag_tw++;
   }

   else if (flag_tw == 16)
   {
      if (position_now > 45 * weight_tw)
         flag_tw++; // flag 1 증가

#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 17)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;//직진
      angle = tw_angle;
      speed = tw_straight_speed;

      flag_tw++;
   }

   else if (flag_tw == 18)
   {
      if (position_now > 35 * weight_tw1)
         flag_tw++; // flag 1 증가
      speed = 0;
#ifdef DEBUG_TW
      printf("EncoderCounter_Read() = %d\n", EncoderCounter_Read()); // EncoderCounter_Read를 쓰면 느려지니 디버깅할떄만 쓰기
#endif
   }

   else if (flag_tw == 19)//후진
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;
      angle = tw_angle;
      speed = -70;

      flag_tw++;
   }

   else if (flag_YL == 20)//후진 엔코더 적용
   {
      if (position_now < -(10 * weight_tw1))
         flag_tw++;
   }

   else if (flag_YL == 1)//후진
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;
      angle = tw_angle;
      speed = -70;

      flag_YL++;
   }

   else if (flag_YL == 2)//후진 엔코더 적용
   {
      if (position_now < -(70 * weight_tw1))
         flag_YL++;
   }

   else if (flag_YL == 3)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500 + t * 450;
      angle = tw_angle;
      speed = tw_curve_speed;

      flag_YL++;
   }

   else if (flag_YL == 4)
   {
      if (position_now > 30 * weight_tw)
         flag_YL++;
   }

   else if (flag_YL == 5)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;
      angle = tw_angle;
      speed = tw_straight_speed;

      flag_YL++;
   }

   else if (flag_YL == 6)
   {
      if (position_now > 20 * weight_tw1)
         flag_YL++;
   }

   else if (flag_YL == 7)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500 - t * 450;
      angle = tw_angle;
      speed = tw_curve_speed;

      flag_YL++;
   }

   else if (flag_YL == 8)
   {
      if (position_now > 45 * weight_tw)
         flag_YL++;
   }

   else if (flag_YL == 9)
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;
      angle = tw_angle;
      speed = tw_curve_speed; // 마지막 단계 속도 줄이기

      flag_YL++;
   }

   else if (flag_YL == 10)
   {
      if (position_now > 10 * weight_tw1)
         flag_YL++;
   }
   else if (flag_YL == 11)//후진
   {
      EncoderCounter_Write(position_zero);//엔코더 초기화
      tw_angle = 1500;
      angle = tw_angle;
      speed = -70;

      flag_YL++;
   }

   else if (flag_YL == 12)//후진 엔코더 적용
   {
      if (position_now < -(10 * weight_tw1))
         flag_YL++;
   }

   else if (flag_tw == 21 || flag_YL == 13)
   {
      speed = 0;
      DesireSpeed_Write(speed);
      printf("I gonna stop \n");
      color = 0;
      return 1;
   }
   return 0;
}

//flag에 따른 모듈 변화(1 = 회전교차로 2 = 3way 3 = 신호등)
//TODO flag 유지 or 함수가 끝났으면 flag = 0 으로 바꿔주기
int flag_module(int flag, IplImage* imgResult) {//TODO : 구간 나가면 return 돌려주기
	static int traffic = 0;
	static int threeway_flag = 0;
	static int destiny;
	int rotary_end = 1;
	int threeway_end = 0;
	//rotary
	if (flag == 1) {
		printf("rotary module\n\n");
		rotary_end =rotary();
		if (rotary_end == 0) {
			return 0;
		}
		return 1;
	}
	//3차선
	else if (flag == 2) {

		if (threeway_flag == 0) {
			printf(" \n===== 3way detected====\n");
			printf(" \n------------------------\n");

			Alarm_Write(ON);
			DesireSpeed_Write(0);
			sleep(2);
			Alarm_Write(OFF);

			destiny = find_center_in_3way();
			printf("Destiny = %d\n ", destiny);
			
			CameraYServoControl_Write(1800);//sleep(100);
			
			if (destiny == 0) {
				return 0;
			}
			else
				threeway_flag = 1;
		}
		else if (threeway_flag == 1) {
			printf(" \n===== 3way =========\n");
			threeway_end = Threeway_hardcoding(imgResult, destiny);//민성 알고리즘
			if (threeway_end == 1){
				threeway_flag = 2;
			}
			return 2;
		}
		else {
			printf("===== 3way finished====\n");
			return 0;
		}
		return 2;
	}
	//신호등
	else if (flag == 3) {
		printf("trafficlight module\n\n");
		printf("traffic = %d\n", traffic);
		if (traffic == 0)
			traffic = Traffic_Light(imgResult);
		else if (traffic == 1 || traffic == 2)
			traffic = AfterTraffic(traffic, imgResult);
		else if (traffic == 3 || traffic == 4)
			traffic = endMission(imgResult);
		else if (traffic == 5)
			speed = 0;
		return 3;
	}
}
///////////////////////////////////////////////////////////////////////////////
/////////////////////white count에 대한 모듈들//////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
// Parking Merge
// Code from HG Cha
////////////////////////////////////////////////////////////////////////////////////////////

int filteredIR(int num) // 필터링한 적외선 센서값
{
	int i;
	int sensorValue = 0;

	if (distance_warmming){			//초기 시작후, DistanceValue에 쓰레기값들이 있을때 Filtered_IR이 잘못된 값을 밷는걸 방지
		if(num == 5) {// 왼쪽
			for(i=0; i<15; i++)
			{
				sensorValue += DistanceValue[0][i];
			}
		}
		else if(num == 3) // 오른쪽
		{
			for(i=0; i<15; i++)
			{
				sensorValue += DistanceValue[1][i];
			}
		}
		else if(num == 4) // 후방
		{
			for(i=0; i<15; i++)
			{
				sensorValue += DistanceValue[2][i];
			}
		}
		else if(num == 1) // 전방
		{
			for(i=0; i<15; i++)
			{
				sensorValue += DistanceValue[3][i];
			}
		}
		sensorValue /= 15;
		return sensorValue;
	}
	else
		return 0; //초기 시작후, DistanceValue에 쓰레기값들이 있을때 Filtered_IR이 잘못된 값을 밷는걸 방지
}

void init_parking()
{
    first_left_detect = FALSE;
    second_left_detect = FALSE;
    third_left_detect = FALSE;

    first_right_detect = FALSE;
    second_right_detect = FALSE;
    third_right_detect = FALSE;
}

void vertical_parking_left() // ?섏쭅 二쇱감 
{	
	EncoderCounter_Write(0);
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() >= -8500)
	{
		DesireSpeed_Write(-encoder_speed);
	}   

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	//PSD 센서 이용
	SteeringServoControl_Write(1470);
	while(filteredIR(4) <= 3000)
	{
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 3000)
	{
		DesireSpeed_Write(encoder_speed);
	} // ?꾩쭊

	EncoderCounter_Write(0);
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() <= 8500)
	{
		DesireSpeed_Write(encoder_speed);
	} // 90???뚯쟾

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 3000)
	{
		DesireSpeed_Write(encoder_speed);
	} // ?꾩쭊

	init_parking();
}

void vertical_parking_right() // ?섏쭅 二쇱감 
{	
	EncoderCounter_Write(0);
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() >= -8000)
	{
		DesireSpeed_Write(-encoder_speed);
	} // 90???뚯쟾  

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	//PSD 센서 이용
	SteeringServoControl_Write(1470);
	while(filteredIR(4) <= 3000)
	{
		DesireSpeed_Write(-50);
	}

	DesireSpeed_Write(0);
	CarLight_Write(ALL_ON);
    usleep(1000000);
    CarLight_Write(ALL_OFF);
    Alarm_Write(ON);
    usleep(100000);
	Alarm_Write(OFF);
	
	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 2000)
	{
		DesireSpeed_Write(encoder_speed);
	} // ?꾩쭊

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() <= 9000)
	{
		DesireSpeed_Write(encoder_speed);
	} // 90???뚯쟾

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 3000)
	{
		DesireSpeed_Write(encoder_speed);
	} 

	init_parking();
}

void parallel_parking_right()
{
	/////////////////////////////////// 들어가기 ////////////////////////////////////////////
	/*
	EncoderCounter_Write(0);
	SteeringServoControl_Write(1470);
	while(EncoderCounter_Read() <= 500)
	{
		DesireSpeed_Write(encoder_speed);
	} 
	*/
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() >= -5000)
	{
		DesireSpeed_Write(-encoder_speed);
	}   

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);
	//PSD 센서 이용
	SteeringServoControl_Write(1470);
	while(filteredIR(4) <= 3000)
	{
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() >= -6000)
	{
		DesireSpeed_Write(-encoder_speed);
	}

	DesireSpeed_Write(0);
	CarLight_Write(ALL_ON);
    usleep(1000000);
    CarLight_Write(ALL_OFF);

    Alarm_Write(ON);
    usleep(100000);
	Alarm_Write(OFF);
	
 	////////////////////////////////////////// 나가기 //////////////////////////// 

 	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() <= 6000)
	{
		DesireSpeed_Write(encoder_speed);
	}

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);
	while(EncoderCounter_Read() <= 2500)
	{
		DesireSpeed_Write(encoder_speed);
	}

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() <= 5000)
	{
		DesireSpeed_Write(encoder_speed);
	} 

	init_parking();
}

void parallel_parking_left()
{
	/////////////////////////////////// 들어가기 ////////////////////////////////////////////
	/*
	EncoderCounter_Write(0);	
	SteeringServoControl_Write(1470);
	while(EncoderCounter_Read() <= 500)
	{
		DesireSpeed_Write(encoder_speed);
	} 
	*/
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);	
	while(EncoderCounter_Read() >= -5000)
	{
		DesireSpeed_Write(-encoder_speed);
	}   
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);	
	//PSD 센서 이용
	SteeringServoControl_Write(1470);
	while(filteredIR(4) <= 3000)
	{
		DesireSpeed_Write(-50);
	}  
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);	
	while(EncoderCounter_Read() >= -6000)
	{
		DesireSpeed_Write(-encoder_speed);
	}
	
	DesireSpeed_Write(0);
	CarLight_Write(ALL_ON);
	usleep(1000000);
	CarLight_Write(ALL_OFF);
	
	Alarm_Write(ON);
	usleep(100000);
	Alarm_Write(OFF);
	
	////////////////////////////////////////// 니가기 /////////////////////////////////
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() <= 6000)
	{
		DesireSpeed_Write(encoder_speed);
	}
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);	
	while(EncoderCounter_Read() <= 2500)
	{
		DesireSpeed_Write(encoder_speed);
	}
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() <= 5000)
	{
		DesireSpeed_Write(encoder_speed);
	} 

	init_parking();
}

void check_parking()
{ 
	static int left_flag = 0;
	static bool channel_warmming
	int encoder_parking = 0; //주차시 벽인식후 엔코더스텝수 이상 주행해도 벽이 추가로 검출이 안되는지 확인용 변수

	channel_leftNow = filteredIR(LEFT);
	channel_rightNow = filteredIR(RIGHT);

	if(channel_leftNow != 0 && channel_leftPrev != 0 && channel_rightNow != 0 && channel_rightPrev != 0)	//차량 시작후, 채널초기값 0으로인해 difference_left,right 값이 잘못 표기됨을 방지
		channel_warmming = TRUE;

	if(channel_warmming){
		difference_left = channel_leftNow - channel_leftPrev;
		difference_right = channel_rightNow - channel_rightPrev;
	}
	
	channel_leftPrev = channel_leftNow;
	channel_rightPrev = channel_rightNow;
	printf("difference_left = %d\n", difference_left);
	printf("difference_right = %d\n", difference_right);
	encoder_parking = EncoderCounter_Read();
	
	if(first_left_detect == FALSE && difference_left > 170){
		printf("\n\n-------------jumped over the threshold by %d-------------\n", difference_left);
		EncoderCounter_Write(0);
		left_flag = 1;
	}
	else if(left_flag == 1 && first_left_detect == FALSE && difference_left < 50){
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			printf("\n\nFIRST_LEFT_DETECT\n\n\n");
			EncoderCounter_Write(0);
			first_left_detect = TRUE;
	}
	else if(left_flag == 1 && first_left_detect == TRUE && second_left_detect == FALSE && difference_left < -300){
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			left_flag = 2;
	}
	else if(left_flag == 2 && first_left_detect == TRUE && second_left_detect == FALSE && difference_left > -50){
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			printf("\n\nSECOND_LEFT_DETECT\n\n\n");
			EncoderCounter_Write(0);
			second_left_detect = TRUE;
	}
	else if(left_flag == 2 && first_left_detect == TRUE && second_left_detect == TRUE && third_left_detect == FALSE && difference_left > 170){
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			left_flag = 3;
	}
	else if(left_flag == 3 && first_left_detect == TRUE && second_left_detect == TRUE && third_left_detect == FALSE && difference_left < 50){
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			printf("\n\nTHIRD_LEFT_DETECT\n\n\n");
			third_left_detect = TRUE;
			left_flag = 4;
	}
	else if(left_flag == 4){			//주차공간이라 인식한 경우, 주차시작
		parking_space = encoder_parking;
		printf("\n\n PARKING SPACE : %d", parking_space);
		CarLight_Write(ALL_ON);
    	usleep(100000);
    	CarLight_Write(ALL_OFF);
		Alarm_Write(ON);
		usleep(50000);
		Alarm_Write(OFF);
		if(parking_space > 7000)
			parallel_parking_left();
		else
			vertical_parking_left();
	}
	else if(encoder_parking > 12000){		//엔코더 스텝수가 12000이상 초과할 경우, 앞선 벽들 인식은 잘못 인식한걸로 간주하고 초기화.
		left_flag = 0;
		first_left_detect = FALSE;
		second_left_detect = FALSE;
		third_left_detect = FALSE;
		printf("\n\n=================FLAG CLEAR!=================\n\n\n");
	}
}

// 주차 함수 끝
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


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

    int y_start_line = 140;     //y_start_line과 y_end_line 차는 line_gap의 배수이어야 함.
    int y_end_line = 120;
    int y_high_start_line = 100;
    int y_high_end_line = 80;

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

    int line_gap = 5;  //line by line 스캔시, lower line과 upper line의 차이는 line_gap px
    int tolerance = 40; // center pixel +- tolerance px 내에서 라인검출시 for문 종료 용도
    int high_tolerance = 60;

    float low_line_weight = 320; // control angle weight
    float high_line_weight = 80;
    float control_angle = 0;
	int high_ignore_angle = 100;

    int left[240] = {0};
    int right[240] = {imgResult->width-1};
    float left_slope[2] = {0.0};
    float right_slope[2] = {0.0};

    static bool max_turn_ready_left = false ;
    static bool max_turn_ready_right = false ;

    bool continue_turn_left = false;
    bool continue_turn_right = false;

    printf("turn_left_max = %d , turn_right_max = %d\n",turn_left_max,turn_right_max);
    printf("max_turn_ready_left = %d , max_turn_ready_right = %d\n",max_turn_ready_left,max_turn_ready_right);

    for(i = y_start_line ; i>y_end_line ; i=i-line_gap){
	    if (turn_right_max == true){        // max turn right시 우측부터 좌측차선 읽기 시작
			j = imgResult->width - 1;
            printf("sweeping starting point : right\n");
        }
        else if(max_turn_ready_right == true){        // max turn right시 우측 3/4부터 좌측차선 읽기 시작
			j = imgResult->width*3/4;
            printf("sweeping starting point : 3/4 right\n");
        }
		else if(turn_left_max == true || max_turn_ready_left == true){      // max turn left 위해 왼쪽부터 우측차선읽으면 좌측차선 검색 불필요
			j = 0;
            printf("left line searching skip!\n");
        }
        else{                                                               // 평상시엔 중앙에서 시작
            j = (imgResult->width) / 2;
            printf("sweeping starting point : center\n");
        }
        for(; j>0 ; j--){                            //Searching the left line point
                left[y_start_line-i] = j;
                if(imgResult->imageData[i*imgResult->widthStep + j] == whitepx){
                    for( k = 0 ; k < low_line_width ; k++){                     //차선이 line_width만큼 연속으로 나오는지 확인
                        if( j-k <= 0)
                          k = low_line_width - 1;
                        else if(imgResult->imageData[i*imgResult->widthStep + j - k] == whitepx)
                          continue;
                        break;
                    }
                    if(k == low_line_width - 1){
                      valid_left_amount++;
                      break;
                    }
                }
		}
		if (turn_left_max == true){          // max turn left시 좌측부터 우측차선 읽기 시작
			j = 0 ; 
            printf("sweeping starting point : left\n");
        }
        else if(max_turn_ready_left == true){        // max turn right시 좌측 1/4부터 좌측차선 읽기 시작
			j = imgResult->width*1/4;
            printf("sweeping starting point : 1/4 right\n");
        }
		else if(turn_right_max == true || max_turn_ready_right == true){      // max turn left 위해 왼쪽부터 우측차선읽으면 좌측차선 검색 불필요
			j = imgResult->width - 1;
            printf("right line searching skip!\n");
        }
        else{                                                               // 평상시엔 중앙에서 시작
            j = (imgResult->width) / 2;
            printf("sweeping starting point : center\n");
        }
        for(; j<imgResult->width ; j++){             //Searching the right line point
                right[y_start_line-i] = j;
                if(imgResult->imageData[i*imgResult->widthStep + j] == whitepx){
                      for( k = 0 ; k < low_line_width ; k++){                   //차선이 line_width만큼 연속으로 나오는지 확인
                        if( j + k >= imgResult->widthStep)
                          k = low_line_width - 1;
                        else if(imgResult->imageData[i*imgResult->widthStep + j + k] == whitepx)
                          continue;
                        break;
                    }
                    if(k == low_line_width - 1){
                      valid_right_amount++;
                      break;
                    }
                }
        }

        if(left[y_start_line-i]>((imgResult->width/2)-tolerance)){     //검출된 차선이 화면중앙부근에 있는경우, 차선검출 종료후 반대방향으로 최대조향 flag set
            if(turn_left_max == false){
            	printf("continue_turn_right set!\n");
                continue_turn_right = true;
                break;
            }
        }
        else if(right[y_start_line-i]<((imgResult->width/2)+tolerance)){
            if(turn_right_max == false){
            	printf("continue_turn_left set!\n");
                continue_turn_left = true;
                break;
            }
        }
    }

    if (continue_turn_left == false && continue_turn_right == false){          //turn max flag가 false인 경우 1. 직선 2. 과다곡선
        printf("continue_turn_flag_off__1__\n");
        if(turn_left_max == true){                                  //2. 과다곡선인 경우, 차선이 정상검출범위내로 돌아올때까지 턴 유지
            for(i = imgResult->widthStep -1 ; i > (imgResult->width/2) + line_tolerance ; i--){
            	if(imgResult->imageData[y_start_line*imgResult->widthStep + i] == whitepx){
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
            	if(imgResult->imageData[y_start_line*imgResult->widthStep + i] == whitepx){
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

            if(valid_left_amount > 1){                                          //좌측 차선 기울기 계산
                left_slope[0] = (float)(left[0] - left[(valid_left_amount-1)*line_gap])/(float)(valid_left_amount*line_gap);
            }
            else left_slope[0] = 0;

            if(valid_right_amount > 1){                                          //우측 차선 기울기 계산
                right_slope[0] = (float)(right[0] - right[(valid_right_amount-1)*line_gap])/(float)(valid_right_amount*line_gap);
            }
            else right_slope[0] = 0;
            
            control_angle = (left_slope[0] + right_slope[0])*low_line_weight;        //차량 조향 기울기 계산
        }

            printf("\nleft line = ");
            for(i=0;i<valid_left_amount;i++)printf("%d  ",left[i*line_gap]);
            printf("    valid left line = %d\n",valid_left_amount);
            printf("right line = ");
            for(i=0;i<valid_right_amount;i++)printf("%d ",right[i*line_gap]);
            printf("    valid right line = %d\n",valid_right_amount);
            printf("left_slope : %f ,right_slope : %f   	",left_slope[0],right_slope[0]);
            printf("Control_Angle_low : %f \n\n",control_angle);

    turn_left_max = continue_turn_left;             //현재 프레임에서 최대조향이라고 판단할 경우, 최대조향 전역변수 set.
    turn_right_max = continue_turn_right;

    if(turn_left_max == false && turn_right_max == false && control_angle == 0.0 ){   //아랫쪽차선 검출시도후, 직진주행 하는 경우,
        printf("Does not detected low_lain\n");
        for(i = y_high_start_line ; i>y_high_end_line ; i=i-line_gap){
          for(j=(imgResult->width)/2 ; j>0 ; j--){                            //Searching the left line point
                  left[y_high_start_line-i] = j;
                  if(imgResult->imageData[i*imgResult->widthStep + j] == whitepx){
                      for( k = 0 ; k < high_line_width ; k++){                     //차선이 line_width만큼 연속으로 나오는지 확인
                          if( j-k <= 0)
                            k = high_line_width - 1;
                          else if(imgResult->imageData[i*imgResult->widthStep + j - k] == whitepx)
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
                  if(imgResult->imageData[i*imgResult->widthStep + j] == whitepx){
                        for( k = 0 ; k < high_line_width ; k++){                   //차선이 line_width만큼 연속으로 나오는지 확인
                          if( j + k >= imgResult->widthStep)
                            k = high_line_width - 1;
                          else if(imgResult->imageData[i*imgResult->widthStep + j + k] == whitepx)
                            continue;
                          break;
                      }
                      if(k = high_line_width - 1){
                        valid_high_right_amount++;
                        break;
                      }
                  }
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
  
      if(abs(control_angle)>high_ignore_angle){    //위쪽차선에서 과하게 꺾을경우, 방지 ; 코너에서 인코스로 들어오는걸 방지
		if (control_angle > 0) {
            max_turn_ready_left = true;
            printf("max_turn_ready_left set!\n");
        }
		else {
            max_turn_ready_right = true;
            printf("max_turn_ready_right set!\n");
	    }
        control_angle = 0;
      }
      else{
          if (max_turn_ready_left || max_turn_ready_right){
          max_turn_ready_left = false;
          max_turn_ready_right = false;
          printf("max_turn_ready_flag clear!\n");
          }
      }
    }

    if (turn_left_max == true)                      //차량 조향각도 판별
        angle = 2000;
    else if (turn_right_max == true)
        angle = 1000;
    else{
        angle = 1500 + control_angle ;                                  // Range : 1000(Right)~1500(default)~2000(Left)
		angle = angle>2000? 2000 : angle<1000 ? 1000 : angle;           // Bounding the angle range
    }

    #ifdef SPEED_CONTROL
        if(angle<1200||angle>1800)      //직선코스의 속도와 곡선코스의 속도 다르게 적용
           speed = curve_speed;
        else if(max_turn_ready_left || max_turn_ready_right)
            speed = (straight_speed + curve_speed)/2;
        else
           speed = straight_speed;
    #endif

    #ifdef ROI
        for(i=0;i<imgResult->widthStep;i++) imgResult->imageData[y_start_line*imgResult->widthStep + i] = 255;
        for(i=0;i<imgResult->widthStep;i++) imgResult->imageData[y_end_line*imgResult->widthStep + i] = 255;
		for(i=0;i<imgResult->widthStep;i++) imgResult->imageData[y_high_start_line*imgResult->widthStep + i] = 255;
		for(i=0;i<imgResult->widthStep;i++) imgResult->imageData[y_high_end_line*imgResult->widthStep + i] = 255;
    #endif
}


void ControlThread(void *unused){
	int i = 0;
	int line = 0;
	int is_rotary_traffic = 0;
	int module_process = 0;
	static int stop_check = 0;
	static int flag = 0;
	static int stop_line_detected = 0;
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

	/*
		CarLight_Write(ALL_OFF);
		angle = 1500;                       // Range : 600(Right)~1500(default)~2400(Left)
    	CameraXServoControl_Write(angle);
    	angle = 1800;                       // Range : 1200(Up)~1500(default)~1800(Down)
		CameraYServoControl_Write(angle);
	*/

	channel_leftPrev = filteredIR(LEFT);
	channel_rightPrev = filteredIR(RIGHT);

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
		// int line = isLine();
		/*
			긴급 탈출은 적외선 값을 읽어서 독자 쓰레드를 파야 할지도 모른다고 코멘트 주셨습니다.
			주차는 독자 로직으로 해야할것 같다고 동재선배님이 코멘트 주셨습니다.
		*/
		//if(line == 1 || line == 2) angle = 1500 + 500 * (3 - 2 * line);		
		//else
		if (flag > 0) {
			flag = flag_module(flag, imgResult);
		}

		//////정지선 판단까지 & 정지 후 교차로,신호등 판단
		else if (stop_check == 1) {
			if (stop_line_detected == 1) { // 신호등인지 로터리인지 판단
				is_rotary_traffic = detect_signal(imgResult);
				if (is_rotary_traffic == 1) {
					flag = 3;//신호등
					stop_check = 0;
					
				}
				else if (is_rotary_traffic == 2) {
					flag = 1;//rotary
					stop_check = 0;
					color = 0;
				}
			}
			//정지선
			else if (speed>0) {
				stop_line_detected = detectStop(imgResult);//정지선 검출전 주행 정지선 밟으면 return
				printf("detect stopline\n\n");
			}
			else {
				Find_Center(imgResult);
			}

			if (stop_line_detected > 0 && color == 0) {
				color = 1;
				//color = 5;//check black
				DesireSpeed_Write(0);
				CameraYServoControl_Write(1600);
			}
		}

		//흰색 인식 후 점선(3차선)인지 정지선인지 판단
		else if (white_count > 500) {//2000bef for test down 
			speed = 60;
			DesireSpeed_Write(speed);
			module_process = white_line_process(imgOrigin);
			//3차선
			if (module_process == 2) {
				flag = 2;
			}
			else if (module_process == 1){
				stop_check = 1;
				printf("white line detected\n\n");	
			}
			else {
				printf("Error!! 0000000\n");
				// 주차영역인지 확인
				//check_parking();
				printf("\n\nFind_Center!!\n\n");
				Find_Center(imgResult);
				speed = 60;
			}		
		}

		//평상시 Find_Center 작동
		else {
			// 주차영역인지 확인
			//check_parking();
			printf("\n\nFind_Center!!\n\n");
			Find_Center(imgResult);
		}

		//급정지면 무조건 정지!
		if(red_count>280*10*0.4){
			printf("red_count!\n\n");
			speed = 0;
		} 
		
		SteeringServoControl_Write(angle);
		DesireSpeed_Write(speed);
		//===================================
		//  LOG 파일 작성
        //writeLog(i);
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

void LineThread(void *unused) 
{
	while(1)
	{
		int dir = 0;
		int sensor = LineSensor_Read();        // black:1, white:0
		int s = 0;
		int c[8];
		int i=0;
		int angle = 0;

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
			isOverLine = true;
		} else if( c[5]==1 && c[6]==0 || c[7]==0 ){
			angle = 1000;
			dir = 2;
			isOverLine = true;
		}

		if(emergencyReturnFlag)
		{
			if(dir == 1 || dir == 2)
			{
				angle = 1500 + 500 * (3 - 2 * dir);	
				DesireSpeed_Write(100);
				SteeringServoControl_Write(angle);
			}
		}
		/*
		if(isOverLine) printf("true\n");
		else printf("false\n");
		*/
	}
}

void DistanceThread(void *unused) // 0: 왼쪽 1: 오른쪽 2: 후방 3: 전방
{
	dThreadTime = 0;
	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;

	int i, j;
	while(1)
	{
		GetTime(&pt1);
		ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

		if(distanceFlag)
		{
			for(i=0; i<15; i++) 
			{
					DistanceValue[0][i] = DistanceSensor(5); // 왼쪽
					DistanceValue[1][i] = DistanceSensor(3); // 오른쪽
					DistanceValue[2][i] = DistanceSensor(4); // 후방
					DistanceValue[3][i] = DistanceSensor(1); // 전방
					printf("DistanceValue = %d  %d  %d  %d\n", DistanceValue[0][i], DistanceValue[1][i], DistanceValue[2][i], DistanceValue[3][i]);
					usleep(10000);
			}
		}
		GetTime(&pt2);
		ptime2 = (NvU64)pt2.tv_sec * 1000000000LL + (NvU64)pt2.tv_nsec;
		//printf("--------------------------------operation time=%llu.%09llu[s]\n", (ptime2 - ptime1) / 1000000000LL, (ptime2 - ptime1) % 1000000000LL);
		dThreadTime += (ptime2 - ptime1) / 1000000000LL;
		distance_warmming = TRUE;							//distanceThread가 DistanceValue배열을 모두 채웠는지 확인용 flag. 처음시작시 배열이 모두 0이기에 잘못사용되는걸 막기위함.
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

	pthread_t cntThread, lineThread, distanceThread;

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
	gain = 50;
	SpeedPIDProportional_Write(gain);
	//I-gain
	gain = 40;
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
	CarLight_Write(FRONT_ON);
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
	//pthread_create(&lineThread, NULL, &LineThread, NULL);
	pthread_create(&distanceThread, NULL, &DistanceThread, NULL);

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
