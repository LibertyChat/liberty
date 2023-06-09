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

#include <stdio.h> // fprintf, FILE, sprintf
#include <stdlib.h> // NULL, free, _Exit
#include <string.h> // malloc, strcpy, strlen, strcat
#include <strings.h> // bzero
#include <unistd.h> // getenv
#include <assert.h> // assert
#include <sqlite3.h> // sqlite3_open, sqlite3_close, sqlite3_exec
#include <sys/stat.h> // mkdir, S_IRWXU, S_IRWXG, S_IROTH, S_IXOTH
#include <time.h> // tm, time_t, localtime, time
#include <sys/signal.h> // SIGSEGV, sigaction, sig_atomic_t, sigemptyset
#include <ncurses/ncurses.h> // initscr, start_color, timeout, set_escdelay,
                             // init_pair, newwin, delwin, box, mvwprintw,
                             // mvprintw, mvwchgat, getch, WINDOW, noecho,
                             // nocbreak, cbreak, echo, LINES, COLS, curs_set,
                             // keypad
#include <panel.h> // PANEL, new_panel, move_panel, show_panel, update_panel
#include <locale.h> // set_locale, LC_ALL

#define DEFAULT_PERMISSION S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

volatile sig_atomic_t proc_status;
WINDOW* mainwin;

char* userhome;
char* configroot;
char* configdir;
char* dbfilepath;

static char* logdir;
static char* logfilepath;
static FILE* logfile;

bool initialized;

sqlite3* maindb;

void segfault(int sig){
    fprintf(logfile,
            "###################################################\n"
            "An error has occurred within LIBERTY:\n"
            "    Signal: 11\n"
            "    Description: Segmentation fault (core dumped)\n"
            "Sorry! Liberty has to exit now!\n"
            "###################################################\n"
            );
    close_proc();
    fprintf(stderr, "Segmentation Fault, Please check the logfile at %s\n", logfilepath);
    _Exit(1);
}

bool initialize_liberty_database(){
    configroot=(char*)malloc((strlen(userhome)+strlen("/.config")+1)*sizeof(char));
    bzero(configroot, strlen(userhome)+strlen("/.config")+1);
    strcpy(configroot, userhome);
    strcat(configroot, "/.config");

    if(access(configroot, F_OK))
      assert(mkdir(configroot, DEFAULT_PERMISSION)!=-1);

    configdir=(char*)malloc((strlen(configroot)+1+strlen("/liberty"))*sizeof(char));
    bzero(configdir, strlen(configroot)+strlen("liberty")+1);
    strcpy(configdir, configroot);
    strcat(configdir, "/liberty");

    if(access(configdir, F_OK))
      assert(mkdir(configdir, DEFAULT_PERMISSION)!=-1);

    dbfilepath=(char*)malloc((strlen(configdir)+1+strlen("/userdata.db"))*sizeof(char));
    bzero(dbfilepath, strlen(configdir)+strlen("/userdata.db")+1);
    strcpy(dbfilepath, configdir);
    strcat(dbfilepath, "/userdata.db");

    logdir=(char*)malloc((strlen(configdir)+1+strlen("/logs"))*sizeof(char));
    bzero(logdir, strlen(configdir)+strlen("/logs")+1);
    strcpy(logdir, configdir);
    strcat(logdir, "/logs");

    if(access(logdir, F_OK))
      assert(mkdir(logdir, DEFAULT_PERMISSION)!=-1);

    struct tm* _timer;
    time_t* time_s=(time_t*)malloc(sizeof(time_t));
    time(time_s);
    _timer=localtime(time_s);
    free(time_s);

    logfilepath=(char*)malloc((strlen(logdir)+44)*sizeof(char));
    bzero(logfilepath, 43+strlen(logdir));
    int count=0;
    do{
        if(count==0)
          snprintf(logfilepath, 44+strlen(logdir), "%s/liberty-%04d-%02d-%02d-%02d:%02d:%02d.log", logdir, 
                   _timer->tm_year+1900, _timer->tm_mon+1, _timer->tm_mday, _timer->tm_hour,
                   _timer->tm_min, _timer->tm_sec);
        else
          snprintf(logfilepath, 44+strlen(logdir), "%s/liberty-%04d-%02d-%02d-%02d:%02d:%02d-%d.log", logdir, 
                   _timer->tm_year+1900, _timer->tm_mon+1, _timer->tm_mday, _timer->tm_hour,
                   _timer->tm_min, _timer->tm_sec, count);
        if(access(logfilepath, F_OK)==0){
            count++;
            continue;
        }
        else
          break;
    }while(1);

    logfile=fopen(logfilepath, "w");
    assert(logfile!=NULL);

    sqlite3_open(dbfilepath, &maindb);
    char* ErrorMsg={0};
    sqlite3_exec(maindb, 
                "CREATE TABLE SERVER_LIST("
                    "NAME     CHAR(50),"
                    "ADDRESS  CHAR(128),"
                    "PORT     INT"
                ");"
                 , NULL, NULL, &ErrorMsg);
    fprintf(logfile, "SQLite3 Create: %s\n", ErrorMsg);
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
    free(logdir);
    free(logfilepath);
    sqlite3_close(maindb);
    echo();
    nocbreak();
    curs_set(1);
    endwin();
    fprintf(logfile, "- Program Exit\n");
    fclose(logfile);
}

static WINDOW* createwin(int h, int w, int sy, int sx, char* name){
    WINDOW* newwindow;
    newwindow=newwin(h, w, sy, sx);
    box(newwindow, 0, 0);
    wrefresh(newwindow);
    fprintf(logfile, "Window Created, name=%s, size=%dx%d, position=(%d,%d)\n", name, w, h, sx, sy);
    return newwindow;
}

static void deletewin(WINDOW* win, char* name){
    delwin(win);
    fprintf(logfile, "Delete Window, %s deleted\n", name);
}

int quit_proc(){
    WINDOW* menu=createwin(7, 27, LINES/2-3, COLS/2-(27/2), "Menu");
    wmove(menu, 2, 4);
    wprintw(menu, "Are you sure exit?");
    wmove(menu, 4, 5);
    wprintw(menu, "[Yes]      [No]");
    int choice=2;
    mvwchgat(menu, 4, 16, 4, A_BOLD, 2, NULL);
    mvwprintw(menu, 0, 2, " Menu ");
    wrefresh(menu);
    PANEL* MENU=new_panel(menu);
    int ch=0;
    bool breakblock=false;
    bool quit_prog=false;
    while(1){
        move_panel(MENU, LINES/2-3, COLS/2-(27/2));
        update_panels();
        if(quit_prog){
            del_panel(MENU);
            fprintf(logfile, "Menu Exit Called\n");
            return true;
        }
        if(breakblock){
            del_panel(MENU);
            fprintf(logfile, "Menu Exit Canceled\n");
            return false;
        }
        ch=getch();
        if(ch!=-1)
          fprintf(logfile, "Keypad Issue: %d\n", ch);
        else
          continue;
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
            if(choice==1){
                quit_prog=true;
            }
            else{
                breakblock=true;
            }
            break;
          case 27:
            wclear(menu);
            wrefresh(menu);
            breakblock=true;
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
    WINDOW* serverlist=createwin(LINES-1, 30, 1, 0, "Server List");
    box(serverlist, 0, 0);
    WINDOW* chatwin=createwin(LINES-4, COLS-30, 1, 30, "Chat Window");
    box(chatwin, 0, 0);
    wrefresh(serverlist);
    wrefresh(chatwin);
    PANEL* SL=new_panel(serverlist);
    PANEL* CW=new_panel(chatwin);
    WINDOW* TextInput=createwin(3, COLS-30, LINES-3, 30, "Text Input Window");
    box(TextInput, 0, 0);
    wrefresh(TextInput);
    PANEL* TI=new_panel(TextInput);
    show_panel(TI);
    bottom_panel(TI);
    move_panel(TI, LINES-3, 30);
    wrefresh(TI->win);
    update_panels();
    while(1){
        wrefresh(mainwin);
        if(LINES!=oh||COLS!=ow){
            fprintf(logfile, "Screen Resized, Now is: %dx%d\n", COLS, LINES);
            oh=LINES;
            ow=COLS;
            wclear(mainwin);
            box(mainwin, 0, 0);
            mvwprintw(mainwin, 0, 0, "Liberty - Current Server: %s\n", server_choice);
            mvwchgat(mainwin, 0, 0, COLS, A_BOLD, 3, NULL);
            wrefresh(mainwin);

            move_panel(SL, 1, 0);
            wresize(serverlist, LINES-1, 30);
            wclear(serverlist);
            box(serverlist, 0, 0);
            mvwprintw(serverlist, 0, 2, " Server List ");
            wrefresh(serverlist);

            move_panel(TI, LINES-3, 30);
            wresize(TI->win, 3, COLS-30);
            wclear(TI->win);
            box(TI->win, 0, 0);
            mvwprintw(TI->win, 0, 2, " Edit Message ");
            wrefresh(TI->win);

            move_panel(CW, 1, 30);
            wresize(chatwin, LINES-4, COLS-30);
            wclear(chatwin);
            box(chatwin, 0, 0);
            mvwprintw(chatwin, 0, 2, " Free Chat ");
            wrefresh(chatwin);

            update_panels();
        }
        if(quit)
          break;
        ch=getch();
        if(ch!=-1)
          fprintf(logfile, "Keypad Issue: %d\n", ch);
        else
          continue;
        switch(ch){
          case 27:
            fprintf(logfile, "Page: Menu\n");
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
