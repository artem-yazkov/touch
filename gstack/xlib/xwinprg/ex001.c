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
      printf("Не удалось установить соединение с графическим терминалом.\n");
      return 1;
    };

  /* Получим предварительные сведения */
  ourScreen=DefaultScreen(ourDisplay);          // Экран по-умолчанию
  rootWindow=RootWindow(ourDisplay, ourScreen); // Корневое окно
  bgcolor=WhitePixel(ourDisplay, ourScreen);    // Белый цвет экрана

  /* Открываем окно */
  myWindow=XCreateSimpleWindow(ourDisplay,rootWindow,100, 100, 320, 200,
    0, 0, bgcolor);
    
  /* Делаем окно видимым */
  XMapWindow(ourDisplay, myWindow);
  
  /* Все сформированные команды принудительно сбрасываем на сервер */
  XFlush(ourDisplay);

  /* Спим 5 секунд */
  sleep(5);
  
  /* Уничтожаем окно */
  XDestroyWindow(ourDisplay, myWindow);

  /* Закрываем соединение с сервером */
  XCloseDisplay(ourDisplay);

  return 0;
 };
