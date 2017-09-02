#include <stdio.h>
#include <stdlib.h>
#include "car_lib.h"

int channel_left[1000];
int difference;
int first_wall_detect = 0;
int second_wall_detect = 0;
int third_wall_detect = 0; 
int parking_space = 0;

// void parallel_parking();
// void vertical_parking();

void main()
{
	CarControlInit();
	PositionControlOnOff_Write(UNCONTROL);
	SpeedControlOnOff_Write(CONTROL);

	int i=1;
	int j=0;

	SteeringServoControl_Write(1490);
	DesireSpeed_Write(100);
	
	channel_left[0] = 0;
	for(j=0 ; j<10; j++)
	{
	channel_left[0] += DistanceSensor(5);
	}
	channel_left[0] /= 5;
	printf("channel_left[0] = %d\n", channel_left[0]); 
		
	while(1)
	{

	channel_left[i] = 0;
	for(j=0 ; j<5 ; j++)
	{
	channel_left[i] += DistanceSensor(5);
	}
	channel_left[i] /= 5;
	printf("channel_left[%d] = %d\n", i, channel_left[i]);
	
	difference = channel_left[i] - channel_left[i-1];
	printf("difference = %d\n", difference);

	if(first_wall_detect == 0 && difference >= 150)
	{
	first_wall_detect++;
	printf("\n\nfirst_wall_detect\n\n\n");
	}
	
	if(first_wall_detect == 1 && second_wall_detect == 0 && difference <= -100)
	{	
	second_wall_detect++;
	printf("\n\nsecond_wall_detect\n\n\n");
	
	PositionControlOnOff_Write(CONTROL);
	PositionProportionPoint_Write(20);
	EncoderCounter_Write(0);
	DesireEncoderCount_Write(10000);
	}

	if(second_wall_detect == 1 && third_wall_detect == 0 && difference >= 100)
	{
	
	parking_space = EncoderCounter_Read();
	DesireEncoderCount_Write(0); // 멈추기
	PositionControlOnOff_Write(UNCONTROL);
	third_wall_detect++;
	printf("\n\nthird_wall_detect\n");
	printf("\n\nparking_space is %d\n\n", parking_space);
	DesireSpeed_Write(0);

	if(parking_space > 7000)
	{
		printf("parking space is %d\n", parking_space);
		printf("parallel_parking\n");
		//parallel_parking();
		break;
	}
	else
	{
		printf("parking space is %d\n", parking_space);
		printf("vertical_parking\n");
		//vertical_parking();
		break;
	}
	
	// DesireSpeed_Write(0);
	// printf("STOP!\n");
	}
	i++;
	
	if(i==1000)
	{
	channel_left[0] = channel_left[999];
	i = 1;
	}	
}

void parallel_parking() // 수평주차 
{
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() >= -8500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	} // 90 turn

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -3000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	}
	
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);

	while(EncoderCounter_Read() <= 4250)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -5000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(2000);

	while(EncoderCounter_Read() <= 2000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	}
	
	EncoderCounter_Write(0);

	SteeringServoControl_Write(1496);

	while(EncoderCounter_Read() >= -1500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);

	SteeringServoControl_Write(2000);

	while(EncoderCounter_Read() <= 1500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -1500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() <= 1500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	}

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -1000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	}
}

void vertical_parking() // 수직 주차 
{	
	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() >= -8500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	} // 90도 회전  

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() >= -3000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(-50);
	} // 후진 

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() <= 3000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	} // 전진

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(2000);
	
	while(EncoderCounter_Read() <= 8500)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	} // 90도 회전

	EncoderCounter_Write(0);
	
	SteeringServoControl_Write(1496);
	
	while(EncoderCounter_Read() <= 3000)
	{
		printf("EncoderCounter_Read() : %d", EncoderCounter_Read());
		DesireSpeed_Write(50);
	} // 전진
}