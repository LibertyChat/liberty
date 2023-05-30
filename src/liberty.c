/*
 * LiberalChat - Liberty
 *
 * GPL-v3 LICENSE
 *
 * Free software organization
 */

#ifdef __cplusplus
extern "C"{
#endif

#include "liberty.h"

#include <stdio.h> // fprintf
#include <stdlib.h> // NULL
#include <string.h> // malloc, strcpy, strlen, strcat
#include <strings.h> // bzero
#include <unistd.h> // getenv
#include <assert.h> // assert
#include <sqlite3.h> /* for storing user data */
#include <sys/stat.h> // mkdir

#include <sys/signal.h> // SIGSEGV, sigaction, sig_atomic_t, sigemptyset

#include <ncurses/ncurses.h> /* for visual */
#include <locale.h> // set_locale, LC_ALL

#define DEFAULT_PERMISSION S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

volatile sig_atomic_t proc_status;
WINDOW* mainwin;

char* userhome;
char* configroot;
char* configdir;
char* dbfilepath;

bool initialized;

sqlite3* maindb;

void segfault(int sig){
    close_proc();
    fprintf(stderr,
            "###################################################\n"
            "An error has occurred within LIBERTY:\n"
            "    Signal: 11\n"
            "    Description: Segmentation fault (core dumped)\n"
            "Sorry! Liberty has to exit now!\n"
            "###################################################\n"
            );
}

bool initialize_liberty_database(){
    configroot=(char*)malloc((strlen(userhome)+strlen("/.config")+1)*sizeof(char));
    bzero(configroot, strlen(userhome)+strlen("/.config")+1);
    strcpy(configroot, userhome);
    strcat(configroot, "/.config");

    if(access(configroot, F_OK))
      assert(mkdir(configroot, DEFAULT_PERMISSION)==-1);

    configdir=(char*)malloc((strlen(configroot)+1+strlen("/liberty"))*sizeof(char));
    bzero(configdir, strlen(configroot)+strlen("liberty")+1);
    strcpy(configdir, configroot);
    strcat(configdir, "/liberty");

    if(access(configdir, F_OK))
      assert(mkdir(configdir, DEFAULT_PERMISSION)==-1);

    dbfilepath=(char*)malloc((strlen(configdir)+1+strlen("/userdata.db"))*sizeof(char));
    bzero(dbfilepath, strlen(configdir)+strlen("/userdata.db")+1);
    strcpy(dbfilepath, configdir);
    strcat(dbfilepath, "/userdata.db");

    sqlite3_open(dbfilepath, &maindb);
    char* ErrorMsg={0};
    sqlite3_exec(maindb, 
                "CREATE TABLE SERVER_LIST("
                    "NAME     CHAR(50),"
                    "ADDRESS  CHAR(128),"
                    "PORT     INT"
                ");"
                 , NULL, NULL, &ErrorMsg);
    fprintf(stderr, "log: sql create: %s\n", ErrorMsg);
    return true;
}

bool initialize_liberty(){
    proc_status=0;
    char buf[1024]={0};
    if(getenv("HOME")==NULL){
        fprintf(stderr, "liberty: no environment variable called HOME was set ** FAILED\n");
        return false;
    }
    strcpy(buf, getenv("HOME"));
    userhome=(char*)malloc((strlen(buf)+1)*sizeof(char));
    bzero(userhome, strlen(buf)+1);
    strcpy(userhome, buf);
    assert(initialize_liberty_database());
    return true;
}

void close_proc(){
    free(userhome);
    free(configroot);
    free(configdir);
    free(dbfilepath);
    sqlite3_close(maindb);
    echo();
    nocbreak();
    curs_set(1);
    endwin();
}

static WINDOW* createwin(int h, int w, int sy, int sx/*, int add_border*/){
    WINDOW* newwindow;
    newwindow=newwin(h, w, sy, sx);
    box(newwindow, 0, 0);
    wrefresh(newwindow);
    return newwindow;
}

int quit_proc(){
    WINDOW* menu=createwin(7, 27, LINES/2-3, COLS/2-(27/2)/*  true*/);
    wmove(menu, 2, 4);
    wprintw(menu, "Are you sure exit?");
    wmove(menu, 4, 5);
    wprintw(menu, "[Yes]      [No]");
    int choice=2;
    mvwchgat(menu, 4, 16, 4, A_BOLD, 2, NULL);
    wrefresh(menu);
    int ch=0;
    bool breakblock=false;
    bool quit_prog=false;
    while(1){
        if(quit_prog)
          return true;
        if(breakblock)
          return false;
        ch=getch();
        switch(ch){
          case KEY_LEFT:
            if(choice==2){
                choice=1;
                mvwchgat(menu, 4, 16, 4, A_NORMAL, 1, NULL);
                mvwchgat(menu, 4, 5, 5, A_BOLD, 2, NULL);
                wrefresh(menu);
            }
            break;
          case KEY_RIGHT:
            if(choice==1){
                choice=2;
                mvwchgat(menu, 4, 5, 5, A_NORMAL, 1, NULL);
                mvwchgat(menu, 4, 16, 4, A_BOLD, 2, NULL);
                wrefresh(menu);
            }
            break;
          case 10:
            wclear(menu);
            wrefresh(menu);
            delwin(menu);
            if(choice==1){
                quit_prog=true;
            }
            else{
                breakblock=true;
            }
            break;
        }
    }
    return false;
}

static void listen_keypad(){
    char* server_choice=(char*)malloc(sizeof(char)*(1));
    bzero(server_choice, 1);
    int ch;
    bool quit=false;
    int oh, ow;
    flushinp();
    WINDOW* serverlist=createwin(LINES-1, 30, 1, 0);
    box(serverlist, 0, 0);
    WINDOW* chatwin=createwin(LINES-1, COLS-30, 1, 30);
    box(chatwin, 0, 0);
    wrefresh(serverlist);
    wrefresh(chatwin);
    while(1){
        wrefresh(mainwin);
        if(LINES!=oh||COLS!=ow){
            oh=LINES;
            ow=COLS;
            wclear(mainwin);
            box(mainwin, 0, 0);
            mvwprintw(mainwin, 0, 0, "Liberty - Current Server: %s\n", server_choice);
            mvwchgat(mainwin, 0, 0, COLS, A_BOLD, 3, NULL);
            wrefresh(mainwin);
            wresize(serverlist, LINES-1, 30);
            wclear(serverlist);
            box(serverlist, 0, 0);
            wrefresh(serverlist);
            wresize(chatwin, LINES-1, COLS-30);
            wclear(chatwin);
            box(chatwin, 0, 0);
            wrefresh(chatwin);
        }
        if(quit)
          break;
        ch=getch();
        switch(ch){
          case 27:
            if(quit_proc())
              quit=true;
            break;
        }
    }
}

int main(){
    struct sigaction act;
    act.sa_handler=segfault;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGSEGV, &act, NULL);
    assert(initialize_liberty());
    setlocale(LC_ALL, getenv("LANG")!=NULL?getenv("LANG"):"");
    mainwin=initscr();
    start_color();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(-1);
    nodelay(stdscr, TRUE);
    set_escdelay(0);
    noecho();
    cbreak();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_WHITE);
    init_pair(3, COLOR_BLACK, COLOR_GREEN);
    box(mainwin, 0, 0);
    wrefresh(mainwin);
    listen_keypad();
    close_proc();
    return 0;
}


#ifdef __cplusplus
}
#endif
