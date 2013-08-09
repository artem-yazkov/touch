#include <stdio.h>
#include <X11/Xlib.h>

int main()
 {
  Display *ourDisplay;
  
  ourDisplay=XOpenDisplay(NULL);
  if (ourDisplay==NULL)
    {
      printf("Не удалось установить соединение с графическим терминалом.\n");
      return 1;
    };

  /* Выводим сведения о графическом терминале */
  printf("Установлено соединение с графическим терминалом.\n");
  printf(" Номер соединения: %d;\n",ConnectionNumber(ourDisplay));
  printf(" Используется протокол версии %d.%d;\n",ProtocolVersion(ourDisplay),ProtocolRevision(ourDisplay));
  printf(" Разработчик сервера: %s;\n",ServerVendor(ourDisplay));
  printf(" Версия сервера: %d\n",VendorRelease(ourDisplay));
  printf(" Строка соединения: [%s];\n",DisplayString(ourDisplay));
  printf(" Количество экранов на сервере: %d;\n",ScreenCount(ourDisplay));
  printf(" Номер экрана по-умолчанию: %d;\n",DefaultScreen(ourDisplay));

  XCloseDisplay(ourDisplay);
  return 0;
 };
