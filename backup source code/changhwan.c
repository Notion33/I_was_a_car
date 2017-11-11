void changhwan(){

	char fileName[40];
	char fileName1[40];         // TY add 6.27
	char fileName_color[40];         // NYC add 8.25
	//char fileName2[30];           // TY add 6.27
	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;
	int num =0;
	int i = 0;
	int j = 0;
	int data= 0;
	int channel =1;
	int cc=0;
	
	int new_white_count=0;
	int	left_white_count = 0;
	int right_white_count = 0;
	
	bool center_of_3way =false;
	bool middle_of_3way =false;
	int detect_object =0;

	IplImage* imgOrigin;
	IplImage* imgResult;            // TY add 6.27

	// cvCreateImage
	imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
	imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27

	cvZero(imgResult);          // TY add 6.27

	while (1)
	{
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);

		GetTime(&pt1);
		ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

		Frame2Ipl(imgOrigin, imgResult, color);

		pthread_mutex_unlock(&mutex);

		new_white_count=0;
		left_white_count=0;
		right_white_count = 0;

		if(center_of_3way == false){ // 차량이 아직 흰석 중앙에 위치 하지 않음. 더 조향해야함
		
			if(middle_of_3way == false){ // middle of 3_way == false , 중앙보다 덜 갔을때 계속 조향
	//			printf("\n middle_of_3way == ///false/// \n");
				printf("new_white_count = %d",new_white_count);
				printf("center of 3way is %s // middle_of_3way is %s \n",center_of_3way ? "true" : "false", middle_of_3way ? "true" : "false");


				for(i = 50;i<200;i++){
					for(j=160; j<320; j++){
						if(imgOrigin->imageData[(i*320+j)*3]>200 && imgOrigin->imageData[(i*320+j)*3+1]>100){
							imgResult->imageData[i*320+j] = 255;
							new_white_count ++;	//white pixel in right
						}
						else if(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164); //black default
						else imgResult->imageData[i*320+j] = 127;
					}
				}

				if(new_white_count>1200){ 
					middle_of_3way=true;
					SteeringServoControl_Write(2000);
					DesireSpeed_Write(80);
				
				printf("new_white_count = %d",new_white_count);
				printf("center of 3way is %s // middle_of_3way is %s \n",center_of_3way ? "true" : "false", middle_of_3way ? "true" : "false");

				}

				else {
					SteeringServoControl_Write(2000);
					DesireSpeed_Write(80);
					
				}
			}
			else { //middle of 3way == true
				printf("\n //middle//_of_3way == ///true/// \n");

				for(i = 50;i<200;i++){
					for(j = 0;j<160;j++){
						if(imgOrigin->imageData[(i*320+j)*3]>200 && imgOrigin->imageData[(i*320+j)*3+1]>100){
							imgResult->imageData[i*320+j] = 255;//white pixel in left
							left_white_count ++;
						}
						else if(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164); //black default
						else imgResult->imageData[i*320+j] = 127;
					}
					for(j=160; j<320; j++){
						if(imgOrigin->imageData[(i*320+j)*3]>200 && imgOrigin->imageData[(i*320+j)*3+1]>100){
							imgResult->imageData[i*320+j] = 255;
							right_white_count ++;	//white pixel in right
						}
						else if(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164); //black default
						else imgResult->imageData[i*320+j] = 127;
					}
				}
				if ((left_white_count>800 && right_white_count>800)||(left_white_count<300 && right_white_count<300)){
					center_of_3way = true;
				}
			}	
		}

		else { //center of 3way == true 이면
				// 흰샌 점선이 차량 중앙을 지나 오른쪽에 치우쳤을때 중앙 기준 흰색 픽셀이 좌우 비슷해질때까지 조향
				printf("\n /////////find center algorithm///////// \n");
				SteeringServoControl_Write(1200);           	
				while( cc <10)
					{
						DesireSpeed_Write(70);
						cc++;
					}
            
			//	Find_Center_dr2(imgResult);
				// data = DistanceSensor(channel);
    //         	printf("channel = %d, distance = 0x%04X(%d) \n", channel, data, data);
    //         	usleep(100000);

            	// if(data>561) detect_object++;
            	// if(detect_object>1){

            		DesireSpeed_Write(0);
            		SteeringServoControl_Write(1500);
            	 	DesireSpeed_Write(-70);
            	 	sleep(1);
  	           		DesireSpeed_Write(0);
  	           		int ndetection = detect_hwan();
  	           		if(ndetection < 0) {
  	           			
            		SteeringServoControl_Write(1200);
            		sleep(1);
            	 	DesireSpeed_Write(80);
            		sleep(1);
            			DesireSpeed_Write(0);
  	           	
  	           		}
  	           		else if(ndetection>0){
  	           		SteeringServoControl_Write(1800);
            		sleep(1);
            		DesireSpeed_Write(80);
            		sleep(1);
            			DesireSpeed_Write(0);
  	      
  	           		}
  	           		printf("\ndetection = %d\n",ndetection);
  	           		while(1){printf("system end\n");
  	           		sleep(1);}
             	// 	Threeway_hardcoding();
		}
				 			
		
				/*	if(픽셀이 좌우가 숫자 각으면){
						SteeringServoControl_Write(2000);
						DesireSpeed_Write(80);
								}
					if(left_white_count > 1000 && right_white_count >1000){
						find center;
						find center >130 && <160
					}*/
		printf("\n new_white_count = %d \n\n",new_white_count);
		printf("\n right_white_count = %d \n\n",right_white_count);
		printf("\n left_white_count = %d \n\n",left_white_count); 
		printf("\n num = %d \n",num);
		sprintf(fileName, "imgsaved/0922_%d.png", num);          // TY add 6.27
		sprintf(fileName1, "imgsaved/origin_%d.png", num);          // TY add 6.27
		
		num++;
   		cvSaveImage(fileName, imgResult, 0); 
   	//	cvSaveImage(fileName1, imgOrigin, 0);  
  
   		         // TY add 6.27	

   		
	}
}