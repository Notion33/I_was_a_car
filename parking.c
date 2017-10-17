#include <stdio.h>
#include <stdlib.h>
#include "car_lib.h"

void main()
{
	CarControlInit();
	PositionControlOnOff_Write(UNCONTROL);
	SpeedControlOnOff_Write(CONTROL);

	SteeringServoControl_Write(1496);
	DesireSpeed_Write(0);
}