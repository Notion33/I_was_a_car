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
int first_wall_count = 0; 

int filteredIR(int num) // 필터링한 적외선 센서값 
{
	int sensorValue = 0;

	for(j=0; j<30; j++)
	{
		sensorValue[i] += DistanceSensor(num);
	}

	sensorValue[i] /= 30;
	
	return sensorValue;
}

// void parallel_parking_left();
// void vertical_parking_left();
// void parallel_parking_right();
// void vertical_parking_right();

void main()
{
	CarControlInit();
	PositionControlOnOff_Write(UNCONTROL);
	SpeedControlOnOff_Write(CONTROL);

	SteeringServoControl_Write(1496);
	DesireSpeed_Write(120);

	channel_leftPrev = filteredIR(LEFT);
	channel_rightPrev = filteredIR(RIGHT);
	
	while(1)
	{
		channel_leftNow = filteredIR(LEFT);
		channel_rightNow = filteredIR(RIGHT);
		difference_left = channel_leftNow - channel_leftPrev;
		difference_right = channel_rightNow - channel_rightPrev;
		channel_leftPrev = channel_leftNow;
		channel_rightPrev = channel_rightNow;

		#ifdef debug_mode
		printf("difference_left = %d\n", difference_left);
		printf("difference_right = %d\n", difference_right);
		#endif

		if((first_wall_detect == FALSE && difference_left >= 150) || (first_wall_detect == FALSE && difference_right >= 150))
		{
			channel_leftNow = filteredIR(LEFT);
			channel_rightNow = filteredIR(RIGHT);
			difference_left = channel_leftNow - channel_leftPrev;
			difference_right = channel_rightNow - channel_rightPrev;
			channel_leftPrev = channel_leftNow;
			channel_rightPrev = channel_rightNow;
	
			#ifdef debug_mode
			printf("difference_left = %d\n", difference_left);
			printf("difference_right = %d\n", difference_right);
			#endif

			if(difference_left >= 150 || difference_right >= 150)
			{
				continue;
			}
			else
			{
				first_wall_detect = TRUE;
			}
		}
	
		if((first_wall_detect == TRUE && second_wall_detect == FALSE && difference_left <= -100) || (first_wall_detect == TRUE && second_wall_detect == FALSE && difference_right <= -100))
		{	
			channel_leftNow = filteredIR(LEFT);
			channel_rightNow = filteredIR(RIGHT);
			difference_left = channel_leftNow - channel_leftPrev;
			difference_right = channel_rightNow - channel_rightPrev;
			channel_leftPrev = channel_leftNow;
			channel_rightPrev = channel_rightNow;
			
			#ifdef debug_mode
			printf("difference_left = %d\n", difference_left);
			printf("difference_right = %d\n", difference_right);
			#endif

			if(difference_left <= -100 || difference_right <= -100)
			{
				continue;
			}
			else
			{
				second_wall_detect = TRUE;
				PositionControlOnOff_Write(CONTROL);
				PositionProportionPoint_Write(20);
				EncoderCounter_Write(0);
			}
		}

		if(second_wall_detect == TRUE && third_wall_detect == FALSE && difference_left >= 100)
		{
			channel_leftNow = filteredIR(LEFT);
			channel_rightNow = filteredIR(RIGHT);
			difference_left = channel_leftNow - channel_leftPrev;
			difference_right = channel_rightNow - channel_rightPrev;
			channel_leftPrev = channel_leftNow;
			channel_rightPrev = channel_rightNow;
	
			#ifdef debug_mode
			printf("difference_left = %d\n", difference_left);
			printf("difference_right = %d\n", difference_right);
			#endif

			if(difference_left >= 100 || difference_right >= 100)
			{
				continue;
			}
			else
			{
				parking_space = EncoderCounter_Read();
				PositionControlOnOff_Write(UNCONTROL);
				third_wall_detect = TRUE;
				#ifdef debug_mode
				printf("\n\nthird_wall_detect\n");
				printf("\n\nparking_space is %d\n\n", parking_space);
				#endif
				if(parking_space >= 7000)
				{
					//parallel_parking_left();
				}	
				else
				{
					//vertical_parking_left();
				}
			}
		}

		if(second_wall_detect == TRUE && third_wall_detect == FALSE && difference_right >= 100)
		{
			channel_leftNow = filteredIR(LEFT);
			channel_rightNow = filteredIR(RIGHT);
			difference_left = channel_leftNow - channel_leftPrev;
			difference_right = channel_rightNow - channel_rightPrev;
			channel_leftPrev = channel_leftNow;
			channel_rightPrev = channel_rightNow;

			#ifdef debug_mode
			printf("difference_left = %d\n", difference_left);
			printf("difference_right = %d\n", difference_right);
			#endif

			if(difference_right >= 100)
			{
				continue;
			}
			else
			{
				parking_space = EncoderCounter_Read();
				PositionControlOnOff_Write(UNCONTROL);
				third_wall_detect = TRUE;
				#ifdef debug_mode
				printf("\n\nthird_wall_detect\n");
				printf("\n\nparking_space is %d\n\n", parking_space);
				#endif
				if(parking_space >= 7000)
				{
					//parallel_parking_right();
				}
				else
				{
					//vertical_parking_right();
				}
			}
		}
}

/*
void parallel_parking_left() // 수평주차 
{
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

void parallel_parking_right() // 수평주차 
{
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
*/