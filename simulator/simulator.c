#include <stdio.h>
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

bool turn_left_max = false;         //TY add
bool turn_right_max = false;
//==============================================================================
//  Find_Center Algorithm area

void Find_Center(IplImage* imgResult)      //TY add 6.27
{
    int i=0;
    int j=0;

    int y_start_line = 160;     //y_start_line과 y_end_line 차는 line_gap의 배수이어야 함.
    int y_end_line = 140;

    int valid_left_amount = 0;
    int valid_right_amount = 0;

    int left_line_start = 0;
    int left_line_end = 0;
    int right_line_start = 0;
    int right_line_end = 0;
    int bottom_line = 200;
    int line_width = 70;
    int speed = 0;
    int curve_speed = 70;       //default : 60
    int straight_speed = 100;    //default : 90

    int line_gap = 4;  //line by line 스캔시, lower line과 upper line의 차이는 line_gap px
    int tolerance = 40; // center pixel +- tolerance px 내에서 라인검출시 for문 종료 용도
    int angle=1500;
    float weight = 300; // control angle weight
    float control_angle = 0;

    int left[240] = {0};
    int right[240] = {imgResult->width-1};
    float left_slope[2] = {0.0};
    float right_slope[2] = {0.0};

    bool continue_turn_left = false;
    bool continue_turn_right = false;

    for(i = y_start_line ; i>y_end_line ; i=i-line_gap){
        for(j=(imgResult->width)/2 ; j>0 ; j--){                            //Searching the left line point
                left[y_start_line-i] = j;
                if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                    valid_left_amount++;
                    break;
                }
        }
        for(j=(imgResult->width)/2 ; j<imgResult->width ; j++){             //Searching the right line point
                right[y_start_line-i] = j;
                if(imgResult->imageData[i*imgResult->widthStep + j] == 255){
                    valid_right_amount++;;
                    break;
                }
        }
        if(left[y_start_line-i]>((imgResult->width/2)-tolerance)||right[y_start_line-i]<((imgResult->width/2)+tolerance)){     //검출된 차선이 화면중앙부근에 있는경우, 차선검출 종료후 반대방향으로 최대조향 flag set
            if(valid_left_amount > valid_right_amount && turn_left_max == false){
            	printf("continue_turn_right set!\n");
                continue_turn_right = true;
            }
            else if(valid_right_amount > valid_left_amount && turn_right_max == false){
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
            for(i = imgResult->widthStep -1 ; i > (imgResult->width/2) + line_width ; i--){
            	if(imgResult->imageData[y_start_line*imgResult->widthStep + i] == 255){
                	continue_turn_left = false;
                	printf("continue_turn_flag_off__2_left__\n");
                    break;
                }
                else{
                	printf("continue_turn_flag_on__2_left__\n");
                	continue_turn_left = true;
                	break;
                }
            }
        }
        else if (turn_right_max ==true){
            for(i = 0 ; i < (imgResult->width/2) - line_width ; i++){
            	if(imgResult->imageData[y_start_line*imgResult->widthStep + i] == 255){
                	continue_turn_right = false;
                	printf("continue_turn_flag_off__2_right__\n");
                    break;
                }
                else{
                	printf("continue_turn_flag_on__2_right__\n");
                	continue_turn_right = true;
                	break;
                }
            }
        }
    }

    if (continue_turn_left == false && continue_turn_right == false){   //1. 직선인 경우, 조향을 위한 좌우측 차선 검출 후 기울기 계산
            printf("continue_turn_flag_off__3__\n");
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
            printf("    valid left line = %d\n",valid_left_amount);

            if(valid_left_amount > 1){                                          //좌측 차선 기울기 계산
                left_slope[0] = (float)(left[0] - left[(valid_left_amount-1)*line_gap])/(float)(valid_left_amount*line_gap);
            }
            else left_slope[0] = 0;

            if(valid_right_amount > 1){                                          //우측 차선 기울기 계산
                right_slope[0] = (float)(right[0] - right[(valid_right_amount-1)*line_gap])/(float)(valid_right_amount*line_gap);
            }
            else right_slope[0] = 0;

            control_angle = (left_slope[0] + right_slope[0])*weight;        //차량 조향 기울기 계산

            printf("left[0] = %d left[last] = %d right[0] = %d right[last] = %d \n",left[0] , left[(valid_left_amount-1)*line_gap],right[0] , right[(valid_right_amount-1)*line_gap]);
            printf("left_slope : %f ,right_slope : %f   	",left_slope[0],right_slope[0]);
            printf("Control_Angle : %f \n",control_angle);
        }

    turn_left_max = continue_turn_left;             //현재 프레임에서 최대조향이라고 판단할 경우, 최대조향 전역변수 set.
    turn_right_max = continue_turn_right;

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
    DesireSpeed_Write(speed);
    #endif

    for(i=0;i<imgResult->widthStep;i++){
    	imgResult->imageData[y_start_line*imgResult->widthStep + i] = 255;
    }

}

void SteeringServoControl_Write(int angle){
  sim_angle = angle;
  //printf("Servo call! angle : %d",sim_angle);
}

void DesireSpeed_Write(int speed){
  sim_speed = speed;
  //printf("Speed call! Speed : %d",sim_speed);
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
  mode = getchar();

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

  cvPutText(imgResult, str, myPoint, &font, cvScalar(255,255,255,0));
  //cvPutText(Mat&, string& ,textPoint, &font, cvScalar(255,255,255,0));

}

CvPoint getEndPoint(int angle){
  CvPoint point;
  //double x=-1, y=-1;
  int len = 208;
  double seta = 90 - (angle-1500)/10;

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

  cvLine(imgResult, point1, point2, CV_RGB(255,0,0), 2, 8, 0);
}



int main(int argc, char const *argv[]) {

  int index = 0; // index of image
  char file_name[40];
  char str_info[50];
  sprintf(file_name, "../captureImage/imgResult%d.png", index);

  //initializing images
  IplImage* img = cvLoadImage(file_name, CV_LOAD_IMAGE_GRAYSCALE);
  if(img==0){ //null check
    printf("No Testset Image! Index : %d\n",index);
    return;
  }
  img_width = cvGetSize(img).width;
  img_height = cvGetSize(img).height;
  //printf("width : %d",img_width);
  //printf("height : %d",img_height);
  IplImage* imgResult = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U,1);
  cvZero(imgResult);
  //TODO mode selecting
  //int mode = selectMode();


  //show the image
  cvNamedWindow("simulator",CV_WINDOW_AUTOSIZE);
  cvShowImage("simulator",imgResult);


  while(1){
    sprintf(file_name, "../captureImage/imgResult%d.png", index);
    img = cvLoadImage(file_name, CV_LOAD_IMAGE_GRAYSCALE);
    if(img==0){ //null check
      printf("No Testset Image! Index : %d\n",index);
      return;
    }

    //imgResult = img;
    imgResult = (IplImage*)cvClone(img);
    //TODO 이미지 처리
    Find_Center(imgResult);
    sprintf(str_info, "[Image %d]  Angle : %d, Speed : %d", index, sim_angle, sim_speed);
    writeonImage(imgResult, str_info);
    drawonImage(imgResult, sim_angle);

    cvShowImage("simulator",imgResult);

    int key = cvWaitKey(200);
    if(key=='q'){
      printf("Closing the Simulator...\n");
      return;
    }

    index++;
  }



  //TODO save images

  //close the image
  cvWaitKey(0);
  cvDestroyWindow("simulator");
  cvReleaseImage(&img);
  cvReleaseImage(&imgResult);

  return 0;
}
