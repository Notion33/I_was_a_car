// gcc main.c -o main

#include <stdio.h>
#include <stdlib.h>
#include "car_lib.h" //

#define SPEED_CONTROL     // to test speed control
#define POSITION_CONTROL  // to test postion control
#define SERVO_CONTROL     // to test servo control(steering & camera position)
//#define DISTANCE_SENSOR     // to test distance sensor

void main(void)
{
    unsigned char status;
    short speed;
    unsigned char gain;
    int position, position_now;
    short camangle,angle;
    int channel;
    int data=0;
    char sensor;
    int i, j;
    int tol;
    char byte = 0x80;
    int flag =0;
    int escape=0; 
    CarControlInit();

#ifdef SERVO_CONTROL
    /////////SERVO_CONTROL//////////
    printf("\n\n 3. servo control\n");
    angle = 1500;
    SteeringServoControl_Write(angle);
    Alarm_Write(ON);
    usleep(100000);
    Alarm_Write(OFF);
    sleep(2);

    //camera y servo set

    camangle = 1800;                       // Range : 1200(Up)~1500(default)~1800(Down)
    CameraYServoControl_Write(camangle);
    sleep(1);

    camangle = 1600;
    CameraYServoControl_Write(camangle);
    sleep(1);
    
    Alarm_Write(ON);
    usleep(100000);
    Alarm_Write(OFF);
    sleep(2); 
#endif


#ifdef POSITION_CONTROL
     // 1. position control -------------------------------------------------------
    printf("\n\n 1. position control\n");

    //jobs to be done beforehand;
    SpeedControlOnOff_Write(CONTROL);   // speed controller must be also ON !!!
    
    speed = 100; // speed set     --> speed must be set when using position controller
    DesireSpeed_Write(speed);

    //control on/off
    status = PositionControlOnOff_Read();
    printf("PositionControlOnOff_Read() = %d\n", status);
    PositionControlOnOff_Write(CONTROL);

    //position controller gain set
    gain = PositionProportionPoint_Read();    // default value = 10, range : 1~50
    printf("PositionProportionPoint_Read() = %d\n", gain);
    gain = 20;
    PositionProportionPoint_Write(gain);
            
    //position write
    position_now = 0;  //initialize //0 default
    EncoderCounter_Write(position_now);
    
    //position set
    position=DesireEncoderCount_Read();
    printf("DesireEncoderCount_Read() = %d\n", position);
    position = 500;
    DesireEncoderCount_Write(position);

    position=DesireEncoderCount_Read();
    printf("DesireEncoderCount_Read() = %d\n", position);
    
    tol = 100;    // tolerance
    while(abs(position_now-position)>tol)
    {
        position_now=EncoderCounter_Read();
        printf("EncoderCounter_Read() = %d\n", position_now);
    }
    sleep(1);
#endif

#ifdef SPEED_CONTROL
    // 2. speed control ----------------------------------------------------------
    printf("\n\n 2. speed control\n");

    //jobs to be done beforehand;
    PositionControlOnOff_Write(UNCONTROL); // position controller must be OFF !!!

    //control on/off
    status=SpeedControlOnOff_Read();
    printf("SpeedControlOnOff_Read() = %d\n", status);
    SpeedControlOnOff_Write(CONTROL);

    //speed controller gain set
    //P-gain
    gain = SpeedPIDProportional_Read();        // default value = 10, range : 1~50
    printf("SpeedPIDProportional_Read() = %d \n", gain);
    gain = 20;
    SpeedPIDProportional_Write(gain);

    //I-gain
    gain = SpeedPIDIntegral_Read();        // default value = 10, range : 1~50
    printf("SpeedPIDIntegral_Read() = %d \n", gain);
    gain = 20;
    SpeedPIDIntegral_Write(gain);
    
    //D-gain
    gain = SpeedPIDDifferential_Read();        // default value = 10, range : 1~50
    printf("SpeedPIDDefferential_Read() = %d \n", gain);
    gain = 20;
    SpeedPIDDifferential_Write(gain);

    //speed set    
    speed = DesireSpeed_Read();
    printf("DesireSpeed_Read() = %d \n", speed);
    speed = 80;
    DesireSpeed_Write(speed);
 
    sleep(1);  //run time 

    SteeringServoControl_Write(1200);

    speed = 100;
    DesireSpeed_Write(speed);
    sleep(2);

    SteeringServoControl_Write(1800);
    sleep(2);  //run time 

    SteeringServoControl_Write(1500);

    while(flag<2){
        printf("flag = %d",flag);
        while(escape<2){
            channel =6;
            data = DistanceSensor(channel);
            printf("channel = %d, distance = 0x%04X(%d) \n", channel, data, data);
            usleep(100000);
            if(data>1500)escape++;
        }

        flag ++;
        Alarm_Write(ON);
        usleep(100000);
        Alarm_Write(OFF);

        while(flag<2){ 
            printf("flag = %d",flag);
            channel =5;
            data = DistanceSensor(channel);
            printf("channel = %d, distance = 0x%04X(%d) \n", channel, data, data);
            usleep(100000);
            if(data>1500) flag++;
        }
    }
    Alarm_Write(ON);
    usleep(100000);
    Alarm_Write(OFF);

    SteeringServoControl_Write(1800);
    sleep(2);  //run time 

    SteeringServoControl_Write(1200);
    sleep(2);

    SteeringServoControl_Write(1500);
    sleep(2);
   
   #endif

    speed = DesireSpeed_Read();
    printf("DesireSpeed_Read() = %d \n", speed);

    speed = 0;
    DesireSpeed_Write(speed);

    
    sleep(1);


    
}



