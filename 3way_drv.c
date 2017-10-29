void Find_Centerfor3way(IplImage* imgResult)		//TY add 6.27
{
		#define APPROXIMATE_DISTANCE 280 // approximately 10cm = 140px from leftside of the camera if increased car go more straight
		
		int i, j, k; //for loop
		int width = 320;//data of the input image
		int height = 240;
		
		int x = 0;//for cycle
		int y = 0;
		
		int cutdown = 30;//length of y pixel which display bumper 
		int weight = 500/APPROXIMATE_DISTANCE;
		
		int centerpixel = 0;
		
		int leftpixel = 0;//linepixel
		int rightpixel = 0;
		
		int  finl = 0;//whether line of leftside detected
		int finr = 0;
		
		int centerofpixel = 0;//centerlinepixel
		

		for (y = height-1-cutdown; y >=0; y-=2){//cut down bumper pixel 
			if(finl||finr)break;
			for (x = width / 2 -1; x>=4; x--){//left side
				if (imgResult->imageData[y * width + x] == whitepx){//Search pixels
					for(i=0; i<5; i++){ //check successive 5 white pixels
						if(imgResult->imageData[y * width + x - i] != whitepx)break;
						if(i==4){
							leftpixel = x;
							finl = 1;
						}		
					}
					x = x - i;
					if(finl==1)break;
				}
			}
			
			for (x = width/2; x<width - 4; x++){
				if (imgResult->imageData[y * width + x] == whitepx){//Search pixels
					for(i=0; i<5; i++){ //check successive 5 white pixels
						if(imgResult->imageData[y * width + x + i] != whitepx)break;
						if(i==4){
							rightpixel = x;
							finr = 1;
						}			
					}
					x = x + i;
					if(finl==1)break;
				}
			}
		}
	

		if(!(finl||finr)) //?? Ž?? x???? 
			angle = 1500;
		
		printf("Left_line = %d\n", leftpixel);
		printf("Right_line = %d\n", rightpixel);
		printf("Centerofpixle = %d\n",centerofpixel);
		centerpixel = finl&&finr? (rightpixel+leftpixel)/2:(finl==0?rightpixel-distance:leftpixel+distance);
		
		if(centerpixel-160<10&&centerpixel-160>-10)angle = 1500;//일정값 이상 차이가 없다면 직진
		else
		angle = 1500 - weight*(centerpixel-160);
		
		SteeringServoControl_Write(angle);//motor control 
}