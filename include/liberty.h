/*
 * LiberalChat - Liberty
 *
 * GPL-v3 LICENSE
 *
 * Free software organization
 */

#ifndef _LIBERTY_H
#define _LIBERTY_H

#ifdef __cplusplus
extern "C"{
#else
typedef enum bool{
    false=0, true=1
}bool;
#endif

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#else
#error "SQLite3 is required!"
#endif

#ifdef HAVE_NCURSESW
#include <ncurses/ncurses.h>
#else
#error "ncursesw is required!"
#endif

#include <sys/signal.h> // sig_atomic_t

extern volatile sig_atomic_t proc_status;

extern char* userhome;
extern char* configroot;
extern char* configdir;
extern char* dbfilepath;

extern bool initialized;

extern sqlite3* maindb;

extern bool initialize_liberty_database();
extern bool initialize_liberty();

extern void close_proc();

#ifdef __cplusplus
}
#endif

#endif
