//==============================================================================
//  Global variable & header file area
#include "stdbool.h"

#define SERVO_CONTROL
#define SPEED_CONTROL
#define IMGSAVE

#define UNCONTROL 0
#define CONTROL 1
#define whitepx -1

#define straight_speed 200
#define curve_speed 130

bool turn_left_max = false;         //TY add
bool turn_right_max = false;

bool Right_Max = false;
bool Left_Max = false;

//==============================================================================

void Find_Center(IplImage* imgResult)		//TY add 6.27
{
	int i=0;
    int j=0;
    int k=0;

    int y_start_line = 150;     //y_start_line과 y_end_line 차는 line_gap의 배수이어야 함.
    int y_end_line = 130;
    int y_high_start_line = 110;
    int y_high_end_line = 90;

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
    int tolerance = 25; // center pixel +- tolerance px 내에서 라인검출시 for문 종료 용도
    int high_tolerance = 60;
    //int angle=1500;
    float low_line_weight = 320; // control angle weight
    float high_line_weight = 80;
    float control_angle = 0;

    int left[240] = {0};
    int right[240] = {imgResult->width-1};
    float left_slope[2] = {0.0};
    float right_slope[2] = {0.0};

    bool continue_turn_left = false;
    bool continue_turn_right = false;

    for(i = y_start_line ; i>y_end_line ; i=i-line_gap){
		if (turn_right_max == true)
			j = imgResult->width - 1;
		else
			j = (imgResult->width) / 2;
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
                    if(k = low_line_width - 1){
                      valid_left_amount++;
                      break;
                    }
                }
		}
		if (turn_left_max == true)
			j = 0 ; 
		else
			j = (imgResult->width) / 2 ;
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
                    if(k = low_line_width - 1){
                      valid_right_amount++;
                      break;
                    }
                }
        }
        if(left[y_start_line-i]>((imgResult->width/2)-tolerance)||right[y_start_line-i]<((imgResult->width/2)+tolerance)){     //검출된 차선이 화면중앙부근에 있는경우, 차선검출 종료후 반대방향으로 최대조향 flag set
            if(valid_left_amount >= valid_right_amount && turn_left_max == false){
            	printf("continue_turn_right set!\n");
                continue_turn_right = true;
            }
            else if(valid_right_amount >= valid_left_amount && turn_right_max == false){
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
            printf("    valid right line = %d\n",valid_right_amount);

            if(valid_left_amount > 1){                                          //좌측 차선 기울기 계산
                left_slope[0] = (float)(left[0] - left[(valid_left_amount-1)*line_gap])/(float)(valid_left_amount*line_gap);
            }
            else left_slope[0] = 0;

            if(valid_right_amount > 1){                                          //우측 차선 기울기 계산
                right_slope[0] = (float)(right[0] - right[(valid_right_amount-1)*line_gap])/(float)(valid_right_amount*line_gap);
            }
            else right_slope[0] = 0;
            
            control_angle = (left_slope[0] + right_slope[0])*low_line_weight;        //차량 조향 기울기 계산

            printf("left_slope : %f ,right_slope : %f   	",left_slope[0],right_slope[0]);
            printf("Control_Angle_low : %f \n\n",control_angle);
        }

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
        //   if(left[y_high_start_line-i]>((imgResult->width/2)-high_tolerance)||right[y_high_start_line-i]<((imgResult->width/2)+high_tolerance))     //검출된 차선이 화면중앙부근에 있는경우, 아랫쪽차선까지 올수있도록 무시
        //       break;
        }

        for(i=0;i<=valid_high_left_amount;i++){                        //좌측 차선 검출
            if(left[i*line_gap]!=0){
                left_line_start = y_high_start_line - i * line_gap;
                left_line_end = y_high_start_line - (i + valid_high_left_amount) * line_gap;
                break;
            }
        }
        for(i=0;i<=valid_high_right_amount;i++){                        //우측 차선 검출
            if(right[i*line_gap]!=imgResult->width-1){
                right_line_start = y_high_start_line - i * line_gap;
                right_line_end = y_high_start_line - (i + valid_right_amount) * line_gap;
                break;
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
  
      if(abs(control_angle)>100)    //위쪽차선에서 과하게 꺾을경우, 방지 ; 코너에서 인코스로 들어오는걸 방지
        control_angle = 0;

    }


    if (turn_left_max == true)                      //차량 조향각도 판별
        angle = 2000;
    else if (turn_right_max == true)
        angle = 1000;
    else{
        angle = 1500 + control_angle ;                                  // Range : 1000(Right)~1500(default)~2000(Left)
		angle = angle>2000? 2000 : angle<1000 ? 1000 : angle;           // Bounding the angle range
    }
    //SteeringServoControl_Write(angle);

    #ifdef SPEED_CONTROL
        if(angle<1200||angle>1800)      //직선코스의 속도와 곡선코스의 속도 다르게 적용
           speed = curve_speed;
        else
           speed = straight_speed;
    #endif

    #ifdef ROI
        for(i=0;i<imgResult->widthStep;i++){
            imgResult->imageData[y_start_line*imgResult->widthStep + i] = whitepx;
            }
        for(i=0;i<imgResult->widthStep;i++){
            imgResult->imageData[y_end_line*imgResult->widthStep + i] = whitepx;
			}
			for(i=0;i<imgResult->widthStep;i++){
				imgResult->imageData[y_high_start_line*imgResult->widthStep + i] = whitepx;
			}
			for(i=0;i<imgResult->widthStep;i++){
				imgResult->imageData[y_high_end_line*imgResult->widthStep + i] = whitepx;
			}
    #endif
}