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

bool Right_Max = false;
bool Left_Max = false;

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
	int width = 320;//40
	int height = 240;//60
	int Left_Down = 0; // 3 사분면
	int Right_Down = 0; // 4 사분면
	int Left_Up = 0; // 2 사분면
	int Right_Up = 0; // 1 사분면

	int x = 0;
	int y = 0;
	int angle = 0;

	int Dif = 0, Dif1 = 0; // 2사분면 - 1사분면 픽셀수 ; 3사분면 - 4사분면 픽셀수;
	int Left_Sum = 0, Right_Sum = 0; //왼쪽, 오른쪽 픽셀 갯수
	int Gap = 0; // 왼쪽 - 오른쪽 픽셀 갯수;
	int Up = 0, Down = 0;// 1,2 dimension's sum & 3,4 dimension's sum

	float weight = 1.8, weight2 = 1.2;// control angle weight
	int speed = 0;
	int straight_speed = 115;
	int curve_speed = 85;
	//총 픽셀은 320 *240 = 76800


	for (y = 100; y < height / 2; y++)
	{
		for (x = 80; x < width / 2; x++)
		{
			if (imgResult->imageData[y * width + x] == whitepx)//Find white pixels
			{
				Left_Up++;

			}// 2사분면
		}
		for (x = 240; x > width / 2; x--)
		{
			if (imgResult->imageData[y * width + x] == whitepx)
			{
				Right_Up++;
			}// 1사분면
		}
	}
	printf("Left_Up = %d\n", Left_Up);
	printf("Right_Up = %d\n", Right_Up);
	Dif1 = Left_Up - Right_Up;
	printf("Difference1 is %d\n", Dif1);


	for (y = (height / 2) + 1; y < 160; y++)
	{
		for (x = 80; x < width / 2; x++)
		{
			if (imgResult->imageData[y * width + x] == whitepx)//Find white pixels
			{
				Left_Down++;

			}// 3사분면
		}
		for (x = 240; x > width / 2; x--)
		{
			if (imgResult->imageData[y * width + x] == whitepx)
			{
				Right_Down++;
			}// 4사분면
		}

	}
	printf("Left_Down = %d\n", Left_Down);
	printf("Right_Down = %d\n", Right_Down);

	Dif = Left_Down - Right_Down;
	printf("Difference is %d\n", Dif);


	Left_Sum = Left_Down + Left_Up;
	Right_Sum = Right_Down + Right_Up;
	Gap = Left_Sum - Right_Sum;
	Up = Left_Up + Right_Up;
	Down = Left_Down + Right_Down;

	printf("left_sum=%d, Right_sum=%d, gap=%d\n", Left_Sum, Right_Sum, Gap);



	if (Dif == 0 && Dif1 == 0)
	{
		angle = 1500;
	}

	else if (Gap > 0) // turn right
	{
		if (Dif <= 10 && Dif1 <= 10)
		{
			angle = 1500;
		}

		else if (Right_Up < Left_Up)
		{
			if (Down == 0)
			{
				if (Right_Sum == 0)
				{
					angle = 1250;
				}//only one side pixels
				else
				{
					angle = 1000;
					Right_Max = true;
				}
			}

			else
				angle = 1500 - Gap * weight;
		}

	}// angle < 1500 turn right

	else if (Gap < 0) // turn left
	{
		if (Dif >= -10 && Dif1 >= -10)
		{
			angle = 1500;
		}

		else if (Right_Up > Left_Up)
		{
			if (Down == 0)
			{
				if (Left_Sum == 0)
				{
					angle = 1750;
				}
				else
				{
					angle = 2000;
					Left_Max = true;
				}
			}

			else
				angle = 1500 - Gap * weight;
		}

	}// angle > 1500 turn left

	if (Left_Max || Right_Max == true)
	{
    Left_Up = Left_Sum;
		Right_Up = Right_Sum;
    
		if (Dif <= 10 && Dif1 <= 10)
		{
			Right_Max = false;
		}
		else if (Dif >= -10 && Dif1 >= -10)
		{
			Left_Max = false;
		}
		else if (Right_Max == true)
		{
			angle = 1000;
		}
		else if (Left_Max == true)
		{
			angle = 2000;
		}
	}

	angle = angle > 2000 ? 2000 : angle < 1000 ? 1000 : angle;

	/*angle = angle > 2000 ? 2000: angle;
	angle = angle < 1000 ? 1000 : angle;*/

	SteeringServoControl_Write(angle);

#ifdef SPEED_CONTROL
	if (angle<1200 || angle>1800)
		speed = curve_speed;
	else
		speed = straight_speed;
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
  printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
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
  CvPoint myPoint = cvPoint(10,235);

  cvPutText(imgResult, str, myPoint, &font, cvScalar(255,255,255,0));
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
  point1.y = img_height-20;
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



int main(int argc, char const *argv[]) {

  int i = 0, index = 0; // index of image
  char file_name[40];
  char str_info[50];
  unsigned char asd;
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
    return;
  }
  if(imgResult==0){ //null check
    printf("No Testset ImageResult! Index : %d\n",index);
    return;
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

  int mode = selectMode();
  if(mode!=1 && mode!=2) return;

  while(1){
    sprintf(file_name, "../captureImage/imgResult%d.png", index);
    //img = cvLoadImage(file_name, CV_LOAD_IMAGE_GRAYSCALE);
    img = cvLoadImage(file_name, -1);
    imgResult = cvLoadImage(file_name, 1);
    if(img==0){ //null check
      printf("No Testset Image! Index : %d\n",index);
      return;
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
      } else if(key==32){ //space bar
        cvWaitKey(0);
      }
      index++;
    }

    else if(mode == 2){
      int key = cvWaitKey(0);
      if(key=='q'){
        printf("Closing the Simulator...\n");
        break;
      } else if(key==65361){  //left key
        index--;
      } else if(key==65363){  //right key
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
