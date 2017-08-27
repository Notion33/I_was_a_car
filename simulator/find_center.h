//==============================================================================
//  Global variable & header file area
#include "stdbool.h"

#define SERVO_CONTROL
#define SPEED_CONTROL
#define IMGSAVE

#define UNCONTROL 0
#define CONTROL 1
#define whitepx -1
#define SHOWLOG

bool turn_left_max = false;         //TY add
bool turn_right_max = false;

bool Right_Max = false;
bool Left_Max = false;

//==============================================================================

void Find_Center(IplImage* imgResult)
{
    int i=0, j=0;
    int count = 0;
    //TODO 웨이트 주어 회전구간 구현
    int tWeight = 1;
    int refAngle = 1500;            //기준각
    int minAngleDef = 100;          //차선조정 차각
    int weakAngleDef = 300;         //약한곡선 차각
    int maxAngleDef = 500*tWeight;  //큰곡선 차각
    int straightSpeed = 130;        //직선속도
    int turningSpeed = 100;         //곡선속도
    int width = imgResult->width;
    int height = imgResult->height;
    int spacing = 40;                       // 중앙선 기준으로 spacing만큼은 무시
    int rightTop = 0, leftTop = 0;          // 0~3
    int rightBottom = 0, leftBottom = 0;    // 0~2

    char status[30];

    //  직진 코스 읽기
    int checkStraightLineY[4] = {50, 70, 130, 150}; // 상단과 하단의 1/6씩 버림

    //  상단부 2줄 읽어서 직진코스 확인
    for (i = 0; i < 2; i++) {
      count = 0;
      for(j=width/2 + spacing; j<width; j++){ // 우측 상단
        int idx = checkStraightLineY[i]*imgResult->widthStep + j;
        if(imgResult->imageData[idx] == whitepx){
          count++;
          if(count>5){    // 차선이라 판단
            rightTop = rightTop + i+1;    //가까울수록 가중치
            break;
          }
        }
      }
      count = 0;
      for(j=width/2 - spacing; j>=0; j--){    // 좌측 상단
        int idx = checkStraightLineY[i]*imgResult->widthStep + j;
        if(imgResult->imageData[idx] == whitepx){
          count++;
          if(count>5){    // 차선이라 판단
            leftTop = leftTop + i+1;      //가까울수록 가중치
            break;
          }
        }
      }
    }

    //  하단부는 비어야있어야 함
    for (i = 2; i < 4; i++) {
      count = 0;
      for(j=width/2 + spacing; j<width; j++){ // 우측 하단
        int idx = checkStraightLineY[i]*imgResult->widthStep + j;
        if(imgResult->imageData[idx] == whitepx){
          count++;
          if(count>5){    // 차선이라 판단
            rightBottom++;
            break;
          }
        }
      }
      count = 0;
      for(j=width/2 - spacing; j>=0; j--){    // 좌측 하단
        int idx = checkStraightLineY[i]*imgResult->widthStep + j;
        if(imgResult->imageData[idx] == whitepx){
          count++;
          if(count>5){    // 차선이라 판단
            leftBottom++;
            break;
          }
        }
      }
    }

    //  교점 분포에 따른 주행 판단
    // TODO TODO 상황별 시뮬레이션 보며 상황문 조정!!
    if(leftTop+rightTop >= 5){      // 3+3 or 3+2
        //직선
        angle = refAngle;
        speed = straightSpeed;                  //직진
        sprintf(status, "Straight!");

    } else if(leftTop+rightTop >= 4){   //3+1 or 2+2
        //직선 및 핸들 조정
        if(leftBottom > rightBottom){
            angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
            sprintf(status, "-- centerline ->");
        } else if(leftBottom < rightBottom){
            angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
            sprintf(status, "<- centerline --");
        } else if(leftBottom == rightBottom){
            angle = refAngle;                   //직진
            sprintf(status, "Straight!");
        }
        speed = straightSpeed;

    } else if(leftTop == 3 || rightTop == 3){   //Already leftTop+rightTop==3 이하
        //약한 곡선
        if(leftTop == 3 && leftBottom >= 1){
            angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
            speed = straightSpeed;
            sprintf(status, "-- centerline ->");
        } else if(rightTop == 3 && rightBottom >= 1){
            angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
            speed = straightSpeed;
            sprintf(status, "<- centerline --");
        } else if(leftTop == 3 && rightBottom == 2){
            angle = refAngle + weakAngleDef;    //약한 좌회전
            speed = turningSpeed;
            sprintf(status, "<- left turn --");
        } else if(rightTop == 3 && leftBottom == 2){
            angle = refAngle - weakAngleDef;    //약한 우회전
            speed = turningSpeed;
            sprintf(status, "-- right turn ->");
        }

    } else if(leftTop+rightTop == 3){   // right1 + left2 or right2+left1
        //전방에 약한 회전
        if(leftTop > rightTop){
            angle = refAngle - weakAngleDef;    //약한 우회전
            speed = turningSpeed;
            sprintf(status, "-- right turn ->");
        } else if(leftTop < rightTop){
            angle = refAngle + weakAngleDef;    //약한 좌회전
            speed = turningSpeed;
            sprintf(status, "<- left turn --");
        }

    } else if(rightTop+leftTop <= 2){
        //큰 곡선 : 최대조향각
        if(rightBottom == 0 && leftBottom == 0){
            angle = refAngle;
            speed = straightSpeed;                  //직진
            sprintf(status, "Straight!");
        }
    } else if(rightTop+leftTop == 0){
        //큰 곡선 : 최대조향각
        if(leftBottom == 2){
            angle = refAngle - maxAngleDef;     //최대조향 우회전
            speed = turningSpeed;
            sprintf(status, "--- Max Right Turn! >>>");
        } else if(rightBottom == 2){
            angle = refAngle + maxAngleDef;     //최대조향 좌회전
            speed = turningSpeed;
            sprintf(status, "<<< Max Left Turn! ---");
        } else if(rightBottom == 0 && leftBottom == 0){
            angle = refAngle;
            speed = straightSpeed;                  //직진
            sprintf(status, "Straight!");
        }
    }

    else {
        //angle 유지
    }

    #ifdef SHOWLOG
    //TODO 현재상태 문자열로 반환

    printf("\n");
    printf("============\n");
    printf("|| %d || %d ||\n",leftTop, rightTop);
    printf("|| %d || %d ||\n",leftBottom, rightBottom);
    printf("============\n");
    printf("Angle : %d  Speed : %d\n", angle, speed);
    printf("Current Status : %s\n", status);  //TODO 현재 상태 문자열로 출력
    #endif

    //DesireSpeed_Write(speed);
    //SteeringServoControl_Write(angle);
}
