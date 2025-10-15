#ifndef MYFUNC_H
#define MYFUNC_H

#include <stdio.h>
#include <conio.h>
#include <stddef.h>
#include <windows.h>

static inline unsigned int BinToDec (int stellen) {
   char ch;
   int i = 0;
   unsigned int res = 0; 
   if (stellen>(sizeof(stellen)*8)) stellen=sizeof(stellen)*8;
   char text[stellen+1];
   
   i = 0;
   do {
       ch = getch();
       if (ch == 8) {      //Backspace
                if (i > 0) {
                    i--;
                    printf("\b \b");
                    }
       } else if (ch=='0' || ch=='1' ) {
                    if (i<stellen) {
                       text[i] = ch;
                       i++;
                       printf("%c",ch);
                    }
       } 
   } while (ch != 13);
   text[i] = '\0';
   
   //Pruefen Binar
   i = 0;
   res = 0;
   while (text[i] != '\0') {
         res = res * 2 + (text[i] - '0');
         i++;
   }
   return res;
}

static inline void gotoxy(short x, short y) {
    COORD pos = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

static inline void getxy(short *x, short *y) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        *x = csbi.dwCursorPosition.X;
        *y = csbi.dwCursorPosition.Y;
    } else {
        *x = 0;
        *y = 0;
    }
}

static inline void getConsoleSize(short *width, short *height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        *width  = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        *width = *height = 0;
    }
}

static inline void hidecursor (void) {
     HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
     CONSOLE_CURSOR_INFO info;
     info.dwSize = 100;
     info.bVisible = FALSE;
     SetConsoleCursorInfo(consoleHandle, &info);
}


static inline void enableVT (void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m;
    GetConsoleMode(h, &m);
    SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static inline void rahmen_pos(int w, int h, int xpos, int ypos, char ch) {
  for (int y=0; y<=h; y++) {
      gotoxy (xpos, ypos+y);
      for (int x=0; x<=w; x++)
        {
          if (y==0 || y==h) printf("%c", ch);
          else if (x==0 || x==w) printf("%c", ch);
          else printf(" ");
        }
      printf("\n");
  }
}

static inline void rahmen_pg(int w, int h, int xpos, int ypos, int clear) {
  for (int y=0; y<=h-1; y++) {
      gotoxy (xpos, ypos+y);
      for (int x=0; x<=w-1; x++)
        {
          if (y==0 && x==0) printf("%c", 218); //left upper
          else if (y==0 && x==w-1) printf("%c", 191); //right upper
          else if (y==h-1 && x==0) printf("%c", 192); //left lower
          else if (y==h-1 && x==w-1) printf("%c", 217); //right lower
          else if (y==0 || y==h-1) printf("%c", 196);
          else if (x==0 || x==w-1) printf("%c", 179);
          else if (clear) printf(" ");
          else printf("\033[C"); //bestehende Zeichen nicht uberschreiben
        }
      printf("\n");
  }
}

static int max2(int a,int b){ return a>b?a:b; }
static int min2(int a,int b){ return a<b?a:b; }

static inline char* eingabeText(int maxlen) {
    char *p;
    char ch;
    int i=0;
    p = (char*)malloc(maxlen + 1);
    //printf("Bitte Text eingeben (max %d Zeichen): ", maxlen);   
    do {
       ch = getch();
       //if ((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9')) {
       if (ch>=32 && ch<=255 && i<maxlen) {
            p[i]=ch;
            printf("%c",ch);
            i++;
       } else if (ch == 8 && i>0) {      //Backspace
            i--;
            printf("\b \b");
       }             
    } while (ch!=13);
    p[i] = '\0';
    return p;
}

#endif // MYFUNC_H
