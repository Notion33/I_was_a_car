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
#include "stdbool.h"	//TY add

#define SERVO_CONTROL     // TY add 6.27
#define SPEED_CONTROL
//#define IMGSAVE

////////////////////////////////////////////////////////////////////////////


#define VIP_BUFFER_SIZE 6
#define VIP_FRAME_TIMEOUT_MS 100
#define VIP_NAME "vip"

#define MESSAGE_PRINTF printf

#define CRC32_POLYNOMIAL 0xEDB88320L

#define RESIZE_WIDTH  320
#define RESIZE_HEIGHT 240


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
      
        if(i%3 == 0)                        // once in three loop = 10 Hz
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




////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////  Find_Center : TY 영상처리하여 조향값 찾아내는 알고리즘.
/////////////////////////////////////  << 추후 조향값만 반환하고, 실제조향하는 함수를 따로 분리해주어야함.
/////////////////////////////////////  빈공간에 원형만 선언해둠.
////////////////////////////////////////////////////////////////////////////////////////////
void Find_Center(IplImage* imgResult, IplImage* imgCenter)      //TY add 6.27
{
    int i=0;
    int j=0;

    int y_start_line = 100;     //y_start_line과 y_end_line 차는 line_gap의 배수이어야 함.
    int y_end_line = 200;

    int valid_left_amount = 0;
    int valid_right_amount = 0;

    int left_line_start = 0;
    int left_line_end = 0;
    int right_line_start = 0;
    int right_line_end = 0;

    int line_gap = 20;  //line by line 스캔시, lower line과 upper line의 차이는 line_gap px
    int tolerance = 10; // center pixel +- tolerance px 내에서 라인검출시 for문 종료 용도
    int angle=1500;
    float weight = 500; // control angle weight
    float control_angle = 0;

    int left[400] = {0};
    int right[400] = {0};
    float left_slope[200] = {0.0};
    float right_slope[200] = {0.0};
    
    bool turn_left_max = false;
    bool turn_right_max = false;
    
    for(i = y_start_line ; i<y_end_line ; i=i+line_gap){
        for(j=(imgResult->width/2) ; j>=0 ; j--){                            //Searching the left line point
            left[i] = j;
            if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                valid_left_amount++;
                break;
            }
        }
        for(j=(imgResult->width/2) ; j<=imgResult->width ; j++){             //Searching the right line point
            right[i] = j;
            if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                valid_right_amount++;;
                break;
            }
        }
        if(left[i]>((imgResult->width/2)-tolerance)||right[i]<((imgResult->width/2)+tolerance)){                    //검출된 좌측혹은 우측차선이 화면중앙에 있는경우, 차선검출 종료후 반대방향으로 최대조향 flag set
            if(valid_left_amount == 1)
                turn_right_max = true;
            else if(valid_right_amount == 1)
                turn_left_max = true;
            break;
            }
    }

    for(i=0;i<=valid_left_amount;i++){                        //좌측 유효 라인 포인트 범위 계산
        if(left[y_start_line+i*line_gap]!=0){
            left_line_start = y_start_line+i*line_gap;
            left_line_end = y_start_line + (i + valid_left_amount)*line_gap;
            break;
        }
    }
    
    for(i=0;i<=valid_right_amount;i++){                        //우측 유효 라인 포인트 범위 계산
        if(right[y_start_line+i*line_gap]!=imgResult->width){
            right_line_start = y_start_line+i*line_gap;
            right_line_end = y_start_line + (i + valid_right_amount)*line_gap;
            break;
        }
    }

    printf("\nleft line = ");
    for(i=0;i<valid_left_amount;i++)printf("%d  ",left[left_line_start+i*line_gap]);
    printf("\nright line = ");
    for(i=0;i<valid_right_amount;i++)printf("%d ",right[right_line_start+i*line_gap]);
    printf("\n");

    if(valid_left_amount > 1){
        //for(i=left_line_start;i<=valid_left_amount;i++)                         //left,right 유효 라인 포인트만 직선 기울기들 계산
                left_slope[0] = (float)(left[left_line_start] - left[left_line_start+(valid_left_amount-1)*line_gap])/(float)(valid_left_amount*line_gap);
    }
    else left_slope[0] = 0;
    if(valid_right_amount > 1){
        //for(i=right_line_start;i<=valid_right_amount;i++)                         //left,right 유효 라인 포인트만 직선 기울기들 계산
                right_slope[0] = (float)(right[right_line_start] - right[right_line_start+(valid_right_amount-1)*line_gap])/(float)(valid_right_amount*line_gap);
    }
    else right_slope[0] = 0;
    
    control_angle = (left_slope[0] + right_slope[0])*weight;        //positive : turn right , negative : turn left

    printf("%d ,%d, %d, %d \n",left[left_line_start] , left[left_line_start+(valid_left_amount-1)*line_gap],right[right_line_start] , right[right_line_start+(valid_right_amount-1)*line_gap]);
    printf("left_slope : %f ,right_slope : %f   ",left_slope[0],right_slope[0]);
    printf("\nvalid amount // left : %d , right : %d\n",valid_left_amount,valid_right_amount);
    printf("Control_Angle : %f \n",control_angle);

    //steer servo set
    if (turn_left_max == true)
        angle = 2000;
    else if (turn_right_max == true)
        angle = 1000;
    else{
        angle = 1500 - control_angle ;                                  // Range : 1000(Right)~1500(default)~2000(Left)
        angle = angle>2000? 2000 : angle<1000 ? 1000 : angle;           // Bounding the angle range
    }
    
    SteeringServoControl_Write(angle);

}


void *ControlThread(void *unused)
{
    int i=0;
    char fileName[30];
    char fileName1[30];         // TY add 6.27
    //char fileName2[30];           // TY add 6.27
    NvMediaTime pt1 ={0}, pt2 = {0};
    NvU64 ptime1, ptime2;
    struct timespec;
 
    IplImage* imgOrigin;
    IplImage* imgResult;            // TY add 6.27
    //IplImage* imgCenter;          // TY add 6.27
    
    // cvCreateImage
    imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
    imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27
    //imgCenter = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);         // TY add 6.27

    cvZero(imgResult);          // TY add 6.27
    //cvZero(imgCenter);            // TY add 6.27
 
    while(1)
    {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        
        GetTime(&pt1);
        ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

        Frame2Ipl(imgOrigin, imgResult); // save image to IplImage structure & resize image from 720x480 to 320x240
                                         // TY modified 6.27  Frame2Ipl(imgOrigin) -> Frame2Ipl(imgOrigin, imgResult)

        pthread_mutex_unlock(&mutex);     
        
        // TODO : control steering angle based on captured image ---------------

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////TY.만약 IMGSAVE(26번째줄)가 정의되어있으면 imgOrigin.png , imgResult.png 파일을 captureImage폴더로 저장.
//

        Find_Center(imgResult, imgCenter); // TY Centerline 검출해서 조향해주는 알고리즘
        /////////////////////////////////////  << 추후 조향값만 반환하고, 실제조향하는 함수를 따로 분리해주어야함.

        #ifdef IMGSAVE              
        sprintf(fileName, "captureImage/imgOrigin%d.png", i);
        sprintf(fileName1, "captureImage/imgResult%d.png", i);          // TY add 6.27
        //sprintf(fileName2, "captureImage/imgCenter%d.png", i);            // TY add 6.27

        
        cvSaveImage(fileName, imgOrigin, 0);
        cvSaveImage(fileName1, imgResult, 0);           // TY add 6.27
        //cvSaveImage(fileName2, imgCenter, 0);         // TY add 6.27
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


	#ifdef SPEED_CONTROL
	    // 2. speed control ---------------------------------------------------------- TY
	    printf("\n\nspeed control\n");
	    PositionControlOnOff_Write(UNCONTROL); // 엔코더 안쓰고 주행할 경우 UNCONTROL세팅
	    //control on/off
	    SpeedControlOnOff_Write(CONTROL);
	    //speed controller gain set 		   // PID range : 1~50 default : 20
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
	    speed = 60;
	    DesireSpeed_Write(speed);
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


    if(NvThreadCreate(&vipThread, CaptureThread, &vipCtx, NV_THREAD_PRIORITY_NORMAL) != RESULT_OK)
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

    #ifdef SPEED_CONTROL	//TY ADD
    	speed = 0;
		DesireSpeed_Write(speed);
	#endif

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

