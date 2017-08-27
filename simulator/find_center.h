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
    int spacing = 30;                       // 중앙선 기준으로 spacing만큼은 무시
    int rightTop = 0, leftTop = 0;          // 0~3
    int rightBottom = 0, leftBottom = 0;    // 0~2
    int rightAssist = 0, leftAssist = 0;    // 0 or 1

    char status[30];

    //  직진 코스 읽기
    int checkStraightLineY[5] = {55, 70, 80, 130, 150}; // 상단과 하단의 1/6씩 버림
    //int checkStraightLineY[5] = {65, 75, 80, 150, 170}; // 상단과 하단의 1/6씩 버림
    if(turn_left_max || turn_right_max){
        printf("In MAXMODE! \n");
        checkStraightLineY[0] = 85;
        checkStraightLineY[1] = 90;
        checkStraightLineY[2] = 100;
    }

    //  상단부 2줄 읽어서 직진코스 확인
    for (i = 0; i < 2; i++) {
      count = 0;
      for(j=width/2 + spacing; j<width-spacing/4; j++){ // 우측 상단
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
      for(j=width/2 - spacing; j>=0+spacing/4; j--){    // 좌측 상단
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

    // 상단 Assist line : 커브 판단 보조
    count = 0;
    for(j=width/2 + spacing; j<width; j++){ // 우측 상단
      int idx = checkStraightLineY[2]*imgResult->widthStep + j;
      if(imgResult->imageData[idx] == whitepx){
        count++;
        if(count>5){    // 차선이라 판단
          rightAssist = 1;
          break;
        }
      }
    }
    count = 0;
    for(j=width/2 - spacing; j>=0; j--){    // 좌측 상단
      int idx = checkStraightLineY[2]*imgResult->widthStep + j;
      if(imgResult->imageData[idx] == whitepx){
        count++;
        if(count>5){    // 차선이라 판단
          leftAssist = 1;
          break;
        }
      }
    }

    //  하단부는 비어야있어야 함
    for (i = 3; i < 5; i++) {
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

    //==========================================================================
    //==========================================================================

    // // 최대조향각 유지 (매끄럽게 하기위해 or 노이즈 영향 최소화)
    // if(turn_left_max || turn_right_max){
    //
    // }

    // 최대조향 유지가 아닌 경우
    if(leftTop + rightTop == 6){
        // 상단 2줄 모두 검출 : 무조건 직진
        angle = refAngle;
        speed = straightSpeed;
        sprintf(status, "Straight!");

    } else if(leftTop + rightTop == 5){
        angle = refAngle;
        speed = straightSpeed;
        sprintf(status, "Straight!");

    } else if(leftTop + rightTop == 4){

        if(leftBottom == rightBottom){  //직진
            angle = refAngle;
            speed = straightSpeed;
            sprintf(status, "Straight!");

        } else if(leftBottom == 1){     //우로 차선조정
            angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
            speed = straightSpeed;
            sprintf(status, "-- centerline ->");

        } else if(rightBottom == 1){    //좌로 차선조정
            angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
            speed = straightSpeed;
            sprintf(status, "<- centerline --");

        } else if(leftBottom == 2){     //약한 우회전
            angle = refAngle - weakAngleDef;    //약한 우회전
            speed = turningSpeed;
            sprintf(status, "-- right turn ->");

        } else if(rightBottom == 2){    //약한 좌회전
            angle = refAngle + weakAngleDef;    //약한 좌회전
            speed = turningSpeed;
            sprintf(status, "<- left turn --");

        }

    } else if(leftTop + rightTop == 3){

        if(leftTop == 3){               //왼쪽 상단만 검출

            if(rightBottom == 2){       //강한 좌회전
                angle = refAngle + maxAngleDef;     //최대조향 좌회전
                speed = turningSpeed;
                turn_left_max = true;
                sprintf(status, "<<< Max Left Turn! ---");

            } else if(leftAssist == 1){             //우로 차선이동
                if(!turn_right_max){
                    angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
                    speed = straightSpeed;
                    sprintf(status, "-- centerline ->");
                } else {    //최대조향 유지
                    angle = refAngle - maxAngleDef;     //최대조향 우회전
                    speed = turningSpeed;
                    turn_right_max = true;
                    sprintf(status, "--- Max Right Turn! >>>");
                }

            } else if(leftBottom >= 1){             //우로 차선이동
                angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
                speed = straightSpeed;
                sprintf(status, "-- centerline ->");

            } else {
                angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
                speed = straightSpeed;
                sprintf(status, "<- centerline --");
            }


        } else if(rightTop == 3){       //오른쪽 상단만 검출

            if(leftBottom == 2){        //강한 우회전
                angle = refAngle - maxAngleDef;     //최대조향 우회전
                speed = turningSpeed;
                turn_right_max = true;
                sprintf(status, "--- Max Right Turn! >>>");

            } else if(rightAssist == 1){            //좌로 차선이동
                if(!turn_left_max){
                    angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
                    speed = straightSpeed;
                    sprintf(status, "<- centerline --");
                } else {    // 최대조향 유지
                    angle = refAngle + maxAngleDef;     //최대조향 좌회전
                    speed = turningSpeed;
                    turn_left_max = true;
                    sprintf(status, "<<< Max Left Turn! ---");
                }


            } else if(rightBottom >= 1){            //좌로 차선이동
                angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
                speed = straightSpeed;
                sprintf(status, "<- centerline --");

            } else {
                angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
                speed = straightSpeed;
                sprintf(status, "-- centerline ->");
            }

        } else if(leftTop > rightTop){  //전방에 우회전
            angle = refAngle - weakAngleDef;    //약한 우회전
            speed = turningSpeed;
            sprintf(status, "-- right turn ->");

        } else if(leftTop < rightTop){  //전방에 좌회전
            angle = refAngle + weakAngleDef;    //약한 좌회전
            speed = turningSpeed;
            sprintf(status, "<- left turn --");

        }

    } else if(leftTop + rightTop == 2){

        if(leftTop == rightTop){
            if(leftBottom == rightBottom){  //직진 1:1
                angle = refAngle;
                speed = straightSpeed;
                sprintf(status, "Straight!");

            } else if(leftBottom > rightBottom){    //약한 우회전
                angle = refAngle - weakAngleDef;    //약한 우회전
                speed = turningSpeed;
                sprintf(status, "-- right turn ->");

            } else if(leftBottom < rightBottom){    //약한 좌회전
                angle = refAngle + weakAngleDef;    //약한 좌회전
                speed = turningSpeed;
                sprintf(status, "<- left turn --");

            }

        } else if(leftTop == 2){
            if(leftBottom == rightBottom){          //약한 우회전
                angle = refAngle - weakAngleDef;    //약한 우회전
                speed = turningSpeed;
                sprintf(status, "-- right turn ->");

            } else if(leftBottom > rightBottom){
                // angle = refAngle - weakAngleDef;    //약한 우회전
                // speed = turningSpeed;
                // sprintf(status, "-- right turn ->");

                angle = refAngle - maxAngleDef;     //최대조향 우회전
                speed = turningSpeed;
                turn_right_max = true;
                sprintf(status, "--- Max Right Turn! >>>");

            }

        } else if(rightTop == 2){
            if(leftBottom == rightBottom){          //약한 좌회전
                angle = refAngle + weakAngleDef;    //약한 좌회전
                speed = turningSpeed;
                sprintf(status, "<- left turn --");

            } else if(leftBottom < rightBottom){
                // angle = refAngle + weakAngleDef;    //약한 좌회전
                // speed = turningSpeed;
                // sprintf(status, "<- left turn --");

                angle = refAngle + maxAngleDef;     //최대조향 좌회전
                speed = turningSpeed;
                turn_left_max = true;
                sprintf(status, "<<< Max Left Turn! ---");

            }

        }

    } else if(leftTop + rightTop == 1){ //TODO 튀는 값 존재

        if(leftTop == 1){
            /*if(leftAssist < rightAssist){   //강한 좌회전
                angle = refAngle + maxAngleDef;     //최대조향 좌회전
                speed = turningSpeed;
                turn_left_max = true;
                sprintf(status, "<<< Max Left Turn! ---");

            } else */if(leftBottom > rightBottom){   //우로 차선조정
                angle = refAngle - minAngleDef;     //핸들 살짝 오른쪽
                speed = straightSpeed;
                sprintf(status, "-- centerline ->");
            } else if(leftBottom < rightBottom){    //약한 좌회전
                angle = refAngle + weakAngleDef;    //약한 좌회전
                speed = turningSpeed;
                sprintf(status, "<- left turn --");
            }
            else if(leftBottom == 0 && rightBottom == 0){
                angle = refAngle;
                speed = straightSpeed;
                sprintf(status, "Straight!");
            }

        } else if(rightTop == 1){
            /*if(leftAssist > rightAssist){   //강한 우회전
                angle = refAngle - maxAngleDef;     //최대조향 우회전
                speed = turningSpeed;
                turn_right_max = true;
                sprintf(status, "--- Max Right Turn! >>>");

            } else */if(leftBottom < rightBottom){   //좌로 차선조정
                angle = refAngle + minAngleDef;     //핸들 살짝 왼쪽
                speed = straightSpeed;
                sprintf(status, "<- centerline --");

            } else if(leftBottom > rightBottom){    //약한 우회전
                angle = refAngle - weakAngleDef;    //약한 우회전
                speed = turningSpeed;
                sprintf(status, "-- right turn ->");

            }
            else if(leftBottom == 0 && rightBottom == 0){
                angle = refAngle;
                speed = straightSpeed;
                sprintf(status, "Straight!");
            }
        }

    } else if(leftTop + rightTop == 0){
        if(leftBottom == rightBottom){          //직진
            angle = refAngle;
            speed = straightSpeed;
            sprintf(status, "Straight!");

        } else if(leftBottom > rightBottom){    //강한 우회전
            angle = refAngle - maxAngleDef;     //최대조향 우회전
            speed = turningSpeed;
            turn_right_max = true;
            sprintf(status, "--- Max Right Turn! >>>");
        } else if(leftBottom < rightBottom){    //강한 좌회전
            angle = refAngle + maxAngleDef;     //최대조향 좌회전
            speed = turningSpeed;
            turn_left_max = true;
            sprintf(status, "<<< Max Left Turn! ---");
        }
    }

    else {
        //angle 유지
    }

    if(turn_left_max || turn_right_max){
        if(angle != refAngle+maxAngleDef){
            turn_left_max = false;
        }
        if(angle != refAngle-maxAngleDef){
            turn_right_max = false;
        }
    }

    #ifdef SHOWLOG
    //TODO 현재상태 문자열로 반환

    printf("\n");
    printf("============\n");
    printf("|| %d || %d ||\n",leftTop, rightTop);
    printf("|| %d || %d ||\n",leftAssist, rightAssist);
    printf("|| %d || %d ||\n",leftBottom, rightBottom);
    printf("============\n");
    printf("Top : %d Assist : %d\n", leftTop+rightTop, leftAssist+rightAssist);
    printf("Angle : %d  Speed : %d\n", angle, speed);
    printf("leftMax : %s ||  rightMax : %s\n", turn_left_max ? "true" : "false", turn_right_max ? "true" : "false");
    printf("Current Status : %s\n\n", status);  //TODO 현재 상태 문자열로 출력
    #endif

    //DesireSpeed_Write(speed);
    //SteeringServoControl_Write(angle);
}
