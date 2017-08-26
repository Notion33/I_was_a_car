#include "stdio.h"
#include <cv.h>
#include <highgui.h>
#include "stdbool.h"

void onTrack(int pos){

}

int main(int argc, char const *argv[]) {

  printf("Start Trackbar Main\n");

  IplImage* img = cvLoadImage(argv[1], -1);

  if(img==0){
    printf("No image to read!\n");
    return;
  }

  IplImage* imgResult = cvCreateImage(cvGetSize(img),IPL_DEPTH_8U,1);


  cvNamedWindow("yuv", 1);
  //cvShowImage("yuv", img);

  int low_y, high_y, low_u, high_u, low_v, high_v;
  cvCreateTrackbar("low_y", "yuv", &low_y, 255, onTrack);
	cvCreateTrackbar("low_u", "yuv", &low_u, 255, onTrack);
	cvCreateTrackbar("low_v", "yuv", &low_v, 255, onTrack);
	cvCreateTrackbar("high_y", "yuv", &high_y, 255, onTrack);
	cvCreateTrackbar("high_u", "yuv", &high_u, 255, onTrack);
	cvCreateTrackbar("high_v", "yuv", &high_v, 255, onTrack);

  cvSetTrackbarPos("low_y","yuv",0);
  cvSetTrackbarPos("low_u","yuv",0);
  cvSetTrackbarPos("low_v","yuv",0);
  cvSetTrackbarPos("high_y","yuv",255);
  cvSetTrackbarPos("high_u","yuv",255);
  cvSetTrackbarPos("high_v","yuv",255);

  while(1){
    low_y = cvGetTrackbarPos("low_y", "yuv");
		low_u = cvGetTrackbarPos("low_u", "yuv");
		low_v = cvGetTrackbarPos("low_v", "yuv");
		high_y = cvGetTrackbarPos("high_y", "yuv");
		high_u = cvGetTrackbarPos("high_u", "yuv");
		high_v = cvGetTrackbarPos("high_v", "yuv");
		cvInRangeS(img, cvScalar(low_y, low_u, low_v, 0), cvScalar(high_y, high_u, high_v, 0), imgResult);

		cvShowImage("yuv", imgResult);
		int key = cvWaitKey(1);
		if (key == 'q')break;
  }


  cvWaitKey(0);
	cvDestroyWindow("yuv");
	cvReleaseImage(&img);

  return 0;
}
