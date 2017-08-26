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
//0822 first commit
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


////////////////////////////////////////////////////////////////////////////

#define SERVO_CONTROL     // TY add 6.27
              // To servo control(steering & camera position)
#define IMGSAVE

////////////////////////////////////////////////////////////////////////////


#define VIP_BUFFER_SIZE 6
#define VIP_FRAME_TIMEOUT_MS 100
#define VIP_NAME "vip"

#define MESSAGE_PRINTF printf

#define CRC32_POLYNOMIAL 0xEDB88320L

#define RESIZE_WIDTH  320
#define RESIZE_HEIGHT 240

#define whitepx 255

int angle = 1500;
int speed = 100;
int colorFlag = 0;

#define OBS_LEFT 1
#define OBS_RIGHT 2
#define OBS_CENTER 3

static NvMediaVideoSurface *capSurf = NULL;

pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
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



    if(i < argc && argv[i][0] == '-')
    {
        while(i < argc && argv[i][0] == '-')
        {
            if(i > 1 && argv[i][1] == '-')
            {
                MESSAGE_PRINTF("Using basic and custom options together is not supported\n");
                return 0;
            }

            // Get options
            if(!strcmp(argv[i], "-va"))
            {
                if(++i < argc)
                {
                    if(sscanf(argv[i], "%f", &args->vipAspectRatio) != 1 || args->vipAspectRatio <= 0.0f) // TBC
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
            else if(!strcmp(argv[i], "-vmr"))
            {
                if(++i < argc)
                {
                    if(sscanf(argv[i], "%ux%u", &args->vipMixerWidth, &args->vipMixerHeight) != 2)
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
            else if(!strcmp(argv[i], "-vf"))
            {
                args->vipFileDumpEnabled = NVMEDIA_TRUE;
                if(++i < argc)
                    args->vipOutputFileName = argv[i];
                else
                {
                    MESSAGE_PRINTF("Missing VIP output file name\n");
                    return 0;
                }
            }
            else if(!strcmp(argv[i], "-vt"))
            {
                if(++i < argc)
                    if(sscanf(argv[i], "%u", &args->vipCaptureTime) != 1)
                    {
                        MESSAGE_PRINTF("Bad VIP capture duration: %s\n", argv[i]);
                        return 0;
                    }
            }
            else if(!strcmp(argv[i], "-vn"))
            {
                if(++i < argc)
                    if(sscanf(argv[i], "%u", &args->vipCaptureCount) != 1)
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

    if(i < argc)
    {
        MESSAGE_PRINTF("%s is not a supported option\n", argv[i]);
        return 0;
    }

    // Check for consistency
    if(i < 2)
    {
        DisplayUsage();
        return 0;
    }


    if(args->vipAspectRatio == 0.0f)
        args->vipAspectRatio = 1.78f;

    if(!args->vipDisplayEnabled && !args->vipFileDumpEnabled)
        args->vipDisplayEnabled = NVMEDIA_TRUE;


    if(!args->vipCaptureTime && !args->vipCaptureCount)
        args->vipCaptureCount = 300;
    else if(args->vipCaptureTime && args->vipCaptureCount)
        args->vipCaptureTime = 0;



    return 1;
}

static int DumpFrame(FILE *fout, NvMediaVideoSurface *surf)
{
    NvMediaVideoSurfaceMap surfMap;
    unsigned int width, height;

    if(NvMediaVideoSurfaceLock(surf, &surfMap) != NVMEDIA_STATUS_OK)
    {
        MESSAGE_PRINTF("NvMediaVideoSurfaceLock() failed in DumpFrame()\n");
        return 0;
    }

    width = surf->width;
    height = surf->height;

    unsigned char *pY[2] = {surfMap.pY, surfMap.pY2};
    unsigned char *pU[2] = {surfMap.pU, surfMap.pU2};
    unsigned char *pV[2] = {surfMap.pV, surfMap.pV2};
    unsigned int pitchY[2] = {surfMap.pitchY, surfMap.pitchY2};
    unsigned int pitchU[2] = {surfMap.pitchU, surfMap.pitchU2};
    unsigned int pitchV[2] = {surfMap.pitchV, surfMap.pitchV2};
    unsigned int i, j;

    for(i = 0; i < 2; i++)
    {
        for(j = 0; j < height / 2; j++)
        {
            fwrite(pY[i], width, 1, fout);
            pY[i] += pitchY[i];
        }
        for(j = 0; j < height / 2; j++)
        {
            fwrite(pU[i], width / 2, 1, fout);
            pU[i] += pitchU[i];
        }
        for(j = 0; j < height / 2; j++)
        {
           fwrite(pV[i], width / 2, 1, fout);
           pV[i] += pitchV[i];
        }
    }


    NvMediaVideoSurfaceUnlock(surf);

    return 1;
}

static int Frame2Ipl(IplImage* img, IplImage* imgResult)
{
    NvMediaVideoSurfaceMap surfMap;
    unsigned int resWidth, resHeight;
    unsigned char y,u,v;
    int num;

    if(NvMediaVideoSurfaceLock(capSurf, &surfMap) != NVMEDIA_STATUS_OK)
    {
        MESSAGE_PRINTF("NvMediaVideoSurfaceLock() failed in Frame2Ipl()\n");
        return 0;
    }

    unsigned char *pY[2] = {surfMap.pY, surfMap.pY2};
    unsigned char *pU[2] = {surfMap.pU, surfMap.pU2};
    unsigned char *pV[2] = {surfMap.pV, surfMap.pV2};
    unsigned int pitchY[2] = {surfMap.pitchY, surfMap.pitchY2};
    unsigned int pitchU[2] = {surfMap.pitchU, surfMap.pitchU2};
    unsigned int pitchV[2] = {surfMap.pitchV, surfMap.pitchV2};
    unsigned int i, j, k, x;
    unsigned int stepY, stepU, stepV;

    unsigned int bin_num=0;

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
    img->imageSize = resHeight*resWidth*3;
    img->widthStep = resWidth*3;
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

     for(j = 0; j < resHeight; j++)
    {
        for(k = 0; k < resWidth; k++)
        {
            x = ResTableX_720To320[k];
            y = pY[i][stepY+x];
            u = pU[i][stepU+x/2];
            v = pV[i][stepV+x/2];

            // - 39 , 111 , 51, 241
            num = 3*k+3*resWidth*(j);
            bin_num = j*imgResult->widthStep + k;
            if( u>-39  &&  u<120  &&  v>45   &&   v<245  ) {
                // 흰색으로
                imgResult->imageData[bin_num] = (char)255;
            }
            else {
                // 검정색으로
                imgResult->imageData[bin_num] = (char)0;
            }

            img->imageData[num] = y;
            img->imageData[num+1] = u;
            img->imageData[num+2] = v;

        }
        stepY += pitchY[i];
        stepU += pitchU[i];
        stepV += pitchV[i];
    }


    NvMediaVideoSurfaceUnlock(capSurf);

    return 1;
}



//img를 받아 -> imgResult(노란차선) , imgColor(flag따라 target color) 추출
static int Frame2Ipl_color(IplImage* img, IplImage* imgResult, IplImage* imgColor, int color)
{
    //color : 1. 살색 2. 흰색차선 3. 검은색신호등 4. 노란색신호등 5. 초록색신호등 6. 빨간색긴급정지  7.노랑&흰색 mix차선
    NvMediaVideoSurfaceMap surfMap;
    unsigned int resWidth, resHeight;
    unsigned char y,u,v;
    int num;

    if(NvMediaVideoSurfaceLock(capSurf, &surfMap) != NVMEDIA_STATUS_OK)
    {
        MESSAGE_PRINTF("NvMediaVideoSurfaceLock() failed in Frame2Ipl()\n");
        return 0;
    }

    unsigned char *pY[2] = {surfMap.pY, surfMap.pY2};
    unsigned char *pU[2] = {surfMap.pU, surfMap.pU2};
    unsigned char *pV[2] = {surfMap.pV, surfMap.pV2};
    unsigned int pitchY[2] = {surfMap.pitchY, surfMap.pitchY2};
    unsigned int pitchU[2] = {surfMap.pitchU, surfMap.pitchU2};
    unsigned int pitchV[2] = {surfMap.pitchV, surfMap.pitchV2};
    unsigned int i, j, k, x;
    unsigned int stepY, stepU, stepV;

    unsigned int bin_num=0;

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
    img->imageSize = resHeight*resWidth*3;
    img->widthStep = resWidth*3;
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


    //노란차선 검출
    int y_max = 255, u_max = 115, v_max = 255;
    int y_min = 180, u_min = 0, v_min = 0;
    //검출할 색상 경계값
    int cy_max = 255, cu_max = 255, cv_max = 255;
    int cy_min = 0, cu_min = 0, cv_min = 0;

    //color : 1. 살색 2. 흰색차선 3. 검은색신호등 4. 노란색신호등 5. 초록색신호등 6. 빨간색긴급정지  7.노랑&흰색 mix차선
    switch (color) {
        case 1:     //살색
            cv_min = 45; cv_max = 127;
            break;
        case 2:     //흰색 차선
            cy_min = 200;
            cu_min = 130;
            break;
        case 3:     //검은색 신호등
            cy_min = 35; cy_max = 50;
            cu_min = 125;
            break;
        case 4:     //노란색 신호등
            cy_min = 90; cy_max = 105;
            cv_min = 146;
            break;
        case 5:     //초록색 신호등
            cy_max = 100;
            cu_max = 127;
            cv_max = 123;
            break;
        case 6:     //빨간색 긴급정지
            cv_min = 140;
            break;
        case 7:     //노랑&흰색 mix 차선
            cy_min = 171;
            break;
        case 8:
            break;

        default:    //노란차선
            y_max = 255; u_max = 115; v_max = 255;
            y_min = 180; u_min = 0; v_min = 0;
            break;
    }

    for(j = 0; j < resHeight; j++)

    {
        for(k = 0; k < resWidth; k++)
        {
            x = ResTableX_720To320[k];
            y = pY[i][stepY+x];
            u = pU[i][stepU+x/2];
            v = pV[i][stepV+x/2];

            // - 39 , 111 , 51, 241
            num = 3*k+3*resWidth*(j);
            bin_num = j*imgResult->widthStep + k;


            // 노란차선 : Threshold에 따른 Binary
            if( y_min<y && y<y_max && u_min<u && u<u_max && v_min<v && v<v_max ) {
                // 흰색으로
                imgResult->imageData[bin_num] = (char)255;
            }
            else {
                // 검정색으로
                imgResult->imageData[bin_num] = (char)0;
            }

            // Target Color : Threshold에 따른 Binary
            if( cy_min<y && y<cy_max && cu_min<u && u<cu_max && cv_min<v && v<cv_max ) {
                // 흰색으로
                imgColor->imageData[bin_num] = (char)255;
            }
            else {
                // 검정색으로
                imgColor->imageData[bin_num] = (char)0;
            }


            img->imageData[num] = y;
            img->imageData[num+1] = u;
            img->imageData[num+2] = v;

        }
        stepY += pitchY[i];
        stepU += pitchU[i];
        stepV += pitchV[i];
    }


    NvMediaVideoSurfaceUnlock(capSurf);

    return 1;
}

void emergencyStopRed(IplImage* imgColor){ // Find_Center보다 뒤에서 사용해야 급정거 효과 있음
    int x = 20, y= 30;
    int width = 280, height = 80;
    int mThreshold = width*height*0.4;
    //int isStop = 0;
    //IplImage* imgEmergency;
    //imgEmergency = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);
    //cvZero(imgEmergency);
    //cvZero(imgEmergency);

    //적색 px판단
    int i, j;
    int count = 0;
    for(j=y; j<y+height; j++){
        for(i=x; i<x+width; i++){
            int px = imgColor->imageData[i + j*imgColor->widthStep];
            if(px == whitepx){
                //TODO
                count++;
            }
        }
    }
    printf("threshold : %d\n", mThreshold);
    if(count > mThreshold){
        //급정지! 대기
        //speed = 0;
        printf("\nStop!!! countpx : %d / %d \n\n",count, mThreshold);
        //DesireSpeed_Write(0);   //정지
        speed = 0;

        //isStop = 1;
    }
    // else if(count < width*height*0.05 && isStop == 1){
    //     printf("\n\n GOGOGOGOGOGOGOGO! \n\n");
    //     //출발
    //     //아예 이 함수 플래그 죽이기
    // }

}

int ThreeWayOBS(int location){

    printf("\n\n 0. light and beep control\n");
    Alarm_Write(ON);
    usleep(100000);
    Alarm_Write(OFF);
    usleep(100000);
    
    if(location == OBS_LEFT){
        printf("OBSTACLE is on left\n");
        return OBS_LEFT;
    }
    else if(location == OBS_RIGHT){

        printf("OBSTACLE is on right\n");
        return OBS_RIGHT;
    }
    else if(location == OBS_CENTER){

        printf("OBSTACLE is on CENTER\n");
       return OBS_CENTER;
    }
    else printf("LOCATION ERROR!!!");

} 

void ThreewaySteering(int location){
    
    // if(location == OBS_LEFT){
    //     return OBS_LEFT;
    // }
    // else if(location == OBS_RIGHT){
    //     return OBS_RIGHT;
    // }
    // else if(location == OBS_CENTER){
//    else printf("LOCATION ERROR!!!")
  // unsigned char status;
  //   short speed;
  //   unsigned char gain;
  //   int position, position_now;
  //   short angle;
  //   int channel;
  //   int data;
  //   char sensor;
  //   int i, j;
  //   int tol;
  //   char byte = 0x80;

if(location == OBS_CENTER){
    printf("\n\n 1. position control on CENTER \n");

    //jobs to be done beforehand;
    SpeedControlOnOff_Write(CONTROL);   // speed controller must be also ON !!!
    speed = 50; // speed set     --> speed must be set when using position controller
    DesireSpeed_Write(speed);

    //control on/off
    status = PositionControlOnOff_Read();
    printf("PositionControlOnOff_Read() = %d\n", status);
    PositionControlOnOff_Write(CONTROL);

    //position controller gain set
    gain = PositionProportionPoint_Read();    // default value = 10, range : 1~50
    printf("PositionProportionPoint_Read() = %d\n", gain);
    gain = 20;
    PositionProportionPoint_Write(gain);
            
    //position write
    position_now = 0;  //initialize
    EncoderCounter_Write(position_now);
    
    //position set
    position=DesireEncoderCount_Read();
    printf("DesireEncoderCount_Read() = %d\n", position);
    position = 300;
    DesireEncoderCount_Write(position);

    position=DesireEncoderCount_Read();
    printf("DesireEncoderCount_Read() = %d\n", position);
    
    tol = 100;    // tolerance
    while(abs(position_now-position)>tol)
    {
        position_now=EncoderCounter_Read();
        printf("EncoderCounter_Read() = %d\n", position_now);
    }
    sleep(1); 
    }

}
    


static unsigned int CaptureThread(void *params)
{
    int i = 0;
    NvU64 stime, ctime;
    NvMediaTime t1 = {0}, t2 = {0}, st = {0}, ct = {0};
    CaptureContext *ctx = (CaptureContext *)params;
    NvMediaVideoSurface *releaseList[4] = {NULL}, **relList;
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

    if(ctx->timeNotCount)
    {
        GetTime(&t1);
        AddTime(&t1, ctx->last * 1000000LL, &t1);
        GetTime(&t2);
        printf("timeNotCount\n");
    }
    GetTime(&st);
    stime = (NvU64)st.tv_sec * 1000000000LL + (NvU64)st.tv_nsec;

    while((ctx->timeNotCount? (SubTime(&t1, &t2)): ((unsigned int)i < ctx->last)) && !stop)
    {
        GetTime(&ct);
        ctime = (NvU64)ct.tv_sec * 1000000000LL + (NvU64)ct.tv_nsec;
        //printf("frame=%3d, time=%llu.%09llu[s] \n", i, (ctime-stime)/1000000000LL, (ctime-stime)%1000000000LL);

        pthread_mutex_lock(&mutex);            // for ControlThread() 

        if(!(capSurf = NvMediaVideoCaptureGetFrame(ctx->capture, ctx->timeout)))
        { // TBD
            MESSAGE_PRINTF("NvMediaVideoCaptureGetFrame() failed in %sThread\n", ctx->name);
            stop = NVMEDIA_TRUE;
            break;
        }

        if(i%3 == 0)                        // once in three loop = 10 Hz --> frequency
            pthread_cond_signal(&cond);        // ControlThread() is called

        pthread_mutex_unlock(&mutex);        // for ControlThread()

        primaryVideo.current = capSurf;
        primaryVideo.pictureStructure = NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD;

        if(NVMEDIA_STATUS_OK != NvMediaVideoMixerRender(ctx->mixer, // mixer
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
        if(NVMEDIA_STATUS_OK != NvMediaVideoMixerRender(ctx->mixer, // mixer
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

        if(ctx->fileDumpEnabled)
        {
            if(!DumpFrame(ctx->fout, capSurf))
            { // TBD
                MESSAGE_PRINTF("DumpFrame() failed in %sThread\n", ctx->name);
                stop = NVMEDIA_TRUE;
            }

            if(!ctx->displayEnabled)
                releaseList[0] = capSurf;
        }

        relList = &releaseList[0];

        while(*relList)
        {
            if(NvMediaVideoCaptureReturnFrame(ctx->capture, *relList) != NVMEDIA_STATUS_OK)
            { // TBD
                MESSAGE_PRINTF("NvMediaVideoCaptureReturnFrame() failed in %sThread\n", ctx->name);
                stop = NVMEDIA_TRUE;
                break;
            }
            relList++;
        }

        if(ctx->timeNotCount)
            GetTime(&t2);

        i++;
    } // while end

    // Release any left-over frames
//    if(ctx->displayEnabled && capSurf && capSurf->type != NvMediaSurfaceType_YV16x2) // To allow returning frames after breaking out of the while loop in case of error
    if(ctx->displayEnabled && capSurf)
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

        while(*relList)
        {
            if(NvMediaVideoCaptureReturnFrame(ctx->capture, *relList) != NVMEDIA_STATUS_OK)
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
    if(NvMediaVideoOutputDevicesQuery(&outputDevices, NULL) != NVMEDIA_STATUS_OK) {
        return;
    }

    // Allocate memory for information for all devices
    outputParams = malloc(outputDevices * sizeof(NvMediaVideoOutputDeviceParams));
    if(!outputParams) {
        return;
    }

    // Get device information for acll devices
    if(NvMediaVideoOutputDevicesQuery(&outputDevices, outputParams) != NVMEDIA_STATUS_OK) {
        free(outputParams);
        return;
    }

    // Find desired device
    for(i = 0; i < outputDevices; i++) {
        if((outputParams + i)->outputDevice == deviceType) {
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
void writeonImage(IplImage* imgResult, char* str_info){
  char* str = str_info;

  //font
  CvFont font;
  cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 0.9, 0.9, 0, 1, CV_AA);
  //cvInitFont(&font, 폰트이름, 1.0, 1.0, 0, 1, CV_AA);

  //textPoint
  CvPoint myPoint = cvPoint(10,235);

  cvPutText(imgResult, str, myPoint, &font, cvScalar(0,0,255,0));
  //cvPutText(Mat&, string& ,textPoint, &font, cvScalar(255,255,255,0));

}

CvPoint getEndPoint(int angle){
  CvPoint point;
  //double x=-1, y=-1;
  int len = 208;
  double seta = 90 + (angle-1500)/10;

  point.x = (int)(RESIZE_WIDTH/2 + len*cos(seta * CV_PI/180.0));
  point.y = (int)(RESIZE_HEIGHT - len*sin(seta * CV_PI/180.0));

return point;
}

void drawonImage(IplImage* imgResult, int angle){
  CvPoint point1, point2;
  point1.x = RESIZE_WIDTH/2;
  point1.y = RESIZE_HEIGHT-20;
  point2 = getEndPoint(angle);

  cvLine(imgResult, point1, point2, CV_RGB(255,255,0), 2, 8, 0);
}
#endif


////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////  Find_Center : TY 영상처리하여 조향값 찾아내는 알고리즘.
/////////////////////////////////////  << 추후 조향값만 반환하고, 실제조향하는 함수를 따로 분리해주어야함.
/////////////////////////////////////  빈공간에 원형만 선언해둠.
////////////////////////////////////////////////////////////////////////////////////////////
void Find_Center(IplImage* imgResult)
{
    // speed와 angle은 내부 변수에서 제거하고
    // 조향과 속도조절 부분은 전역변수 angle과 speed의 값만 바꾸도록 한다.

    //int angle=1500;
    //SteeringServoControl_Write(angle);
}


void *ControlThread(void *unused)
{
    int i=0;

    char fileName[40];
    char fileName1[40];         // TY add 6.27
    char fileName_color[40];         // NYC add 8.25

    //char fileName2[30];           // TY add 6.27
    NvMediaTime pt1 ={0}, pt2 = {0};
    NvU64 ptime1, ptime2;
    struct timespec;

    IplImage* imgOrigin;
    IplImage* imgResult;            // TY add 6.27
    IplImage* imgColor;             // NYC add 8.25
    //IplImage* imgCenter;          // TY add 6.27

    // cvCreateImage
    imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
    imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
    imgColor = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);            // NYC add 6.27
    //imgCenter = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);         // TY add 6.27

    cvZero(imgResult);          // TY add 6.27
    cvZero(imgColor);   //TODO 이거 꼭 필요한가요?
    //cvZero(imgCenter);            // TY add 6.27

    while(1)
    {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);

        GetTime(&pt1);
        ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

        //Frame2Ipl(imgOrigin, imgResult); // save image to IplImage structure & resize image from 720x480 to 320x240
                                         // TY modified 6.27  Frame2Ipl(imgOrigin) -> Frame2Ipl(imgOrigin, imgResult)
        Frame2Ipl_color(imgOrigin, imgResult, imgColor, colorFlag);

        pthread_mutex_unlock(&mutex);

        // TODO : control steering angle based on captured image ---------------

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////TY.만약 IMGSAVE(26번째줄)가 정의되어있으면 imgOrigin.png , imgResult.png 파일을 captureImage폴더로 저장.
//

        Find_Center(imgResult); // TY Centerline 검출해서 조향해주는 알고리즘
        /////////////////////////////////////  << 추후 조향값만 반환하고, 실제조향하는 함수를 따로 분리해주어야함.

        //===================================
        //  장애물 처리 모듈 input : imgColor
       // emergencyStopRed(imgColor);    //NYC //TODO flag to kill
        ThreeWayOBS(OBS_CENTER);
        ThreewaySteering(OBS_CENTER);
        //===================================

        // 조향과 속도처리는 한 프레임당 마지막에 한번에 처리
        SteeringServoControl_Write(angle);
        DesireSpeed_Write(speed);


        #ifdef IMGSAVE
        sprintf(fileName, "captureImage/imgOrigin%d.png", i);
        sprintf(fileName1, "captureImage/imgResult%d.png", i);          // TY add 6.27
        sprintf(fileName_color, "captureImage/imgColor%d.png", i);          // NYC add 8.25
        //sprintf(fileName2, "captureImage/imgCenter%d.png", i);            // TY add 6.27


        cvSaveImage(fileName, imgOrigin, 0);            // should be removed when racing
        cvSaveImage(fileName1, imgResult, 0);           // TY add 6.27
        cvSaveImage(fileName_color, imgColor, 0);       // NYC add 8.25
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
        printf("--------------------------------operation time=%llu.%09llu[s]\n", (ptime2-ptime1)/1000000000LL, (ptime2-ptime1)%1000000000LL);


        i++;
    }
}

int main(int argc, char *argv[])
{
    int err = -1;
    TestArgs testArgs;

    //////////////////////////////// TY add 6.27
    unsigned char status;         // To using CAR Control
    short speed;
    unsigned char gain;
    int position, position_now;
    short angle;
    int channel;
    int data;
    char sensor;
    int i, j;
    int tol;
    char byte = 0x80;
    ////////////////////////////////

    CaptureInputHandle handle;

    NvMediaVideoCapture *vipCapture = NULL;
    NvMediaDevice *device = NULL;
    NvMediaVideoMixer *vipMixer = NULL;
    NvMediaVideoOutput *vipOutput[2] = {NULL, NULL};
    NvMediaVideoOutput *nullOutputList[1] = {NULL};
    FILE *vipFile = NULL;

    NvSemaphore *vipStartSem = NULL, *vipDoneSem = NULL;
    NvThread *vipThread = NULL;

    CaptureContext vipCtx;
    NvMediaBool deviceEnabled = NVMEDIA_FALSE;
    unsigned int displayId;

    pthread_t cntThread;

    signal(SIGINT, SignalHandler);

    memset(&testArgs, 0, sizeof(TestArgs));
    if(!ParseOptions(argc, argv, &testArgs))
        return -1;



// 3. servo control ----------------------------------------------------------
#ifdef SERVO_CONTROL                                    //TY add 6.27

    printf("0. Car initializing \n");
    CarControlInit();


    printf("\n\n 0.1 servo control\n");

    //camera x servo set
    angle = 1500;                       // Range : 600(Right)~1200(default)~1800(Left)
    CameraXServoControl_Write(angle);
    //camera y servo set

    angle = 1800;                       // Range : 1200(Up)~1500(default)~1800(Down)
    CameraYServoControl_Write(angle);

    /*                              //Servo restoration(disable)
    angle = 1500;
    SteeringServoControl_Write(angle);
    CameraXServoControl_Write(angle);
    CameraYServoControl_Write(angle);
    */
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

            if(testutil_capture_input_open(testArgs.i2cDevice, testArgs.vipDeviceInUse, NVMEDIA_TRUE, &handle) < 0)
            {
                MESSAGE_PRINTF("Failed to open VIP device\n");
                goto fail;
            }

            if(testutil_capture_input_configure(handle, &params) < 0)
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


    if(!(vipCapture = NvMediaVideoCaptureCreate(testArgs.vipInputtVideoStd, // interfaceFormat
                                                NULL, // settings
                                                VIP_BUFFER_SIZE)))// numBuffers
    {
        MESSAGE_PRINTF("NvMediaVideoCaptureCreate() failed for vipCapture\n");
        goto fail;
    }


    printf("2. Create NvMedia device \n");
    // Create NvMedia device
    if(!(device = NvMediaDeviceCreate()))
    {
        MESSAGE_PRINTF("NvMediaDeviceCreate() failed\n");
        goto fail;
    }

    printf("3. Create NvMedia mixer(s) and output(s) and bind them \n");
    // Create NvMedia mixer(s) and output(s) and bind them
    unsigned int features = 0;


    features |= NVMEDIA_VIDEO_MIXER_FEATURE_VIDEO_SURFACE_TYPE_YV16X2;
    features |= NVMEDIA_VIDEO_MIXER_FEATURE_PRIMARY_VIDEO_DEINTERLACING; // Bob the 16x2 format by default
    if(testArgs.vipOutputType != NvMediaVideoOutputType_OverlayYUV)
        features |= NVMEDIA_VIDEO_MIXER_FEATURE_DVD_MIXING_MODE;

    if(!(vipMixer = NvMediaVideoMixerCreate(device, // device
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
                                            features , // features
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

    if((vipOutput[0] = NvMediaVideoOutputCreate(testArgs.vipOutputType, // outputType
                                                testArgs.vipOutputDevice[0], // outputDevice
                                                NULL, // outputPreference
                                                deviceEnabled, // alreadyCreated
                                                displayId, // displayId
                                                NULL))) // displayHandle
    {
        if(NvMediaVideoMixerBindOutput(vipMixer, vipOutput[0], NVMEDIA_OUTPUT_DEVICE_0) != NVMEDIA_STATUS_OK)
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
    if(testArgs.vipFileDumpEnabled)
    {
        vipFile = fopen(testArgs.vipOutputFileName, "w");
        if(!vipFile || ferror(vipFile))
        {
            MESSAGE_PRINTF("Error opening output file for VIP\n");
            goto fail;
        }
    }

    printf("6. Create vip pool(s), queue(s), fetch threads and stream start/done semaphores \n");
    // Create vip pool(s), queue(s), fetch threads and stream start/done semaphores
    if(NvSemaphoreCreate(&vipStartSem, 0, 1) != RESULT_OK)
    {
        MESSAGE_PRINTF("NvSemaphoreCreate() failed for vipStartSem\n");
        goto fail;
    }

    if(NvSemaphoreCreate(&vipDoneSem, 0, 1) != RESULT_OK)
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

    if(testArgs.vipCaptureTime)
    {
        vipCtx.timeNotCount = NVMEDIA_TRUE;
        vipCtx.last = testArgs.vipCaptureTime;
    }
    else
    {
        vipCtx.timeNotCount = NVMEDIA_FALSE;
        vipCtx.last = testArgs.vipCaptureCount;
    }


    if(NvThreadCreate(&vipThread, CaptureThread, &vipCtx, NV_THREAD_PRIORITY_NORMAL) != RESULT_OK)// make capture thread / call capture thread
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
    pthread_create(&cntThread, NULL, &ControlThread, NULL); // method to make create thread at linux -- googling **
    //n

    printf("9. Wait for completion \n");
    // Wait for completion
    NvSemaphoreDecrement(vipDoneSem, NV_TIMEOUT_INFINITE);


    err = 0;

fail: // Run down sequence
    // Destroy vip threads and stream start/done semaphores
    if(vipThread)
        NvThreadDestroy(vipThread);
    if(vipDoneSem)
        NvSemaphoreDestroy(vipDoneSem);
    if(vipStartSem)
        NvSemaphoreDestroy(vipStartSem);

    printf("10. Close output file(s) \n");
    // Close output file(s)
    if(vipFile)
        fclose(vipFile);

    // Unbind NvMedia mixer(s) and output(s) and destroy them
    if(vipOutput[0])
    {
        NvMediaVideoMixerUnbindOutput(vipMixer, vipOutput[0], NULL);
        NvMediaVideoOutputDestroy(vipOutput[0]);
    }
    if(vipOutput[1])
    {
        NvMediaVideoMixerUnbindOutput(vipMixer, vipOutput[1], NULL);
        NvMediaVideoOutputDestroy(vipOutput[1]);
    }
    if(vipMixer)
        NvMediaVideoMixerDestroy(vipMixer);


    // Destroy NvMedia device
    if(device)
        NvMediaDeviceDestroy(device);

    // Destroy NvMedia capture(s)
    if(vipCapture)
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
