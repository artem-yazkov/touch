#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>

#define pi 3.1415
#define childNum 12   /* Количество дочерних окон в хороводе */

/* Эта функция по точке в центре, радиусу и углу */
/* вычисляет точку на периметре круга            */
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
  Window  myWindow1, myWindow2, myWindow3; /* Рабочие окна верхнего уровня */
  Window childWindows[childNum];           /* Список дочерних окон         */
  int myDepth;
  XSetWindowAttributes myAttr;
  Visual *myVisual;  
  XSizeHints mySizeHints;
  XClassHint myClassHint;
  int i,j,wx,wy;
  
  ourDisplay=XOpenDisplay(NULL);
  if (ourDisplay==NULL)
    {
      printf("Не удалось установить соединение с графическим терминалом.\n");
      return 1;
    };

  /* Получим предварительные сведения */
  ourScreen=DefaultScreen(ourDisplay);           /* Экран по-умолчанию        */
  rootWindow=RootWindow(ourDisplay, ourScreen);  /* Корневое окно             */
  myDepth=DefaultDepth(ourDisplay, ourScreen);   /* Глубина цветности экрана  */
  myVisual=DefaultVisual(ourDisplay, ourScreen); /* Визуальные характеристики */

  /* Выбираем фон для окон верхнего уровня */
  myAttr.background_pixel=WhitePixel(ourDisplay, ourScreen);  /* Белый цвет экрана */

  /* Создаем три окна */
  myWindow1=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
  myWindow2=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
  myWindow3=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
    
  /* Устанавливаем заголовки окон */
  XStoreName(ourDisplay,myWindow1,"Example window 1");
  XStoreName(ourDisplay,myWindow2,"Example window 2");
  XStoreName(ourDisplay,myWindow3,"Example window 3");

  /* Устанавливаем заголовки иконок */
  XSetIconName(ourDisplay,myWindow1,"example1");
  XSetIconName(ourDisplay,myWindow2,"example2");
  XSetIconName(ourDisplay,myWindow3,"example3");

  /* Устанавливаем ограничения на размеры окон */
  mySizeHints.flags=PMinSize | PMaxSize | PResizeInc;
  mySizeHints.min_width=192; mySizeHints.min_height=128;
  mySizeHints.max_width=640; mySizeHints.max_height=480;
  mySizeHints.width_inc=10; mySizeHints.height_inc=10;
  XSetWMNormalHints(ourDisplay, myWindow1, &mySizeHints);
  XSetWMNormalHints(ourDisplay, myWindow2, &mySizeHints);
  XSetWMNormalHints(ourDisplay, myWindow3, &mySizeHints);

  /* Делаем окна видимыми */
  XMapWindow(ourDisplay, myWindow1);
  XMapWindow(ourDisplay, myWindow2);
  XMapWindow(ourDisplay, myWindow3);

  XFlush(ourDisplay);
  sleep(3);

  /* Окна могли появиться в других координатах                */
  /* Передвигаем окна так, чтобы они наложились друг на друга */
  XMoveWindow(ourDisplay, myWindow1,0,100);
  XMoveWindow(ourDisplay, myWindow2,100,50);
  XMoveWindow(ourDisplay, myWindow3,50,0);

  XFlush(ourDisplay);
  sleep(3);
  
  /* Медленно перестраиваем окна следующим образом  */
  /* первое окно - поверх всех остальных            */
  /* второе окно - посредине                        */
  /* третье окно - в самом низу                     */
  XRaiseWindow(ourDisplay, myWindow2);
  XFlush(ourDisplay);
  sleep(1);
  XRaiseWindow(ourDisplay, myWindow1);
  XFlush(ourDisplay);
  sleep(3);

  /* Плавно увеличим размер второго окна    */
  for (i=1; i<200; i++)
   {
    XResizeWindow(ourDisplay,myWindow2,320+i,200+i);
    XFlush(ourDisplay);
    usleep(40);
   };

  /* Создадим второму окну набор дочерних окон, расположенных по кругу */
  for (i=0; i<childNum; i++)
   {

    /* Вычисляем координаты дочернего окна */
    calcXY(260,200,180,i*(360/childNum),&wx,&wy);

    /* Фон окон будет чёрным */
    myAttr.background_pixel=BlackPixel(ourDisplay, ourScreen);

    /* Дочерние окна появляются именно в указанных координатах */
    childWindows[i]=XCreateWindow(ourDisplay,myWindow2,wx-20,wy-10,40, 20,
                 0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
   };

  /* Включаем дочерние окна */
  XMapSubwindows(ourDisplay,myWindow2);

  XFlush(ourDisplay);
  sleep(1);
  
  /* Устраиваем хоровод из дочерних окон на 3 круга */
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

  /* Уничтожаем окна */
  XDestroyWindow(ourDisplay, myWindow1);
  XFlush(ourDisplay);
  sleep(1);
  XDestroyWindow(ourDisplay, myWindow3);
  XFlush(ourDisplay);
  sleep(1);
  XDestroyWindow(ourDisplay, myWindow2);

  /* Закрываем соединение с сервером */
  XCloseDisplay(ourDisplay);

  return 0;
 };
