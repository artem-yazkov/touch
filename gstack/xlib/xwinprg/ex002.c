#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main(int argc, char **argv)
 {
  Display *ourDisplay;
  int ourScreen;
  Window  myWindow, rootWindow, childWindow;
  unsigned long bgcolor;
  int myDepth;
  XSetWindowAttributes myAttr;
  Visual *myVisual;  
  XSizeHints mySizeHints;
  XClassHint myClassHint;
  char *myPropertyData;
  char *myClassName="example";
  char *myResName="example";
  char *iconName;
  
  ourDisplay=XOpenDisplay(NULL);
  if (ourDisplay==NULL)
    {
      printf("Не удалось установить соединение с графическим терминалом.\n");
      return 1;
    };

  /* Получим предварительные сведения */
  ourScreen=DefaultScreen(ourDisplay);           /* Экран по-умолчанию        */
  rootWindow=RootWindow(ourDisplay, ourScreen);  /* Корневое окно             */
  bgcolor=WhitePixel(ourDisplay, ourScreen);     /* Белый цвет экрана         */
  myDepth=DefaultDepth(ourDisplay, ourScreen);   /* Глубина цветности экрана  */
  myVisual=DefaultVisual(ourDisplay, ourScreen); /* Визуальные характеристики */

  /* Заполняем атрибуты окна */
  myAttr.background_pixel=bgcolor;

  /* Создаем окно */
  myWindow=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
    
  /* Устанавливаем заголовок окна */
  XStoreName(ourDisplay,myWindow,"Example window");

  /* Устанавливаем заголовок иконки */
  XSetIconName(ourDisplay,myWindow,"example");

  /* Устанавливаем ограничения на размеры окна */
  mySizeHints.flags=PMinSize | PMaxSize | PResizeInc;
  mySizeHints.min_width=192; mySizeHints.min_height=128;
  mySizeHints.max_width=640; mySizeHints.max_height=480;
  mySizeHints.width_inc=10; mySizeHints.height_inc=10;
  XSetWMNormalHints(ourDisplay, myWindow, &mySizeHints);

  /* Устанавливаем атрибут командной строки */
  XSetCommand(ourDisplay, myWindow, argv, argc);

  /* Устанавливаем атрибут класса */
  myClassHint.res_name=myResName;
  myClassHint.res_class=myClassName;
  XSetClassHint(ourDisplay,myWindow,&myClassHint);

  /* Создаём дочерние окна */
  myAttr.background_pixel=BlackPixel(ourDisplay, ourScreen);
  childWindow=XCreateWindow(ourDisplay,myWindow,10, 10, 40, 30,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);

  myAttr.win_gravity=NorthEastGravity;
  childWindow=XCreateWindow(ourDisplay,myWindow,270, 10, 40, 30,
    0, myDepth, InputOutput, myVisual, CWBackPixel | CWWinGravity, &myAttr);

  myAttr.win_gravity=SouthWestGravity;
  childWindow=XCreateWindow(ourDisplay,myWindow,10, 160, 40, 30,
    0, myDepth, InputOutput, myVisual, CWBackPixel | CWWinGravity, &myAttr);

  myAttr.win_gravity=SouthEastGravity;
  childWindow=XCreateWindow(ourDisplay,myWindow,270, 160, 40, 30,
    0, myDepth, InputOutput, myVisual, CWBackPixel | CWWinGravity, &myAttr);

  myAttr.win_gravity=CenterGravity;
  childWindow=XCreateWindow(ourDisplay,myWindow,140, 85, 40, 30,
    0, myDepth, InputOutput, myVisual, CWBackPixel | CWWinGravity, &myAttr);

  /* Делаем окна видимыми */
  XMapWindow(ourDisplay, myWindow);
  XMapSubwindows(ourDisplay, myWindow);
  
  /* Все сформированные команды принудительно сбрасываем на сервер */
  XFlush(ourDisplay);

  /* Спим 50 секунд */
  sleep(10);
  
  /* Уничтожаем окно */
  XDestroyWindow(ourDisplay, myWindow);

  /* Закрываем соединение с сервером */
  XCloseDisplay(ourDisplay);

  return 0;
 };
