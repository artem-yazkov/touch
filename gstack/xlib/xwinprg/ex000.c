#include <stdio.h>
#include <X11/Xlib.h>

int main()
 {
  Display *ourDisplay;
  
  ourDisplay=XOpenDisplay(NULL);
  if (ourDisplay==NULL)
    {
      printf("�� ������� ���������� ���������� � ����������� ����������.\n");
      return 1;
    };

  /* ������� �������� � ����������� ��������� */
  printf("����������� ���������� � ����������� ����������.\n");
  printf(" ����� ����������: %d;\n",ConnectionNumber(ourDisplay));
  printf(" ������������ �������� ������ %d.%d;\n",ProtocolVersion(ourDisplay),ProtocolRevision(ourDisplay));
  printf(" ����������� �������: %s;\n",ServerVendor(ourDisplay));
  printf(" ������ �������: %d\n",VendorRelease(ourDisplay));
  printf(" ������ ����������: [%s];\n",DisplayString(ourDisplay));
  printf(" ���������� ������� �� �������: %d;\n",ScreenCount(ourDisplay));
  printf(" ����� ������ ��-���������: %d;\n",DefaultScreen(ourDisplay));

  XCloseDisplay(ourDisplay);
  return 0;
 };
