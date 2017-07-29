// gcc `pkg-config opencv --cflags` test.c  -o test `pkg-config opencv --libs`
// source : http://carstart.tistory.com/188
// compile : http://www.ozbotz.org/opencv-installation/

// ./test t.png

#include <highgui.h>
#include <cv.h>

int main( int argc, char** argv ) 
{
    IplImage* img = cvLoadImage(argv[1], 1);
    IplImage* cannyImage = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
    
    if(img == NULL)
        return;
    
    cvCanny(img, cannyImage, 100, 100, 3);

    cvSaveImage("cannyResult.png" , cannyImage, 0);
    cvReleaseImage(&img);
    return 0;
}

