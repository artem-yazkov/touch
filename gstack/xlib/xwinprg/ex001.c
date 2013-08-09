#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

int main()
 {
  Display *ourDisplay;
  int ourScreen;
  Window  myWindow, rootWindow;
  unsigned long bgcolor;
  
  ourDisplay=XOpenDisplay(NULL);
  if (ourDisplay==NULL)
    {
      printf("�� ������� ���������� ���������� � ����������� ����������.\n");
      return 1;
    };

  /* ������� ��������������� �������� */
  ourScreen=DefaultScreen(ourDisplay);          // ����� ��-���������
  rootWindow=RootWindow(ourDisplay, ourScreen); // �������� ����
  bgcolor=WhitePixel(ourDisplay, ourScreen);    // ����� ���� ������

  /* ��������� ���� */
  myWindow=XCreateSimpleWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, 0, bgcolor);
    
  /* ������ ���� ������� */
  XMapWindow(ourDisplay, myWindow);
  
  /* ��� �������������� ������� ������������� ���������� �� ������ */
  XFlush(ourDisplay);

  /* ���� 5 ������ */
  sleep(5);
  
  /* ���������� ���� */
  XDestroyWindow(ourDisplay, myWindow);

  /* ��������� ���������� � �������� */
  XCloseDisplay(ourDisplay);

  return 0;
 };
