// gcc `pkg-config opencv --cflags` test.c  -o test `pkg-config opencv --libs`
// source : http://carstart.tistory.com/188
// compile : http://www.ozbotz.org/opencv-installation/

// ./test tiger_320x240.jpg tiger_320x240C.jpg

#include <stdio.h>
#include <sys/time.h> 
#include <highgui.h>
#include <cv.h>
#define BASETIME 100000L

void Settimer(void)
{
    struct itimerval t;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    t.it_value.tv_sec = BASETIME; /* 1시간   */
    t.it_value.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &t, NULL) == -1){ perror("Failed to set virtual timer"); }
}

float Gettimer(void)
{
    struct itimerval t;
    float  diftime;
    if (getitimer(ITIMER_REAL, &t) == -1){perror("Failed to get virtual timer");}
    
    diftime=(float)BASETIME-((float)(t.it_value.tv_sec)+((float)t.it_value.tv_usec/1000000.0));
    return(diftime);    
}

int main( int argc, char** argv ) 
{
    float d;
    
    Settimer();              // 시간측정시작
    IplImage* img = cvLoadImage(argv[1], 1);
    //IplImage* img = cvLoadImage("t.png", 1);
    d=Gettimer();            // 위의 계산 소요 시간 
    printf("cvLoadImage    : %4.3f[sec]\n",d);
    
    Settimer();              // 시간측정시작
    IplImage* cannyImage = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
    d=Gettimer();            // 위의 계산 소요 시간 
    printf("cvCreateImage  : %4.3f[sec]\n",d);
    
    if(img == NULL)
        return;
    
    Settimer();              // 시간측정시작
    cvCanny(img, cannyImage, 100, 100, 3);
    d=Gettimer();            // 위의 계산 소요 시간 
    printf("cvCanny        : %4.3f[sec]\n",d);

    Settimer();              // 시간측정시작
    cvSaveImage(argv[2] , cannyImage, 0);
    d=Gettimer();            // 위의 계산 소요 시간 
    printf("cvSaveImage    : %4.3f[sec]\n",d);
    
    Settimer();              // 시간측정시작
    cvReleaseImage(&img);
    d=Gettimer();            // 위의 계산 소요 시간 
    printf("cvReleaseImage : %4.3f[sec]\n",d);
    return 0;
}

