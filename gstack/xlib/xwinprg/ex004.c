#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>

#define pi 3.1415
#define childNum 16   /* ���������� �������� ���� � �������� */

/* ��� ������� �� ����� � ������, ������� � ���� */
/* ��������� ����� �� ��������� �����            */
void calcXY(int cx, int cy, int rd, int ang, int *wx, int *wy)
 {
  *wx=cx+(int) (cos((float)ang*pi/180.0)*(float)rd);
  *wy=cy-(int) (sin((float)ang*pi/180.0)*(float)rd);
 };

/* ������������ ������ ������ ���������� ����� */
void listVisuals(XVisualInfo *firstVisual, int nitems)
 {
   int i,j;
   
   if (firstVisual==NULL)
     {
      printf("���������� ���� �� �������.\n");
     }
    else
     {
      printf("������� ���������� �����: %d.\n",nitems);
      for(i=0; i<nitems; i++)
       {
        printf("%d. ������� ���������: %d, �����: ",i,firstVisual[i].depth);
        switch (firstVisual[i].c_class)
	 {
	  case StaticGray: printf("StaticGray"); break;
	  case StaticColor: printf("StaticColor"); break;
	  case TrueColor: printf("TrueColor"); break;
	  case GrayScale: printf("GrayScale"); break;
	  case PseudoColor: printf("PseudoColor"); break;
	  case DirectColor: printf("DirectColor"); break;
          default: printf("����������"); break;
	 };
        printf(".\n");
       };      
     };
 };

int main(int argc, char **argv)
 {
  Display *ourDisplay;
  int ourScreen;
  Colormap myColormap;
  int myDepth;
  Window rootWindow;
  Window  myWindow;                    /* ������� ���� �������� ������ */
  Window childWindows[childNum];       /* ������ �������� ����         */
  unsigned long childColors[childNum]; /* ����� ������� ������         */
  XColor theColor;
  XSetWindowAttributes myAttr;
  Visual *myVisual;
  XVisualInfo *listOfVisuals;  
  XVisualInfo templateVisual;
  XSizeHints mySizeHints;
  XClassHint myClassHint;
  int numberOfVisuals;
  int i,j,wx,wy;
  char *colorNames[]={"black","dark blue","dark red","dark magenta",
                      "dark green","dark cyan","brown","dim gray",
		      "gray","blue","red","magenta","green","cyan",
		      "yellow","white"};
  
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
  myColormap=DefaultColormap(ourDisplay, ourScreen); /* �������� ������� */

  /* ������� ������ ���� ���������� ����� */
  listOfVisuals=XGetVisualInfo(ourDisplay, VisualNoMask, &templateVisual,
   &numberOfVisuals);
  listVisuals(listOfVisuals,numberOfVisuals);
  if (listOfVisuals!=NULL)
    XFree(listOfVisuals);
  
  /* �������� ��� ��� ���� �������� ������ */
  myAttr.background_pixel=WhitePixel(ourDisplay, ourScreen);  /* ����� ���� ������ */

  /* ������� ���� �������� ������ */
  myWindow=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 300,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
    
  /* ������������� ��������� ���� */
  XStoreName(ourDisplay,myWindow,"Color example window");

  /* ������������� ��������� ������ */
  XSetIconName(ourDisplay,myWindow,"example");

  /* ������������� ����������� �� ������� ���� */
  mySizeHints.flags=PMinSize | PMaxSize;
  mySizeHints.min_width=320; mySizeHints.min_height=300;
  mySizeHints.max_width=320; mySizeHints.max_height=300;
  XSetWMNormalHints(ourDisplay, myWindow, &mySizeHints);

  /* ������ ���� �������� */
  XMapWindow(ourDisplay, myWindow);

  /* ������� ���� ������� ��� �������� ���� */
  myAttr.border_pixel=BlackPixel(ourDisplay, ourScreen);

  /* �������� ������� ���� ����� �������� ����, ������������� �� ����� */
  for (i=0; i<childNum; i++)
   {
    /* ��������� ���������� ��������� ���� */
    calcXY(160,150,110,i*(320/childNum),&wx,&wy);

    /* �������� ���� ��� ���� */
    if (XAllocNamedColor(ourDisplay,myColormap,colorNames[i&15],
                             &theColor,&theColor)!=0)
      {
       childColors[i]=theColor.pixel;
      }
     else
      {
       /* �������� ������� �� ��������� */
       printf("�� ������� �������� ����� ��� �������� ����.\n");
       if (i>0)
        XFreeColors(ourDisplay,myColormap,childColors,i,0);
       XDestroyWindow(ourDisplay, myWindow);
       XCloseDisplay(ourDisplay);
       return 2;
      };

    myAttr.background_pixel=childColors[i];
    childWindows[i]=XCreateWindow(ourDisplay,myWindow,wx-40,wy-30,80, 60,
                 2, myDepth, InputOutput, myVisual,
		 CWBackPixel | CWBorderPixel, &myAttr);
   };

  /* �������� �������� ���� */
  XMapSubwindows(ourDisplay,myWindow);

  XFlush(ourDisplay);
  sleep(1);
  
  /* ���������� ������� �� �������� ���� �� 3 ����� */
  for (j=1080; j>0; )
   {
    for (i=0; i<childNum; i++)
     {
      calcXY(160,150,110,i*(320/childNum)+j,&wx,&wy);
      XMoveWindow(ourDisplay,childWindows[i],wx-40,wy-30);
     };
    XFlush(ourDisplay);
    usleep(20);
    j-=2;
   };

  XFlush(ourDisplay);
  sleep(1);

  /* ��������� ���� � �������� ������� */
  XRestackWindows(ourDisplay,childWindows,childNum);  

  XFlush(ourDisplay);
  sleep(2);

  /* ������� �������� � ������ �������... */
  for (j=0; j<1080; )
   {
    for (i=0; i<childNum; i++)
     {
      calcXY(160,150,110,i*(320/childNum)+j,&wx,&wy);
      XMoveWindow(ourDisplay,childWindows[i],wx-40,wy-30);
     };
    XFlush(ourDisplay);
    usleep(20);
    j+=2;
   };

  /* ����������� ���������� ����� */
  XFreeColors(ourDisplay, myColormap,childColors,childNum,0);

  /* ���������� ���� */
  XDestroyWindow(ourDisplay, myWindow);

  /* ��������� ���������� � �������� */
  XCloseDisplay(ourDisplay);

  return 0;
 };
