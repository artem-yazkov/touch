#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>

#define pi 3.1415
#define childNum 16   /* Количество дочерних окон в хороводе */

/* Эта функция по точке в центре, радиусу и углу */
/* вычисляет точку на периметре круга            */
void calcXY(int cx, int cy, int rd, int ang, int *wx, int *wy)
 {
  *wx=cx+(int) (cos((float)ang*pi/180.0)*(float)rd);
  *wy=cy-(int) (sin((float)ang*pi/180.0)*(float)rd);
 };

/* Подпрограмма вывода списка визуальных типов */
void listVisuals(XVisualInfo *firstVisual, int nitems)
 {
   int i,j;
   
   if (firstVisual==NULL)
     {
      printf("Визуальные типы не найдены.\n");
     }
    else
     {
      printf("Найдено визуальных типов: %d.\n",nitems);
      for(i=0; i<nitems; i++)
       {
        printf("%d. Глубина цветности: %d, Класс: ",i,firstVisual[i].depth);
        switch (firstVisual[i].c_class)
	 {
	  case StaticGray: printf("StaticGray"); break;
	  case StaticColor: printf("StaticColor"); break;
	  case TrueColor: printf("TrueColor"); break;
	  case GrayScale: printf("GrayScale"); break;
	  case PseudoColor: printf("PseudoColor"); break;
	  case DirectColor: printf("DirectColor"); break;
          default: printf("Неизвестен"); break;
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
  Window  myWindow;                    /* Рабочее окно верхнего уровня */
  Window childWindows[childNum];       /* Список дочерних окон         */
  unsigned long childColors[childNum]; /* Набор фоновых цветов         */
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
      printf("Не удалось установить соединение с графическим терминалом.\n");
      return 1;
    };

  /* Получим предварительные сведения */
  ourScreen=DefaultScreen(ourDisplay);           /* Экран по-умолчанию        */
  rootWindow=RootWindow(ourDisplay, ourScreen);  /* Корневое окно             */
  myDepth=DefaultDepth(ourDisplay, ourScreen);   /* Глубина цветности экрана  */
  myVisual=DefaultVisual(ourDisplay, ourScreen); /* Визуальные характеристики */
  myColormap=DefaultColormap(ourDisplay, ourScreen); /* Цветовая палитра */

  /* Выведем список всех визуальных типов */
  listOfVisuals=XGetVisualInfo(ourDisplay, VisualNoMask, &templateVisual,
   &numberOfVisuals);
  listVisuals(listOfVisuals,numberOfVisuals);
  if (listOfVisuals!=NULL)
    XFree(listOfVisuals);
  
  /* Выбираем фон для окна верхнего уровня */
  myAttr.background_pixel=WhitePixel(ourDisplay, ourScreen);  /* Белый цвет экрана */

  /* Создаем окно верхнего уровня */
  myWindow=XCreateWindow(ourDisplay,rootWindow,100, 100, 320, 300,
    0, myDepth, InputOutput, myVisual, CWBackPixel, &myAttr);
    
  /* Устанавливаем заголовок окна */
  XStoreName(ourDisplay,myWindow,"Color example window");

  /* Устанавливаем заголовок иконки */
  XSetIconName(ourDisplay,myWindow,"example");

  /* Устанавливаем ограничения на размеры окна */
  mySizeHints.flags=PMinSize | PMaxSize;
  mySizeHints.min_width=320; mySizeHints.min_height=300;
  mySizeHints.max_width=320; mySizeHints.max_height=300;
  XSetWMNormalHints(ourDisplay, myWindow, &mySizeHints);

  /* Делаем окна видимыми */
  XMapWindow(ourDisplay, myWindow);

  /* Зададим цвет бордюра для дочерних окон */
  myAttr.border_pixel=BlackPixel(ourDisplay, ourScreen);

  /* Создадим второму окну набор дочерних окон, расположенных по кругу */
  for (i=0; i<childNum; i++)
   {
    /* Вычисляем координаты дочернего окна */
    calcXY(160,150,110,i*(320/childNum),&wx,&wy);

    /* Выделяем цвет для окна */
    if (XAllocNamedColor(ourDisplay,myColormap,colorNames[i&15],
                             &theColor,&theColor)!=0)
      {
       childColors[i]=theColor.pixel;
      }
     else
      {
       /* Аварийно выходим из программы */
       printf("Не удалось получить цвета для дочерних окон.\n");
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

  /* Включаем дочерние окна */
  XMapSubwindows(ourDisplay,myWindow);

  XFlush(ourDisplay);
  sleep(1);
  
  /* Устраиваем хоровод из дочерних окон на 3 круга */
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

  /* Развернем окна в обратном порядке */
  XRestackWindows(ourDisplay,childWindows,childNum);  

  XFlush(ourDisplay);
  sleep(2);

  /* Хоровод движется в другую сторону... */
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

  /* Освобождаем выделенные цвета */
  XFreeColors(ourDisplay, myColormap,childColors,childNum,0);

  /* Уничтожаем окно */
  XDestroyWindow(ourDisplay, myWindow);

  /* Закрываем соединение с сервером */
  XCloseDisplay(ourDisplay);

  return 0;
 };
