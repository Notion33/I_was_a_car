#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <cv.h>
#include <highgui.h>
#include <math.h>
#include"stdbool.h"

#define THRESHHOLD img->imageData[(img_width*j+k)*3+1]>39&&img->imageData[(img_width*j+k)*3+1]<120&&img->imageData[(img_width*j+k)*3+2]>45&&img->imageData[(img_width*j+k)*3+2]>245

int sim_angle;
int sim_speed;
int img_height;
int img_width;

//==============================================================================
//  Please put your own Find_Center code!
//  If you have compile Error, feel free to add any parameters you need.
//==============================================================================


void SteeringServoControl_Write(int angle){
  sim_angle = angle;
  //printf("Servo call! angle : %d",sim_angle);
}

void DesireSpeed_Write(int speed){
  sim_speed = speed;
  //printf("Speed call! Speed : %d",sim_speed);
}

int speed = 130;
int angle = 1500;

#include "find_center.h"

//==============================================================================
//  Find_Center Algorithm area




//==============================================================================
//==============================================================================



//==============================================================================
//  Main simulator code!
//
//==============================================================================

int selectMode(){
  int mode = 0;
  printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
  printf("\n\nWelcome to Car Simulator! Please choose the mode\n");
  printf("1. AutoDrive   2. ManualDrive\n");
  printf("Mode : ");
  scanf("%d", &mode);

  switch (mode) {
    case 1 :
      printf("Mode : Auto Drive\n");

      break;
    case 2 :
      printf("Mode : Manual Drive\n");

      break;

    default :
      printf("Error : switch input : %d\n", mode);
      break;
  }

  return mode;

}

void writeonImage(IplImage* imgResult, char* str_info){
  char* str = str_info;

  //font
  CvFont font;
  cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 0.9, 0.9, 0, 1, CV_AA);
  //cvInitFont(&font, ��Ʈ�̸�, 1.0, 1.0, 0, 1, CV_AA);

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

  point.x = (int)(img_width/2 + len*cos(seta * CV_PI/180.0));
  point.y = (int)(img_height - len*sin(seta * CV_PI/180.0));

return point;
}

void drawonImage(IplImage* imgResult, int angle){
  CvPoint point1, point2;
  point1.x = img_width/2;
  point1.y = img_height-20;
  point2 = getEndPoint(angle);

  cvLine(imgResult, point1, point2, CV_RGB(255,255,0), 2, 8, 0);
}

void checkImage(IplImage* img){
  int idx = 0;
  int i=0; int j=0;
  for (i = 0; i < img_width; i++) {
    for (j = 0; j < img_height; j++) {
      idx = i * img_width + j;
      printf("%u", img->imageData[idx]);
    }
    printf("\n");
  }
  printf("Depth : %d\n",img->depth);
  //printf("sizeoftype : %d\n", sizeof(img->imageData[idx]));
  cvWaitKey(0);
}

void refineImage(IplImage* img){
  int idx = 0;
  int i=0; int j=0;
  for (i = 0; i < img_width; i++) {
    for (j = 0; j < img_height; j++) {
      idx = i * img_width + j;
      if(img->imageData[idx]==-1){
        img->imageData[idx]=255;
      }
    }
  }
}

int startFrame(int lastframe){
  int i = 0;
  printf("Please insert a starting frame.(Ex : 0 or 123)  /  %dframe\n",lastframe);
  printf("Staring frame : ");
  scanf("%d", &i);
  return i;
}

int findLastFrame(){
  int index = 0;
  char file_name[40];

  while(1){
    sprintf(file_name, "../captureImage/imgOrigin%d.png", index);
    IplImage* img = cvLoadImage(file_name, -1);
    if(img==0){ //null check
      return index-1;
    }
    index++;
  }
}

void onTrack(int pos){
  //cvWaitKey(100);
}

int main(int argc, char const *argv[]) {
  int j = 0,k = 0;
  int i = 0, index = 0; // index of image
  int previous_idx = -1;
  int automode = 1;
  int lastframe = findLastFrame();
  char file_name[40];
  char str_info[50];
  unsigned char asd;

  printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
  printf("\n\nWelcome to Car Simulator! Please choose the mode\n");
  index = startFrame(lastframe);
  while(index>lastframe){
    printf("Selected frame is out of range. Choose again starting frame\n");
    index = startFrame(lastframe);
  }
  sprintf(file_name, "../captureImage/imgOrigin%d.png", index);

  //initializing images
  IplImage* img = cvLoadImage(file_name, -1);
  if(img==0){ //null check
    printf("No Testset Image! Index : %d\n",index);
    return 1;
  }
  img_width = cvGetSize(img).width;
  img_height = cvGetSize(img).height;
  IplImage* imgResult = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
  cvZero(imgResult);


  //show the image
  cvNamedWindow("simulator",CV_WINDOW_AUTOSIZE);
  cvCreateTrackbar("frame","simulator",&index,lastframe,onTrack);
 int low_y, high_y, low_u, high_u, low_v, high_v;
  cvCreateTrackbar("low_y", "simulator", &low_y, 255, onTrack);
	cvCreateTrackbar("low_u", "simulator", &low_u, 255, onTrack);
	cvCreateTrackbar("low_v", "simulator", &low_v, 255, onTrack);
	cvCreateTrackbar("high_y", "simulator", &high_y, 255, onTrack);
	cvCreateTrackbar("high_u", "simulator", &high_u, 255, onTrack);
	cvCreateTrackbar("high_v", "simulator", &high_v, 255, onTrack);

  cvSetTrackbarPos("low_y","simulator",96);
  cvSetTrackbarPos("low_u","simulator",43);
  cvSetTrackbarPos("low_v","simulator",0);
  cvSetTrackbarPos("high_y","simulator",255);
  cvSetTrackbarPos("high_u","simulator",89);
  cvSetTrackbarPos("high_v","simulator",143);

  cvShowImage("simulator",imgResult);

  while(1){
    sprintf(file_name, "../captureImage/imgOrigin%d.png", index);
    img = cvLoadImage(file_name, -1);
  IplImage* imgResult = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
  cvZero(imgResult);
		low_y = cvGetTrackbarPos("low_y", "simulator");
		low_u = cvGetTrackbarPos("low_u", "simulator");
		low_v = cvGetTrackbarPos("low_v", "simulator");
		high_y = cvGetTrackbarPos("high_y", "simulator");
		high_u = cvGetTrackbarPos("high_u", "simulator");
		high_v = cvGetTrackbarPos("high_v", "simulator");
		cvInRangeS(img, cvScalar(low_y, low_u, low_v, 0), cvScalar(high_y, high_u, high_v, 0), imgResult);

    if(img==0){ //null check
      printf("No Testset Image! Index : %d\n",index);
      return 1;
    }

    //TODO �̹��� ó��
    if(previous_idx != index){  //�ߺ� ������ ����
        printf("//============================================================frame : %d\n",index);
        Find_Center(img);

        SteeringServoControl_Write(angle);
        DesireSpeed_Write(speed);
    }
    sprintf(str_info, "[Image %d]  Angle : %d, Speed : %d", index, sim_angle, sim_speed);
    writeonImage(imgResult, str_info);
    drawonImage(imgResult, sim_angle);

    cvSetTrackbarPos("frame","simulator",index);

    cvShowImage("simulator",imgResult);

    int key = cvWaitKey(200);
    if(key=='q' || key==27){
      printf("\nClosing the Simulator...\n");
      break;
    } else if(key==32){ //space bar
      if(automode==1) automode = 0;
      else if(automode==0) automode = 1;
    }
    previous_idx = index;
    index++;

    if(automode==0){  //if manual mode
      index--;        //���� ������ ����

      if(key==65361 || key==65364){  //left key
        index--;
      } else if(key==65363 || key==65362){  //right key
        index++;
      } else {
        //printf("\n\n Wrong key = %d\n", key);
      }
    }
  }
  //TODO save images

  //close the image
  cvDestroyWindow("simulator");
  cvReleaseImage(&img);
  cvReleaseImage(&imgResult);

  cvWaitKey(0);

  return 0;
}
