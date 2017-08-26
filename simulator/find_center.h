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
    int rightTop = 0, leftTop = 0;          // 0~2
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
            rightTop++;
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
            leftTop++;
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
    if(leftTop+rightTop == 4){
        //직선
        angle = refAngle;
        speed = straightSpeed;                  //직진
        sprintf(status, "Straight!");

    } else if(leftTop+rightTop == 3){
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

    } else if(leftTop == 2 || rightTop == 2){   //Already leftTop+rightTop==2
        //약한 곡선
        if(leftTop == 2 && leftBottom >= 1){
            angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
            speed = straightSpeed;
            sprintf(status, "-- centerline ->");
        } else if(rightTop == 2 && rightBottom >= 1){
            angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
            speed = straightSpeed;
            sprintf(status, "<- centerline --");
        } else if(leftTop == 2 && rightBottom == 2){
            angle = refAngle + weakAngleDef;    //약한 좌회전
            speed = turningSpeed;
            sprintf(status, "<- left turn --");
        } else if(rightTop == 2 && leftBottom == 2){
            angle = refAngle - weakAngleDef;    //약한 우회전
            speed = turningSpeed;
            sprintf(status, "-- right turn ->");
        }

    } else if(rightTop+leftTop < 2){
        //큰 곡선 : 최대조향각
        if(leftBottom >= 1){
            angle = refAngle - maxAngleDef;     //최대조향 우회전
            sprintf(status, "--- Max Right Turn! >>>");
        } else if(rightTop >= 1){
            angle = refAngle + maxAngleDef;     //최대조향 좌회전
            sprintf(status, "<<< Max Left Turn! ---");
        }
        speed = turningSpeed;
    }


    /*
    if(leftTop==2 && rightTop==2 && leftBottom==0 && rightBottom==0){           //직선, 상반부만 검출됨
        angle = 1500;
    } else if(leftTop+rightTop>=3 && leftBottom==0 && rightBottom==0){          //약한 직선
        angle = 1500;
    } else if(leftTop==0 && rightTop==2 && leftBottom==0 && rightBottom>=1) {   //중앙선이탈(우측치우침), 우측차선 크게 검출
        angle = 1600;   //TODO 더 세밀하게 조향
    } else if(leftTop==2 && rightTop==0 && leftBottom>=1 && rightBottom==0) {   //중앙선이탈(좌측치우침), 좌측차선 크게 검출
        angle = 1400;   //TODO 더 세밀하게 조향
    } else if(leftTop>=1 && rightTop==2 && leftBottom==0 && rightBottom>=1) {   //약한 좌회전, 우측차선 크게 검출
        angle = 1800;   //TODO 더 세밀하게 조향
    } else if(leftTop==2 && rightTop>=1 && leftBottom>=1 && rightBottom==0) {   //약한 우회전, 좌측차선 크게 검출
        angle = 1200;   //TODO 더 세밀하게 조향
    } else if(leftTop<=1 && rightTop==0 && leftBottom==0 && rightBottom==2) {   //강한 좌회전, 우측차선만 검출
        angle = 2000;   //TODO 범위 더 정확하게
    } else if(leftTop==0 && rightTop<=1 && leftBottom==2 && rightBottom==0) {   //강한 우회전, 좌측차선만 검출
        angle = 1000;   //TODO 범위 더 정확하게
    }
    */

    else {
        //angle 유지
    }

    #ifdef SHOWLOG
    //TODO 현재상태 문자열로 반환
    /*
    if(leftTop==2 && rightTop==2 && leftBottom==0 && rightBottom==0){           //직선, 상반부만 검출됨
        sprintf(status, "Straight!");
    } else if(leftTop+rightTop>=3 && leftBottom==0 && rightBottom==0){          //약한 직선
        sprintf(status, "weak Straight!");
    } else if(leftTop==0 && rightTop==2 && leftBottom==0 && rightBottom==1) {   //중앙선이탈(우측치우침), 우측차선 크게 검출
        sprintf(status, "<- centerline --");
    } else if(leftTop==2 && rightTop==0 && leftBottom==1 && rightBottom==0) {   //중앙선이탈(좌측치우침), 좌측차선 크게 검출
        sprintf(status, "-- centerline ->!");
    } else if(leftTop>=1 && rightTop==2 && leftBottom==0 && rightBottom>=1) {   //약한 좌회전, 우측차선 크게 검출
        sprintf(status, "left turn <-");
    } else if(leftTop==2 && rightTop>=1 && leftBottom>=1 && rightBottom==0) {   //약한 우회전, 좌측차선 크게 검출
        sprintf(status, "right turn ->");
    } else if(leftTop<=1 && rightTop==0 && leftBottom==0 && rightBottom==2) {   //강한 좌회전, 우측차선만 검출
        sprintf(status, "Max Left Turn! <<<-");
    } else if(leftTop==0 && rightTop<=1 && leftBottom==2 && rightBottom==0) {   //강한 우회전, 좌측차선만 검출
        sprintf(status, "Max Right Turn! ->>>");
    }
    */

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
