# yuvTrackbar 사용법

----

1. `./yuvTrackbar <이미지파일.확장자>`를 입력한다.
2. low는 최소이고 high는 최대값이니 Threshold범위를 잘 조절해서 자신이 원하는 값이 나오도록 조절한다.
3. 구한 값을 토대로 `Frame2Ipl`함수의 Threshold를 조절한다.

----

- 소스를 수정해서 사용하고 싶은 경우 컴파일은 opencv가 설치된 리눅스 환경에서 아래와 같다.
~~~
gcc `pkg-config opencv --cflags` yuvTrackbar.c -o yuvTrackbar `pkg-config opencv --libs`
~~~
