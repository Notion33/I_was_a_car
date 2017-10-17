#include <stdio.h>
#include <stdlib.h>
#include "car_lib.h"

#define TRUE 1
#define FALSE 0
#define LEFT 5
#define RIGHT 3

int channel_leftNow = 0;
int channel_leftPrev = 0;
int channel_rightNow = 0;
int channel_rightPrev = 0;

int difference_left = 0;
int difference_right = 0;

int first_left_detect = FALSE;
int second_left_detect = FALSE;
int third_left_detect = FALSE;

int first_right_detect = FALSE;
int second_right_detect = FALSE;
int third_right_detect = FALSE;

int parking_space = 0;
int sensor = 0;

int i;

int filteredIR(int num) // 필터링한 적외선 센서값
{
int sensorValue = 0;
for(i=0; i<25; i++)
{
sensorValue += DistanceSensor(num);
}
sensorValue /= 25;
return sensorValue;
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
	SteeringServoControl_Write(1470);
	DesireSpeed_Write(70);
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
		printf("difference_left = %d\n", difference_left);
		printf("difference_right = %d\n", difference_right);
	
		if(first_left_detect == FALSE && difference_left >= 200)
		{
			printf("\n\n-------------jumped over the threshold by %d-------------\n", difference_left);
			while(difference_left>=50)
			{
				channel_leftNow = filteredIR(LEFT);
				difference_left = channel_leftNow - channel_leftPrev;
				channel_leftPrev = channel_leftNow;
				printf("difference_left = %d\n", difference_left);
			}
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			printf("\n\nFIRST_LEFT_DETECT\n\n\n");
			first_left_detect = TRUE;
		}
	
		if(first_right_detect == FALSE && difference_right >= 200)
		{
			printf("\n\n-------------jumped over the threshold by %d-------------\n", difference_right);
			while(difference_right>=50)
			{
				channel_rightNow = filteredIR(RIGHT);
				difference_right = channel_rightNow - channel_rightPrev;
				channel_rightPrev = channel_rightNow;
				printf("difference_right = %d\n", difference_right);
			}
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_right);
			printf("\n\nFIRST_RIGHT_DETECT\n\n\n");
			first_right_detect = TRUE;
		}
	
		if(first_left_detect == TRUE && second_left_detect == FALSE && difference_left <= -300)
		{
			printf("\n\n-------------jumped under the threshold by %d-------------\n", difference_left);
			while(difference_left<=-50)
			{
				channel_leftNow = filteredIR(LEFT);
				difference_left = channel_leftNow - channel_leftPrev;
				channel_leftPrev = channel_leftNow;
				printf("difference_left = %d\n", difference_left);
			}
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			printf("\n\nSECOND_LEFT_DETECT\n\n\n");
			second_left_detect = TRUE;
			PositionControlOnOff_Write(UNCONTROL);
			EncoderCounter_Write(0);
		}

		if(first_right_detect == TRUE && second_right_detect == FALSE && difference_right <= -300)
		{
			printf("\n\n-------------jumped under the threshold by %d-------------\n", difference_right);
			while(difference_right<=-50)
			{
				channel_rightNow = filteredIR(RIGHT);
				difference_right = channel_rightNow - channel_rightPrev;
				channel_rightPrev = channel_rightNow;
				printf("difference_right = %d\n", difference_right);
			}
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_right);
			printf("\n\nSECOND_RIGHT_DETECT\n\n\n");
			second_right_detect = TRUE;
			PositionControlOnOff_Write(UNCONTROL);
			EncoderCounter_Write(0);
		}

		if(second_left_detect == TRUE && third_left_detect == FALSE && difference_left >= 200)
		{
			printf("\n\n-------------jumped under the threshold by %d-------------\n", difference_left);
			while(difference_left>=50)
			{
				channel_leftNow = filteredIR(LEFT);
				difference_left = channel_leftNow - channel_leftPrev;
				channel_leftPrev = channel_leftNow;
				printf("difference_left = %d\n", difference_left);
			}
			parking_space = EncoderCounter_Read();
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_left);
			printf("\n\nTHIRD_LEFT_DETECT\n\n\n");
			third_left_detect = TRUE;
			printf("\n\n PARKING SPACE : %d", parking_space);
			if(parking_space > 7000)
			{
				parallel_parking_left();
				break;
			}
			else
			{
				vertical_parking_left();
				break;
			}
		}

		if(second_right_detect == TRUE && third_right_detect == FALSE && difference_right >= 200)
		{
			printf("\n\n-------------jumped under the threshold by %d-------------\n", difference_right);
			while(difference_right>=50)
			{
				channel_rightNow = filteredIR(RIGHT);
				difference_right = channel_rightNow - channel_rightPrev;
				channel_rightPrev = channel_rightNow;
				printf("difference_right = %d\n", difference_right);
			}
			parking_space = EncoderCounter_Read();
			printf("\n\n-------------escaped the loop by %d-------------\n", difference_right);
			printf("\n\nTHIRD_RIGHT_DETECT\n\n\n");
			third_right_detect = TRUE;
			printf("\n\nPARKING SPACE : %d", parking_space);
			if(parking_space > 7000)
			{
				parallel_parking_right();
				break;
			}
			else
			{
				vertical_parking_right();
				break;
			}
		}	
	}
	
	DesireSpeed_Write(0);
}


void vertical_parking_left() // ?섏쭅 二쇱감 
{	
	EncoderCounter_Write(0);
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() >= -8500)
	{
		DesireSpeed_Write(-120);
	}   

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(1)
	{
		sensor = filteredIR(4);
		printf("sensor = %d \n", sensor);
		if(sensor >= 600)
		{
			DesireSpeed_Write(0);
			break;
		}
	} 

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 3000)
	{
		DesireSpeed_Write(120);
	} // ?꾩쭊

	EncoderCounter_Write(0);
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() <= 8500)
	{
		DesireSpeed_Write(120);
	} // 90???뚯쟾

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 3000)
	{
		DesireSpeed_Write(120);
	} // ?꾩쭊
}

void vertical_parking_right() // ?섏쭅 二쇱감 
{	
	EncoderCounter_Write(0);
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() >= -8000)
	{
		DesireSpeed_Write(-120);
	} // 90???뚯쟾  

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(1)
	{
		sensor = filteredIR(4);
		printf("sensor = %d \n", sensor);
		if(sensor >= 600)
		{
			DesireSpeed_Write(0);
			break;
		}
	}

	DesireSpeed_Write(0);
	CarLight_Write(ALL_ON);
    usleep(1000000);
    CarLight_Write(ALL_OFF);
    Alarm_Write(ON);
    usleep(100000);
	Alarm_Write(OFF);
	
	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 2000)
	{
		DesireSpeed_Write(120);
	} // ?꾩쭊

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() <= 9000)
	{
		DesireSpeed_Write(120);
	} // 90???뚯쟾

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1496);
	while(EncoderCounter_Read() <= 3000)
	{
		DesireSpeed_Write(120);
	} 
}

void parallel_parking_right()
{
	/////////////////////////////////// 들어가기 ////////////////////////////////////////////

	EncoderCounter_Write(0);
	SteeringServoControl_Write(1470);
	while(EncoderCounter_Read() <= 500)
	{
		DesireSpeed_Write(120);
	} 

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() >= -5000)
	{
		DesireSpeed_Write(-120);
	}   

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);
	while(1)
	{
		sensor = filteredIR(4);
		printf("sensor = %d \n", sensor);
		if(sensor >= 600)
		{
			DesireSpeed_Write(0);
			break;
		}
	}

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() >= -6000)
	{
		DesireSpeed_Write(-120);
	}

	DesireSpeed_Write(0);
	CarLight_Write(ALL_ON);
    usleep(1000000);
    CarLight_Write(ALL_OFF);

    Alarm_Write(ON);
    usleep(100000);
	Alarm_Write(OFF);
	
 	////////////////////////////////////////// 나가기 //////////////////////////// 

 	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() <= 6000)
	{
		DesireSpeed_Write(120);
	}

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);
	while(EncoderCounter_Read() <= 2500)
	{
		DesireSpeed_Write(120);
	}

	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() <= 5000)
	{
		DesireSpeed_Write(120);
	} 
}

void parallel_parking_left()
{
	/////////////////////////////////// 들어가기 ////////////////////////////////////////////
	EncoderCounter_Write(0);	
	SteeringServoControl_Write(1470);
	while(EncoderCounter_Read() <= 500)
	{
		DesireSpeed_Write(120);
	} 
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);	
	while(EncoderCounter_Read() >= -5000)
	{
		DesireSpeed_Write(-120);
	}   
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);	
	while(1)
	{
		sensor = filteredIR(4);
		printf("sensor = %d \n", sensor);
		if(sensor >= 600)
		{
			DesireSpeed_Write(0);
			break;
		}
	}  
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);	
	while(EncoderCounter_Read() >= -6000)
	{
		DesireSpeed_Write(-120);
	}
	
	DesireSpeed_Write(0);
	CarLight_Write(ALL_ON);
	usleep(1000000);
	CarLight_Write(ALL_OFF);
	
	Alarm_Write(ON);
	usleep(100000);
	Alarm_Write(OFF);
	
	////////////////////////////////////////// 니가기 /////////////////////////////////
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1000);
	while(EncoderCounter_Read() <= 6000)
	{
		DesireSpeed_Write(120);
	}
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(1470);	
	while(EncoderCounter_Read() <= 2500)
	{
		DesireSpeed_Write(120);
	}
	
	EncoderCounter_Write(0);  
	SteeringServoControl_Write(2000);
	while(EncoderCounter_Read() <= 5000)
	{
		DesireSpeed_Write(120);
	} 
}