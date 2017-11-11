int detect_hwan(){
	int width = 280, height = 80;
    int mThreshold = width*height*0.4;
	NvMediaTime pt1 = { 0 }, pt2 = { 0 };
	NvU64 ptime1, ptime2;
	struct timespec;

    int i, j, k;
    
    int left_obj=0;
    int right_obj=0;
    int center_obj=0;

    int countwhite = 0;
    int countblack =0;
    
    int blackseries = 0;
    int originname[40];
    int resultname[40];

    int blackloc=0;
    char obsloc;

    unsigned char status;
    short tw_speed;
    unsigned char gain;
    int position, position_now;
    int channel;
    int data=0;
    char sensor;
    int tol;
    char byte = 0x80;
    int flag =0;
    int escape=0; 
    CarControlInit();
    
    CvPoint startpoint, endpoint, scanbound,centstart,centend;
   
	centstart.x=106;
	centstart.y=122;	
	centend.x=213;
	centend.y=160;

    startpoint.x = 1; //(136,120)  (252,149) // start ROI
    startpoint.y = 122;
 
    endpoint.x = 318; // end ROI
    endpoint.y = 160;

    scanbound.x = 1;
    scanbound.y = 1; // initialize

 
    IplImage* imgOrigin;
	IplImage* imgResult; 

    imgOrigin = cvCreateImage(cvSize(RESIZE_WIDTH, RESIZE_HEIGHT), IPL_DEPTH_8U, 3);
	imgResult = cvCreateImage(cvGetSize(imgOrigin), IPL_DEPTH_8U, 1);           // TY add 6.27

	cvZero(imgResult);          // TY add 6.27

	CameraYServoControl_Write(1600); 	//camera up
    sleep(1);

		/////////////////////////////
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&cond, &mutex);

	GetTime(&pt1);
	ptime1 = (NvU64)pt1.tv_sec * 1000000000LL + (NvU64)pt1.tv_nsec;

	Frame2Ipl(imgOrigin, imgResult, color);

	pthread_mutex_unlock(&mutex);
	////////////////////////////////

	for(i = 0;i<200;i++){
		for(j=0; j<320; j++){
			if(imgOrigin->imageData[(i*320+j)*3]>167 && imgOrigin->imageData[(i*320+j)*3+1]>161){
				imgResult->imageData[i*320+j] = 255;
		//		new_white_count ++;	//white pixel in right
			}
			else if(imgOrigin->imageData[(i*320+j)*3]>22 && imgOrigin->imageData[(i*320+j)*3]<164); //black default
			else imgResult->imageData[i*320+j] = 127;
		}
	}

///////////////////알골
    for(i = 50;i<200;i++){
        for(j=0; j<106; j++){
            int px = imgResult->imageData[i + j*imgResult->widthStep];
            if(px <130 && px>120){
                left_obj++;
            }
        }

        for(j=106; j<213; j++){
            int px = imgResult->imageData[i + j*imgResult->widthStep];
            if(px <130 && px>120){
                center_obj++;
            }
        }
        for(j=213; j<320; j++){
            int px = imgResult->imageData[i + j*imgResult->widthStep];
            if(px <130 && px>120){
                right_obj++;
            }
        }
    }

/*
    for (j = endpoint.y; j > startpoint.y; j--) {
        for (i = endpoint.x; i > startpoint.x; i--) {
            int px = Binaryimg->imageData[i + j*Binaryimg->widthStep];
           
            if (px == blackpx) {
             //   countblack++;//검정색 만나면 스타트! 연속해서 15px 있는지 확인
                for (k = i; k > i- 15; k--) {
                    int px2 = Binaryimg->imageData[k + j*Binaryimg->widthStep];
                    if (px2 == blackpx) blackseries++;
                    else break;
                }
                if (blackseries == 15) {
                    blackloc = i - 7;   
                    break;
                }
                else blackseries = 0;
            }
            
        }
        if (blackseries == 15) break;
    }
    scanbound.x = k;
    scanbound.y = j;*/
    cvRectangle(imgResult, centstart, centend, CV_RGB(255,255,255), 1, 8, 0);
    cvRectangle(imgResult, startpoint, endpoint, CV_RGB(255,255,255), 1, 8, 0); // ROI boundary
  //  cvLine(Binaryimg, scanbound, endpoint, CV_RGB(255,255,0), 1, 8, 0);    //scan boundary
   
    // printf("countblack is %d \n",countblack);
    // printf("countwhite is %d \n",countwhite);
    // printf("black center is %d and blackseries is on %d \n",blackloc,blackseries);	
    sprintf(originname, "imgsaved/origin.png");
    sprintf(resultname, "imgsaved/result.png");
    
    cvSaveImage(originname, imgOrigin, 0);     
    cvSaveImage(resultname, imgResult, 0);    

    printf("\n left_obj = %d, center_obj= %d, right_obj= %d \n",left_obj,center_obj,right_obj);
  
    if(left_obj>right_obj){
    	printf("\n left_obj>right_obj \n");
    	return 1;
    }

    if(left_obj<right_obj){
	   	printf("\n left_obj<right_obj \n");

    	return -1;
    } 
   
}