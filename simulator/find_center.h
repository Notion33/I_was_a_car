//==============================================================================
//  Global variable & header file area
#include "stdbool.h"

#define SERVO_CONTROL
#define SPEED_CONTROL
#define IMGSAVE

#define UNCONTROL 0
#define CONTROL 1
#define whitepx -1

bool turn_left_max = false;         //TY add
bool turn_right_max = false;

bool Right_Max = false;
bool Left_Max = false;

//==============================================================================

void Find_Center(IplImage* imgResult)		//TY add 6.27
{
	int width = 320;//40
	int height = 240;//60
	int Left_Down = 0; // 3 사분면
	int Right_Down = 0; // 4 사분면
	int Left_Up = 0; // 2 사분면
	int Right_Up = 0; // 1 사분면

	int x = 0;
	int y = 0;
	//int angle = 0;

	int Dif = 0, Dif1 = 0; // 2사분면 - 1사분면 픽셀수 ; 3사분면 - 4사분면 픽셀수;
	int Left_Sum = 0, Right_Sum = 0; //왼쪽, 오른쪽 픽셀 갯수
	int Gap = 0; // 왼쪽 - 오른쪽 픽셀 갯수;
	int Up = 0, Down = 0;// 1,2 dimension's sum & 3,4 dimension's sum

	float weight = 1.8, weight2 = 1.2;// control angle weight
	//int speed = 0;
	int straight_speed = 115;
	int curve_speed = 85;
	//총 픽셀은 320 *240 = 76800


	for (y = 100; y < height / 2; y++)
	{
		for (x = 80; x < width / 2; x++)
		{
			if (imgResult->imageData[y * width + x] == whitepx)//Find white pixels
			{
				Left_Up++;

			}// 2사분면
		}
		for (x = 240; x > width / 2; x--)
		{
			if (imgResult->imageData[y * width + x] == whitepx)
			{
				Right_Up++;
			}// 1사분면
		}
	}
	printf("Left_Up = %d\n", Left_Up);
	printf("Right_Up = %d\n", Right_Up);
	Dif1 = Left_Up - Right_Up;
	printf("Difference1 is %d\n", Dif1);


	for (y = (height / 2) + 1; y < 160; y++)
	{
		for (x = 80; x < width / 2; x++)
		{
			if (imgResult->imageData[y * width + x] == whitepx)//Find white pixels
			{
				Left_Down++;

			}// 3사분면
		}
		for (x = 240; x > width / 2; x--)
		{
			if (imgResult->imageData[y * width + x] == whitepx)
			{
				Right_Down++;
			}// 4사분면
		}

	}
	printf("Left_Down = %d\n", Left_Down);
	printf("Right_Down = %d\n", Right_Down);

	Dif = Left_Down - Right_Down;
	printf("Difference is %d\n", Dif);


	Left_Sum = Left_Down + Left_Up;
	Right_Sum = Right_Down + Right_Up;
	Gap = Left_Sum - Right_Sum;
	Up = Left_Up + Right_Up;
	Down = Left_Down + Right_Down;

	printf("left_sum=%d, Right_sum=%d, gap=%d\n", Left_Sum, Right_Sum, Gap);



	if (Dif == 0 && Dif1 == 0)
	{
		angle = 1500;
	}

	else if (Gap > 0) // turn right
	{
		if (Dif <= 10 && Dif1 <= 10)
		{
			angle = 1500;
		}

		else if (Right_Up < Left_Up)
		{
			if (Down == 0)
			{
				if (Right_Sum == 0)
				{
					angle = 1250;
				}//only one side pixels
				else
				{
					angle = 1000;
					Right_Max = true;
				}
			}

			else
				angle = 1500 - Gap * weight;
		}

	}// angle < 1500 turn right

	else if (Gap < 0) // turn left
	{
		if (Dif >= -10 && Dif1 >= -10)
		{
			angle = 1500;
		}

		else if (Right_Up > Left_Up)
		{
			if (Down == 0)
			{
				if (Left_Sum == 0)
				{
					angle = 1750;
				}
				else
				{
					angle = 2000;
					Left_Max = true;
				}
			}

			else
				angle = 1500 - Gap * weight;
		}

	}// angle > 1500 turn left

	if (Left_Max || Right_Max == true)
	{
    Left_Up = Left_Sum;
		Right_Up = Right_Sum;

		if (Dif <= 10 && Dif1 <= 10)
		{
			Right_Max = false;
		}
		else if (Dif >= -10 && Dif1 >= -10)
		{
			Left_Max = false;
		}
		else if (Right_Max == true)
		{
			angle = 1000;
		}
		else if (Left_Max == true)
		{
			angle = 2000;
		}
	}

	angle = angle > 2000 ? 2000 : angle < 1000 ? 1000 : angle;

	/*angle = angle > 2000 ? 2000: angle;
	angle = angle < 1000 ? 1000 : angle;*/

	//SteeringServoControl_Write(angle);

#ifdef SPEED_CONTROL
	if (angle<1200 || angle>1800)
		speed = curve_speed;
	else
		speed = straight_speed;
	//DesireSpeed_Write(speed);
#endif

}
