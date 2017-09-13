#include <stdio.h>
#include <stdlib.h>
#include "car_lib.h"

#define TRUE 1
#define FALSE 0
#define LEFT 5
#define RIGHT 3
#define debug_mode

int channel_leftNow;
int channel_leftPrev;
int channel_rightNow;
int channel_rightPrev;
int difference_left;
int difference_right;
int first_wall_detect = FALSE;
int second_wall_detect = FALSE;
int third_wall_detect = FALSE; 
int parking_space = 0;

void swap(int *ptr1, int *ptr2) 
{
	int temp = *ptr1;

	if (*ptr1 >= *ptr2)
	{
		*ptr1 = *ptr2;
		*ptr2 = temp;
	}
}

int filteredIR(int num) // 필터링한 적외선 센서값 
{
	int sensorValue[5] = { 0 };

	for(i=0; i<5; i++)
	{
		for(j=0; j<10; j++)
		{
			sensorValue[i] += DistanceSensor(num);
		}

		sensorValue[i] /= 10;
	}

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			swap(sensorValue + j, sensorValue + j + 1);
		}
	}

	return sensorValue[2];
}

void parallel_parking_left();
void vertical_parking_left();
void parallel_parking_right();
void vertical_parking_right();

void main()
{
	CarControlInit();
	PositionControlOnOff_Write(UNCONTROL);
	SpeedControlOnOff_Write(CONTROL);

	int i=0;

	SteeringServoControl_Write(1496);
	DesireSpeed_Write(100);

	channel_leftPrev = filteredIR(LEFT);
	channel_rightPrev = filteredIR(RIGHT);
	
	while(1)
	{
		channel_leftNow = 0;
		channel_rightNow = 0;
		channel_leftNow = filteredIR(LEFT);
		channel_rightNow = filteredIR(RIGHT);
		difference_left = channel_leftNow - channel_leftPrev;
		difference_right = channel_rightNow - channel_rightPrev;

		#ifdef debug_mode
		printf("channel_leftNow = %d\n", channel_leftNow);
		printf("difference = %d\n", difference);
		#endif

		channel_leftPrev = channel_leftNow;
		channel_rightPrev = channel_rightNow;

		if((first_wall_detect == FALSE && difference_left >= 150) || (first_wall_detect == FALSE && difference_right >= 150))
		{
			first_wall_detect = TRUE;
			#ifdef debug_mode
			printf("\n\nfirst_wall_detect\n\n\n");
			#endif
		}
	
		if((first_wall_detect == TRUE && second_wall_detect == FALSE && difference_left <= -100) || (first_wall_detect == TRUE && second_wall_detect == FALSE && difference_right <= -100))
		{	
			second_wall_detect = TRUE;

			#ifdef debug_mode
			printf("\n\nsecond_wall_detect\n\n\n");
			#endif
	
			PositionControlOnOff_Write(CONTROL);
			PositionProportionPoint_Write(20);
			EncoderCounter_Write(0);
			DesireEncoderCount_Write(10000);
		}

		if(second_wall_detect == TRUE && third_wall_detect == FALSE && difference_left >= 100)
		{
	
			parking_space = EncoderCounter_Read();
			DesireEncoderCount_Write(0); // 멈추기
			PositionControlOnOff_Write(UNCONTROL);
			third_wall_detect = TRUE;

			#ifdef debug_mode
			printf("\n\nthird_wall_detect\n");
			printf("\n\nparking_space is %d\n\n", parking_space);
			#endif

			DesireSpeed_Write(0);

			if(parking_space > 7000) // 인코더로 측정한 두번째 벽이 7000보다 클 경우
			{
				#ifdef debug_mode
				printf("parking space is %d\n", parking_space);
				printf("parallel_parking\n");
				#endif

				parallel_parking_left();
				break;
			}
			else // 그렇지 않을 경우
			{
				#ifdef debug_mode
				printf("parking space is %d\n", parking_space);
				printf("vertical_parking\n");
				#endif

				vertical_parking_left();
				break;
			}
		}

		if(second_wall_detect == TRUE && third_wall_detect == FALSE && difference_right >= 100)
		{
			parking_space = EncoderCounter_Read();
			DesireEncoderCount_Write(0); // 멈추기
			PositionControlOnOff_Write(UNCONTROL);
			third_wall_detect = TRUE;

			#ifdef debug_mode
			printf("\n\nthird_wall_detect\n");
			printf("\n\nparking_space is %d\n\n", parking_space);
			#endif

			DesireSpeed_Write(0);

			if(parking_space > 7000)
			{
				#ifdef debug_mode
				printf("parking space is %d\n", parking_space);
				printf("parallel_parking\n");
				#endif

				parallel_parking_right();
				break;
			}
			else
			{
				#ifdef debug_mode
				printf("parking space is %d\n", parking_space);
				printf("vertical_parking\n");
				#endif

				vertical_parking_right();
				break;
			}
		}
}

void parallel_parking_left() // 수평주차 
{
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() >= -8500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	} // 90 turn

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}
	
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);

	while(EncoderCounter_Read() <= 4250)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -5000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(2000);

	while(EncoderCounter_Read() <= 2000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}
	
	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(2000);

	while(EncoderCounter_Read() <= 1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() <= 1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -1000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}
}

void vertical_parking_left() // 수직 주차 
{	
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() >= -8500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	} // 90도 회전  

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	} // 후진 

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() <= 3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	} // 전진

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() <= 8500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	} // 90도 회전

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() <= 3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	} // 전진
}

////////////////////////////////////////////////////////////////////////////////////////
void parallel_parking_right() // 수평주차 
{
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1000);
	
	while(EncoderCounter_Read() >= -8500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	} // 90 turn

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}
	
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1000);

	while(EncoderCounter_Read() <= 4250)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -5000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1000);

	while(EncoderCounter_Read() <= 2000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}
	
	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1000);

	while(EncoderCounter_Read() <= 1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1000);
	
	while(EncoderCounter_Read() <= 1500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -1000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	}
}

void vertical_parking_right() // 수직 주차 
{	
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1000);
	
	while(EncoderCounter_Read() >= -8500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	} // 90도 회전  

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(-50);
	} // 후진 

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() <= 3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	} // 전진

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1000);
	
	while(EncoderCounter_Read() <= 8500)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	} // 90도 회전

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() <= 3000)
	{
		#ifdef debug_mode
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		#endif
		DesireSpeed_Write(50);
	} // 전진
}