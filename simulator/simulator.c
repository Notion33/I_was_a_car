#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <cv.h>
#include <highgui.h>
#include <math.h>
//Your Driving Algorithm!
//#include <find_Center.h>

// 1. 이미지에 글씨 넣기 clear

// 2. 이미지 -> findcenter -> 글씨 넣기
// 3. 각도 선으로 이미지화
// 4. 연속이미지
// 5. 모드선택

int sim_angle;
int sim_speed;
int img_height;
int img_width;

//==============================================================================
//  Please put your own Find_Center code!
//  If you have compile Error, feel free to add any parameters you need.
//==============================================================================

//==============================================================================
//  Global variable & header file area
#include "stdbool.h"

#define SERVO_CONTROL
#define SPEED_CONTROL
#define IMGSAVE

#define UNCONTROL 0
#define CONTROL 1
#define whitepx -1

bool turn_left_max = false;         //TY add
bool turn_right_max = false;

//==============================================================================

void SteeringServoControl_Write(int angle){
  sim_angle = angle;
  //printf("Servo call! angle : %d",sim_angle);
}

void DesireSpeed_Write(int speed){
  sim_speed = speed;
  //printf("Speed call! Speed : %d",sim_speed);
}
//==============================================================================
//  Find_Center Algorithm area

void Find_Center(IplImage* imgResult)		//TY add 6.27
{
		float angle = 0;
		int speed = 0;
		int width = 320;//data of the input image
		int height = 240;

		int x = 0;//for cycle
		int y = 0;

		int cutdown = 30;//length of y pixel which display bumper
		int distance = 280; // approximately 10cm = 140px from leftside of the camera
		int weight = 500/distance;

		int counl = 0;//count left pixel of white
		int counr = 0;

		int centerpixel = 0;

		int leftpixel = 0;//linepixel
		int rightpixel = 0;

		int befline = 0;//true or false

		int  finl = 0;//whether line of leftside detected
		int finr = 0;

		int centerofpixel = 0;//centerlinepixel


		int gotomax = 0;


		for (y = height-1-cutdown; y >=0; y-=5){//cut down bumper pixel
			if(finl||finr)break;
			for (x = width / 2 -1; x>=0; x--){//left side
				if (imgResult->imageData[y * width + x] == whitepx){//Search pixels
					if(befline){//if there was white pixels right before
						if(counl==5){// if counl>4 -> regarded as line
						finl = 1;
						leftpixel = x+4;
						}
						else counl++;	//regarded as start point of the line?
					}
					else counl = 1;
					befline = 1;
				}
				else befline = 0;
			}

			befline = 0;

			for (x = width/2; x<width; x++){
				if (imgResult->imageData[y * width + x] == whitepx){//Search pixels
					if(befline){			//if there was white pixels right before
						if(counr==5){// if counl>4 -> regarded as line
						finr = 1;
						rightpixel = x-4;
						}
						else counr++;
					}
					else counr = 1;
					befline = 1;
				}
				else befline = 0;
			}
		}

		befline = 0;

		for(y = 100;y<220;y++)//90->120->110->100 2017.08.17
			if (imgResult->imageData[y * width + 160] == whitepx){
				if(befline ==0)centerofpixel = 1;
				if(centerofpixel>=4){gotomax = 1;break;}
				centerofpixel++;
				befline = 1;
				}


		if(!(finl||finr)){ //?? Ž?? x????
			//Alarm_Write(ON);
			//Alarm_Write(OFF);
			DesireSpeed_Write(0);
    		usleep(1000000);

		}

		printf("Left_line = %d\n", leftpixel);
		printf("Right_line = %d\n", rightpixel);
		printf("Centerofpixle = %d\n",centerofpixel);
		centerpixel = finl&&finr? (rightpixel+leftpixel)/2:(finl==0?rightpixel-distance:leftpixel+distance);

		if(gotomax){
			if(centerpixel>160)angle = 1000;	//turn right maximize
			else angle = 2000;//turn left maximize
				}
		else if(centerpixel-160<10&&centerpixel-160>-10)angle = 1500;
		else
		angle = 1500 - weight*(centerpixel-160);

		SteeringServoControl_Write(angle);//motor control

		#ifdef SPEED_CONTROL
        if(angle<1200||angle>1800)      //직선코스의 속도와 곡선코스의 속도 다르게 적용
           speed = 80;//max==90
        else
           speed = 100;//max==130


	DesireSpeed_Write(speed);
    #endif


}

//==============================================================================
//==============================================================================



//==============================================================================
//  Main simulator code!
//
//==============================================================================

int selectMode(){
  int mode = 0;
  printf("\n\nWelcome to Car Simulator! Please choose the mode\n");
  printf("1. AutoDrive   2. ManualDrive\n");
  printf("Mode : ");
  //scanf("%1d\n", &mode);
  scanf("%d", &mode);
  //mode = getchar();

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
  //cvInitFont(&font, 폰트이름, 1.0, 1.0, 0, 1, CV_AA);

  //textPoint
  CvPoint myPoint = cvPoint(10,15);

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
  //point.x = (int)(img_width/2 + len*cos(seta));
  //point.y = (int)(img_height - len*sin(seta));

return point;
}

void drawonImage(IplImage* imgResult, int angle){
  CvPoint point1, point2;
  point1.x = img_width/2;
  point1.y = img_height;
  //point2.x = getX(angle);
  //point2.y = getY(angle);
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



int startFrame(){
  int i = 0;
  printf("Please insert a starting frame.(Ex : 0 or 123)\n");
  printf("Staring frame : ");
  scanf("%d", &i);
  return i;
}

int main(int argc, char const *argv[]) {

int i = 0, index = 0; // index of image
char file_name[40];
char str_info[50];
unsigned char asd;

int mode = selectMode();
if(mode!=1 && mode!=2) return 1;

index = startFrame();
sprintf(file_name, "../captureImage/imgResult%d.png", index);

  //initializing images
  //IplImage* img = cvLoadImage(file_name, CV_LOAD_IMAGE_GRAYSCALE);
  //CvSize size(320,240);
  //IplImage* imgsize = cvLoadImage(file_name, -1);
  //IplImage* img = cvCreateImage(cvGetSize(imgsize), IPL_DEPTH_8U, 1);
  IplImage* img = cvLoadImage(file_name, -1);
  IplImage* imgResult = cvLoadImage(file_name, 1);
  if(img==0){ //null check
    printf("No Testset Image! Index : %d\n",index);
    return 1;
  }
  if(imgResult==0){ //null check
    printf("No Testset ImageResult! Index : %d\n",index);
    return 1;
  }
  img_width = cvGetSize(img).width;
  img_height = cvGetSize(img).height;
  //printf("width : %d",img_width);
  //printf("height : %d",img_height);
  //IplImage* imgResult = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
  cvZero(imgResult);

  //show the image
  cvNamedWindow("simulator",CV_WINDOW_AUTOSIZE);
  cvShowImage("simulator",imgResult);

  while(1){
    printf("//============================================================frame : %d\n",index);
    sprintf(file_name, "../captureImage/imgResult%d.png", index);
    //img = cvLoadImage(file_name, CV_LOAD_IMAGE_GRAYSCALE);
    img = cvLoadImage(file_name, -1);
    imgResult = cvLoadImage(file_name, 1);
    if(img==0){ //null check
      printf("No Testset Image! Index : %d\n",index);
      return 1;
    }

    // for (i = 0; i < img_width * img_height; i++){
    //   imgResult -> imageData[i] = (char) img -> imageData[i];
    //   if(img -> imageData[i] == 255) printf("0xff");
    // }

    //imgResult = img;

    //imgResult = cvCloneImage(img);
    //imgResult->depth = IPL_DEPTH_8U;
    //printf("typeof : %d\n", );
    //refineImage(imgResult);
    //checkImage(imgResult);
    //TODO 이미지 처리
    Find_Center(img);
    sprintf(str_info, "[Image %d]  Angle : %d, Speed : %d", index, sim_angle, sim_speed);
    writeonImage(imgResult, str_info);
    drawonImage(imgResult, sim_angle);

    cvShowImage("simulator",imgResult);

    if(mode == 1){
      int key = cvWaitKey(200);
      if(key=='q'){
        printf("Closing the Simulator...\n");
        break;
      } else if(key==32){
        cvWaitKey(0);
      }
      index++;
    }

    else if(mode == 2){
      int key = cvWaitKey(0);
      if(key=='q'){
        printf("Closing the Simulator...\n");
        break;
      } else if(key==65361){
        index--;
      } else if(key==65363){
        index++;
      } else {
        printf("\n\n key = %d\n", key);
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
