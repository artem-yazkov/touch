#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>

#define pi 3.1415
#define childNum 12   /* ���������� �������� ���� � �������� */

/* ��� ������� �� ����� � ������, ������� � ���� */
/* ��������� ����� �� ��������� �����            */
void calcXY(int cx, int cy, int rd, int ang, int *wx, int *wy)
 {
  *wx=cx+(int) (cos((float)ang*pi/180.0)*(float)rd);
  *wy=cy-(int) (sin((float)ang*pi/180.0)*(float)rd);
 };

int main(int argc, char **argv)
 {
  Display *ourDisplay;
  int ourScreen;
  Window rootWindow;
  Window  myWindow1, myWindow2, myWindow3; /* ������� ���� �������� ������ */
  Window childWindows[childNum];           /* ������ �������� ����         */
  int myDepth;
  XSetWindowAttributes myAttr;
  Visual *myVisual;  
  XSizeHints mySizeHints;
  XClassHint myClassHint;
  int i,j,wx,wy;
  
  ourDisplay=XOpenDisplay(NULL);
  if (ourDisplay==NULL)
    {
      printf("�� ������� ���������� ���������� � ����������� ����������.\n");
      return 1;
    };

  /* ������� ��������������� �������� */
  ourScreen=DefaultScreen(ourDisplay);           /* ����� ��-���������        */
  rootWindow=RootWindow(ourDisplay, ourScreen);  /* �������� ����             */
  myDepth=DefaultDepth(ourDisplay, ourScreen);   /* ������� ��������� ������  */
  myVisual=DefaultVisual(ourDisplay, ourScreen); /* ���������� �������������� */

  /* �������� ��� ��� ���� �������� ������ */
  myAttr.background_pixel=WhitePixel(ourDisplay, ourScreen);  /* ����� ���� ������ */

  /* ������� ��� ���� */
  myWindow1=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
  myWindow2=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
  myWindow3=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
    
  /* ������������� ��������� ���� */
  XStoreName(ourDisplay,myWindow1,"Example window 1");
  XStoreName(ourDisplay,myWindow2,"Example window 2");
  XStoreName(ourDisplay,myWindow3,"Example window 3");

  /* ������������� ��������� ������ */
  XSetIconName(ourDisplay,myWindow1,"example1");
  XSetIconName(ourDisplay,myWindow2,"example2");
  XSetIconName(ourDisplay,myWindow3,"example3");

  /* ������������� ����������� �� ������� ���� */
  mySizeHints.flags=PMinSize | PMaxSize | PResizeInc;
  mySizeHints.min_width=192; mySizeHints.min_height=128;
  mySizeHints.max_width=640; mySizeHints.max_height=480;
  mySizeHints.width_inc=10; mySizeHints.height_inc=10;
  XSetWMNormalHints(ourDisplay, myWindow1, &mySizeHints);
  XSetWMNormalHints(ourDisplay, myWindow2, &mySizeHints);
  XSetWMNormalHints(ourDisplay, myWindow3, &mySizeHints);

  /* ������ ���� �������� */
  XMapWindow(ourDisplay, myWindow1);
  XMapWindow(ourDisplay, myWindow2);
  XMapWindow(ourDisplay, myWindow3);

  XFlush(ourDisplay);
  sleep(3);

  /* ���� ����� ��������� � ������ �����������                */
  /* ����������� ���� ���, ����� ��� ���������� ���� �� ����� */
  XMoveWindow(ourDisplay, myWindow1,0,100);
  XMoveWindow(ourDisplay, myWindow2,100,50);
  XMoveWindow(ourDisplay, myWindow3,50,0);

  XFlush(ourDisplay);
  sleep(3);
  
  /* �������� ������������� ���� ��������� �������  */
  /* ������ ���� - ������ ���� ���������            */
  /* ������ ���� - ���������                        */
  /* ������ ���� - � ����� ����                     */
  XRaiseWindow(ourDisplay, myWindow2);
  XFlush(ourDisplay);
  sleep(1);
  XRaiseWindow(ourDisplay, myWindow1);
  XFlush(ourDisplay);
  sleep(3);

  /* ������ �������� ������ ������� ����    */
  for (i=1; i<200; i++)
   {
    XResizeWindow(ourDisplay,myWindow2,320+i,200+i);
    XFlush(ourDisplay);
    usleep(40);
   };

  /* �������� ������� ���� ����� �������� ����, ������������� �� ����� */
  for (i=0; i<childNum; i++)
   {

    /* ��������� ���������� ��������� ���� */
    calcXY(260,200,180,i*(360/childNum),&wx,&wy);

    /* ��� ���� ����� ޣ���� */
    myAttr.background_pixel=BlackPixel(ourDisplay, ourScreen);

    /* �������� ���� ���������� ������ � ��������� ����������� */
    childWindows[i]=XCreateWindow(ourDisplay,myWindow2,wx-20,wy-10,40, 20,
                 0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
   };

  /* �������� �������� ���� */
  XMapSubwindows(ourDisplay,myWindow2);

  XFlush(ourDisplay);
  sleep(1);
  
  /* ���������� ������� �� �������� ���� �� 3 ����� */
  for (j=1080; j>0; )
   {
    for (i=0; i<childNum; i++)
     {
      calcXY(260,200,180,i*(360/childNum)+j,&wx,&wy);
      XMoveWindow(ourDisplay,childWindows[i],wx-20,wy-10);
     };
    XFlush(ourDisplay);
    usleep(20);
    j-=2;
   };

  sleep(5);

  /* ���������� ���� */
  XDestroyWindow(ourDisplay, myWindow1);
  XFlush(ourDisplay);
  sleep(1);
  XDestroyWindow(ourDisplay, myWindow3);
  XFlush(ourDisplay);
  sleep(1);
  XDestroyWindow(ourDisplay, myWindow2);

  /* ��������� ���������� � �������� */
  XCloseDisplay(ourDisplay);

  return 0;
 };
