/*---------------------------------------------------------------------------------*/
/* Interactive FORum Multiplexor Software    -    IFORMS  V 1.13                   */
/*---------------------------------------------------------------------------------*/
/*  This code is a collection of software that originally started as a system      */
/*  called:                           NUTS 1.0.3.                                  */
/*           Neils Unix Talk Server system (NUTS) - (C) Neil Robertson 1992/93     */
/*                      Last update 29/5/93  Version 1.0.3                         */
/*                                                                                 */
/* As a result of extensive changes, it can no longer be considered the same code. */
/*                                                                                 */
/*                                                                    Deep         */
/*                                                                                 */
/* Legal note:  This code may not be freely distributed.  Doing so may be in       */
/*              violation of the US Munitions laws which cover exportation of      */
/*              encoding technology.                                               */
/*                                                                                 */
/*---------------------------------------------------------------------------------*/
	
 /* last modified: may 25, 1995 */

 /* things to do:
 /*               add stargate
 /*               add objects
 /*               convert read to a more effective code
 /*               intertalker connectivity
 /*               auto-shutdown
 /*               samesite
 /*               sleepfor, notify, ignorelist
 /*               the who short list
 /*               enhance macros
 /*               investigate using a database
 /*               multiline mail
 /*               tie to real mail
 /*               add command initialization from a file
 /*               create archive process (external code)
 /*               enhance room connectivity
 /*               create standards for diffent term type
 /*               no_new_users_from_site
 /**/

 /****************************************************************
 * NOTE: for AIX users the getrlimit is not supported by the OS.
 *
 *       for sun users the cpp is not ansi standard so preprocessor
 *          strings are not supported properly
 ******************************************************************/

/*--------------------------------------------------------------*/
/* the debug switch make it easier to run this under a debugger */
/* it prevents deamonization and hardcodes the config directory */
/* to testfig when set to 1                                     */
/*--------------------------------------------------------------*/

#define DEBUG 0

/*-------------------------------------------------------------*/
/* includes used for this code                                 */
/*-------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>    /* trial */
#include <arpa/telnet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/un.h>            /* linux users      */
#include <string.h>
/* #include <sys/select>          /* for aix users    */

/*--------------------------------------------------------*/
/* change this to the owner's id on this system           */
/* so promotions can be made                              */
/*--------------------------------------------------------*/

#include "if_cons.h"

/*---------------------------------------------------------*/
/* port definitions                                        */
/*---------------------------------------------------------*/

struct {
         int  total_connections_allowed;
         int  users;
         int  wizes;
         int  who;
         int  interconnect;
         int  cypherconnect;
        } range =
          {  MAX_USERS + MAX_WHO_CONNECTS + MAX_INTERCONNECTS + MAX_CYPHERCONNECTS,
             NUM_USERS,
             NUM_USERS + NUM_WIZES,
             NUM_USERS + NUM_WIZES + MAX_WHO_CONNECTS,
             NUM_USERS + NUM_WIZES + MAX_WHO_CONNECTS + MAX_INTERCONNECTS,
             NUM_USERS + NUM_WIZES + MAX_WHO_CONNECTS + MAX_INTERCONNECTS + MAX_CYPHERCONNECTS
           };

int PORT;                  /* main port for incoming         */
int WIZARD_OFFSET = -50;   /* wiz  port for incoming         */
int WHO_OFFSET    =   7;   /* standard offset for who        */
int INTER_OFFSET  =   2;   /* inter talker offset            */
int CRYPT_OFFSET  =   3;   /* offset for caller-id verify    */
int WWW_OFFSET    = -30;   /* www offset                     */

int ls;     /* ls = listen socket , as= accept socket - both 32 bit */
int who;    /* external who connections                             */
int www;    /* mini www server                                      */
int inter;  /* inter talker connections                             */
int cypher; /* caller id port                                       */
int wiz;    /* wizard port                                          */


/*-------------------------------------------------------------------*/
/* string size constants                                             */
/*-------------------------------------------------------------------*/

#define ARR_SIZE      9700    /* max socket can take is 9600 (+100 to be sure) */
#define MAX_LINE_LEN  200
#define DESC_LEN      41
#define NAME_LEN      21
#define FILE_NAME_LEN 256
#define NUM_LINES     15     /* number of lines of conv. to store */
#define TOPIC_LEN     45
#define EMAIL_LENGTH  80

/*-------------------------------------------------------------------*/
/* room or area constants                                            */
/*-------------------------------------------------------------------*/

#define MAX_AREAS   60
#define PRINUM      2    /* no. of users in room befor it can be made private */
#define PRIROOM     0    /* room that is always private */
#define PRIROOM2    10   /* Second room that's always private */
#define INIT_ROOM   0    /* room that users log into */

#define PRIV_ROOM_RANK 4

/*-------------------------------------------------------------------*/
/* misc. constants used throughout the code                          */
/*-------------------------------------------------------------------*/

#define TRUE          1
#define FALSE         0

#define MIN_HIDE_LEVEL  3
#define SHOW_HIDDEN     1
#define WIZ_LEVEL       4
#define RESOLVE_DEFAULT 1
#define MAX_NEW_PER_DAY 50
#define MAX_LEVEL       7     /* number of ranks                   */

/*-----------------------------------------------------------------*/
/* constants used for the fight command                            */
/*-----------------------------------------------------------------*/

#include "if_text.h"

/*-----------------------------------------------*/
/* wiz only flag                                 */
/*-----------------------------------------------*/

#define WIZ_ONLY -4

/*--------------------------------------------------------*/
/* terminal control switch settings                       */
/*--------------------------------------------------------*/
#define NORM      0
#define BOLD      1
#define COLOR1    2
#define COLOR2    3
#define COLOR3    4
#define COLOR4    5

/*--------------------------------------------------------*/
/* message types used for writeall                        */
/*--------------------------------------------------------*/

#define NONE      0
#define AFK_TYPE  1
#define SAY_TYPE  2
#define LOGIO     3
#define SHOUT     4
#define KNOCK     5
#define MESSAGE   6
#define TOPIC     7
#define KILL      8
#define BCAST     9
#define MOVE      10
#define ECHO      11
#define GREET     12
#define PICTURE   13
#define FIGHT     14
#define ATMOS     15

struct {
        char text[32];
       } flag_names[] = {
       {"misc_things"},
       {"afks"},
       {"says"},
       {"logs"},
       {"shouts"},
       {"knocks"},
       {"messages"},
       {"topics"},
       {"kills"},
       {"bcasts"},
       {"moves"},
       {"echos"},
       {"greets"},
       {"pictures"},
       {"fights"},
       {"atmos"},
       {""}
      };

#define NUM_IGN_FLAGS 16

/*----------------------------------------------------------------------*/
/* define some macros                                                   */
/*----------------------------------------------------------------------*/

#undef feof  /* otherwise feof func. wont compile */
#define LOOP_FOREVER while(1)
#define wbuf(buf) xwrite (f, (char *)buf, sizeof (buf))
#define wval(val) xwrite (f, (char *)&val, sizeof (val))
#define rbuf(buf) xread (f, (char *)buf, sizeof(buf))
#define rval(val) xread (f, (char *)&val, sizeof(val))
#define FCLOSE(file) if (file) fclose(file)
#define CHECK_NAME(var) if (check_fname(var,user)) { \
                          write_str(user,"Illegal name."); \
                          return;}

void sigcall();
int resolve_names = 1;
int check_restriction();
int copy_to_user();
int copy_from_user();
int read_user();
int write_user();
int xwrite();
int xread();
int st_crypt();
int picture();
int preview();
int catall();
int check_fname();
int password();
int roomsec();
int tog_monitor();
int ptell();
int follow();
int beep();
int systime();
int semote();
int print_users();
int usr_stat();
int print_dir();
int print_to_syslog();
int set_sex();
int set_email();
int set();
int swho();
int how_many_users();
int fight_another();
int shutdown_it();

/** Far too many bloody global declarations **/
int last_user;

struct {
        char command[32];            /* command name                      */
        int  su_com;                 /* authority level                   */
        int  jump_vector;            /* the number to use for the command */
       } sys[] = {
                  {".afk",		0,		75},
                  {".bafk",		0,		99},
                  {".cbuff",		0,		42},
                  {".cls",		0,		84},
                  {".cmail",		0,		47},
                  {".desc",		0,		37},
                  {".emote",		0,		11},
                  {".fight",		0,		85},
                  {".go",		0,		7},
                  {".help",		0,		25},
                  {".heartells",	0,		62},
                  {".hilite",		0,		96},
                  {".ignore",		0,		5},
                  {".igtells",		0,		61},
                  {".invite",		0,		10},
                  {".listen",		0,		4},
                  {".look",		0,		6},
                  {".macros",		0,		44},
                  {".news",		0,		27},
                  {".password",		0,		67},
                  {".quit",		0,		0},
                  {".read",		0,		15},
                  {".review",		0,		24},
                  {".rmail",		0,		45},
                  {".rooms",		0,		12},
                  {".say" ,		0,		90},
                  {".semote" ,		0,		69},
                  {".set",		0,		79},
                  {".tell",		0,		3},
                  {".time",		0,		76},
                  {".who",		0,		1},

                  /* on some machines this preprocessor is not allowed.(sun)*/
                  /* change the "."ROOT_ID to something like ".deep"        */
                  /* you will have to change this is one other spot as well */

                  {".orca",		0,		43},

                  {".greet",		2,		40},
                  {".knock",		1,		13},
                  {".meter",            1,              91},
                  {".preview",		1,		66},
                  {".private",		1,		8},
                  {".public",		1,		9},
                  {".search",		1,		23},
                  {".smail",		1,		46},
                  {".ustat",		1,		78},
                  {".with",		1,		102},
                  {".wizards",		0,		101},
                  {".write",		1,		14},

                  {".beep",		2,		74},
                  {".echo",		2,		36},
                  {".follow",		2,		72},
                  {".ptell",		2,		71},
                  {".ranks",		2,		58},
                  {".shout",		2,		2},
                  {".topic",		2,		18},

                  {".arrest",		4,		41},
                  {".muzzle",		4,		51},
                  {".system",		3,		28},
                  {".unmuzzle",		4,		52},
                  {".wipe",		5,		16},

                  {".bcast",		5,		26},
                  {".bring",		3,		54},
                  {".hide",		3,		56},
                  {".monitor", 		5,		70},
                  {".move",		4,		29},
                  {".picture",		3,		65},

                  {".btell",		5,		82},
                  {".demote",		5,		50},
                  {".kill",		5,		21},
                  {".permission",	5,		68},
                  {".promote",		5,		49},
                  {".site",		5,		80},

                  {".atmos",		6,		35},
                  {".nuke",		6,		83},
                  {".restrict",		6,		59},
                  {".resolve",		6,		86},
                  {".unrestrict",	6,		60},
                  {".users",		6,		77},
                  {".wnote",		5,		97},
                  {".xcomm",            6,             100},

                  {".allow_new",	7,		38},
                  {".close",		7,		30},
                  {".open",		7,		31},
                  {".quota" ,		7,		95},
                  {".reinit" ,		7,		73},
                  {".shutdown",		7,		22},
                  {".wwipe",		7,		98},


                  /*-----------------------------------------------*/
                  /* this item marks the end of the list, do not   */
                  /* remove it                                     */
                  /*-----------------------------------------------*/
                  {"<EOL>",	       -1,		-1}
                };


/** Big Letter array map **/
int biglet[26][5][5] =
    {0,1,1,1,0,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
     1,1,1,1,0,1,0,0,0,1,1,1,1,1,0,1,0,0,0,1,1,1,1,1,0,
     0,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,1,1,1,
     1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
     1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,1,1,1,1,
     1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,
     0,1,1,1,0,1,0,0,0,0,1,0,1,1,0,1,0,0,0,1,0,1,1,1,0,
     1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
     0,1,1,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
     0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
     1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
     1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
     1,0,0,0,1,1,1,0,1,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,1,
     1,0,0,0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,
     0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
     1,1,1,1,0,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,
     0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,0,
     1,1,1,1,0,1,0,0,0,1,1,1,1,1,0,1,0,0,1,0,1,0,0,0,1,
     0,1,1,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1,0,
     1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
     1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,
     1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,1,0,1,0,0,0,1,0,0,
     1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,0,1,
     1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,
     1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
     1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1};

char *syserror="Sorry - a system error has occured";

char area_nochange[MAX_AREAS];
char mess[ARR_SIZE+25];    /* functions use mess to send output */
char t_mess[ARR_SIZE+25];  /* functions use t_mess as a buffer */
char conv[MAX_AREAS][NUM_LINES][MAX_LINE_LEN+1]; /* stores lines of conversation in room*/
char bt_conv[NUM_LINES][MAX_LINE_LEN+1]; /* stores lines of conversation in room*/
char datadir[255];
char start_time[30];  /* startup time */
char owner_mes[80];   /* Owner's message status */

int NUM_AREAS;
int num_of_users=0;
int MESS_LIFE=0;  /* message lifetime in days */

int noprompt;
int signl;
int atmos_on;
int syslog_on;
int allow_new;
int average_tells;

int tells;
int commands;
int says;
int says_running;
int tells_running;
int commands_running;

int shutd= -1;
int sys_access=1;
int checked=0;  /* see if messages have been checked */
int bt_count;

/* user structure */
struct profile {
	char name[NAME_LEN];
	char password[NAME_LEN];
	char desc[DESC_LEN]; /* user description */
	char email_addr[EMAIL_LENGTH];
	char sex[10];
	char site[21]; /* internet site number */
        char init_date[17];
        char init_site[21];
        char last_date[17];
        char last_site[21];
	char dir[128];
        char security[MAX_AREAS+1];
	char login_name[NAME_LEN];
	char login_pass[NAME_LEN];
	char page_file[80];
	int locked;
	int suspended;
	int area;
	int listen;
	int shout;
	int igtell;
	int clrmail; /* User is clearing his mail file */
	int sock;  /* socket number */
	int monitor;
	int time;  /* length of time user has been on */
	int vis;   /* is user visible */
	int super;
	int invite;  /* area currently invited to */
	int last_input; /* this and warning are used for idle check */
	int warning_given;
	int logging_in;  /* user is logging in */
	int attleft;  /* no. of logging in attempts left */
	int file_posn;
	char net_name[64]; /* internet site name */
	char macros[10][50];
        int  conv_count;
        char conv[NUM_LINES+1][MAX_LINE_LEN+1]; /* stores lines of conversation in room*/
        int cat_mode;
        int rows;
        int cols;
        int car_return;
        int abbrs;
        int white_space;
        int line_count;
        int number_lines;
        int times_on;
        int afk;
        int upper;
        int lower;
        int aver;
        int hilite;
        int new_mail;
        char flags[25];
        char attach_port;
	};
struct profile ustr[MAX_USERS];
struct profile t_ustr;

struct {
        long logins_today;
        long logins_since_start;
        long quota;
        long new_users_today;
        int  time_started;
} system_stats;

/* area structure */
struct {
	char name[NAME_LEN];
	char move[MAX_AREAS];  /* where u can move to from area */
	char topic[TOPIC_LEN+1];
	int private;
	int hidden;
	int secure;
	int mess_num;  /* num. of messages in area */
	int conv_line; /* next line of conv string to write to */
	} astr[MAX_AREAS];

struct {
        int first_user;
        int second_user;
        int issued;
        int time;
       } fight;

/**** START OF FUNCTIONS ****/

/****************************************************************************
     Main function -
     Sets up network data, signals, accepts user input and acts as
     the switching centre for speach output.
*****************************************************************************/
main(argc,argv)
int argc;
char *argv[];
{
struct sockaddr_in bind_addr,        /* this is the socket for binding       */
                   acc_addr,         /*                        accepting     */
                   who_addr,         /*                        ext. who      */
                   whoi_addr,        /*                        ext. who in   */
                   wiz_addr,         /*                        wizard in     */
                   www_addr;         /*                        www address   */

int as;  /* ls = listen socket , as= accept socket - both 32 bit */
int area;
char port;
int size=sizeof(struct sockaddr_in);
int user;
int com_num;
int len;
int new_user;
fd_set readmask;
char inpstr[ARR_SIZE];
char *inet_ntoa();  /* socket library function */
struct hostent * net_info;
unsigned long ip_address;
char *dest;
char inpchar[2];
int buff_size;
FILE * fp;
char filename[1024];

puts("IFORMS V1.12,  Copyright Public Domain Software, Inc.");
puts("               Interactive FORum Multiplexor Software");
puts("                                                     ");

if (DEBUG)
  {
   puts("NOTE: Running in debug mode, using testfig for configuration info.");
   strcpy(datadir,"testfig");
  }
 else
  if (argc==1)       /* check comline input */
    {
     puts("NOTE: Running with config data directory/n");
     strcpy(datadir,"config");
    }
   else
    {
     strcpy(datadir,argv[1]);
    }

puts("** Talk server booting... **");

printf("Reading area data from dir. %s ...\n",datadir);

/* read system data */
read_init_data();

if (RESOLVE_DEFAULT)
  resolve_names_on(-1," ");
 else
  resolve_names_off(-1," ");


/*---------------------*/
/* initialize sockets  */
/*---------------------*/

printf("Initializing sockets \n");

set_up_socket(PORT,
              0,
              "Main talker port ",
              &bind_addr,
              &ls);

set_up_socket(PORT,
              WIZARD_OFFSET,
              "Wizard Port ",
              &wiz_addr,
              &wiz);

set_up_socket(PORT,
              WHO_OFFSET,
              "External who list ",
              &who_addr,
              &who);

set_up_socket(PORT,
              WWW_OFFSET,
              "Small WWW port ",
              &www_addr,
              &www);

/* initialize functions */
init_user_struct();
init_area_struct();

tells = 0;
commands = 1;
says = 0;
says_running = 0;
tells_running = 0;

system_stats.quota              = MAX_NEW_PER_DAY;
system_stats.logins_today       = 0;
system_stats.logins_since_start = 0;
system_stats.new_users_today    = 0;

puts("Checking for out of date messages...");
check_mess(1);
messcount();

/* log startup */
sysud(1,0);
puts("** System running **");

reset_chal(0,"");

/*----------------------------*/
/* dissociate from tty device */
/*----------------------------*/
if (!DEBUG)
  {
   /*-----------------------*/
   /* first fork            */
   /*-----------------------*/

   switch(fork())
      {
        case -1:    puts("FORK FAILED");
	            goto CLEAN_EXIT;
	
        case 0:     setpgrp();
	            break;  /* child becomes server */
	
        default:    goto CLEAN_EXIT;  /* kill parent */
      }

   /*-----------------------*/
   /* close stdin,out & err */
   /*-----------------------*/
   close(0);  /* close stdin,out & err */
   close(1);
   close(2);

   /*-----------------------*/
   /* second fork           */
   /*-----------------------*/

   switch(fork())
      {
        case -1:    puts("FORK FAILED");
	            goto CLEAN_EXIT;
	
        case 0:     setpgrp();
	            break;  /* child becomes server */
	
        default:    goto CLEAN_EXIT;  /* kill parent */
      }

  }

/*--------------------------------------------------------------------------*/
/* set up alarm & signals (which will prob. never be used but to be sure...)*/
/*--------------------------------------------------------------------------*/
reset_alarm();
signal(SIGPIPE, SIG_IGN);
signal(SIGTERM, shutdown_it);

/*------------------------------------------------*/
/* the following signal ignores may be desireable */
/* to comment out for debugging                   */
/*------------------------------------------------*/

/* this section of signal trapping turned off intentionall (to the linux comment)
signal(SIGILL,SIG_IGN);
signal(SIGIOT,SIG_IGN);
signal(SIGBUS,SIG_IGN);
signal(SIGSEGV,SIG_IGN);
signal(SIGTSTP,SIG_IGN);
signal(SIGCONT,SIG_IGN);
signal(SIGHUP,SIG_IGN);
signal(SIGINT,SIG_IGN);
signal(SIGQUIT,SIG_IGN);
signal(SIGABRT,SIG_IGN);
signal(SIGFPE,SIG_IGN);
signal(SIGURG,SIG_IGN);
signal(SIGTTIN,SIG_IGN);
signal(SIGTTOU,SIG_IGN);

signal(SIGEMT,SIG_IGN);        /* linux users comment this line out */

/*-----------------------------------------------------*/
/* clear the btell buffer                              */
/*-----------------------------------------------------*/

cbtbuff();

/*--------------------------*/
/**** main program loop *****/
/*--------------------------*/

LOOP_FOREVER
   {
	noprompt=0;
	signl=0;
	
	/* set up readmask */
	FD_ZERO(&readmask);

	for (user = 0; user<MAX_USERS; ++user)
	  {
	   if ( ustr[user].area == -1   &&
	       !ustr[user].logging_in)
	     continue;
	
	   FD_SET(ustr[user].sock,&readmask);
          }

	FD_SET(ls,&readmask);
	FD_SET(wiz,&readmask);
	FD_SET(who,&readmask);
        FD_SET(www,&readmask);

	/*------------------------------*/
	/* wait for input on the ports  */
	/*------------------------------*/
	
	select(sizeof(readmask)*8, &readmask, 0, 0, 0);
	
	if (signl)  continue;

	/*---------------------------------------*/
	/* check for connection to who socket    */
	/*---------------------------------------*/
	if (FD_ISSET(who,&readmask))
	  {
           if ( ( as = accept(who, &whoi_addr, &size) ) == -1 )
             {
	       /* we can not open a new file descriptor */
	       FD_CLR(who,&readmask);
	       sprintf(mess,"ERROR -> create who socket %d\n",errno);
	       print_to_syslog(mess);
             }
            else
             {
              fcntl(as,F_SETFL,O_NDELAY);       /* set socket to non-blocking */

              external_who(as);

              close(as);
              continue;
             }
           }

	/*--------------------------------------------*/
	/* check for connection to mini www socket    */
	/*--------------------------------------------*/
	if (FD_ISSET(www, &readmask))
	  {
	
           if ( ( as = accept(www, &www_addr, &size) ) == -1 )
             {
	       /* we can not open a new file descriptor */
	       FD_CLR(www,&readmask);
	       sprintf(mess,"ERROR -> create www socket %d\n",errno);
	       print_to_syslog(mess);
             }
            else
             {
              fcntl(as,F_SETFL,O_NDELAY);       /* set socket to non-blocking */

              external_www(as);


              close(as);
              continue;
             }
           }

	/*---------------------------------------*/
	/* check for connection to listen socket */
	/*---------------------------------------*/
	port = ' ';
	
	if (FD_ISSET(ls,&readmask) || FD_ISSET(wiz,&readmask))
	  {
           if ( FD_ISSET(ls,&readmask))
             {
               if ( ( as = accept(ls,&acc_addr,&size) ) == -1 )
                 {
	          /* we can not open a new file descriptor */
	          sprintf(mess,"ERROR ==== accept can't create user socket errno %d\n",errno);
	          print_to_syslog(mess);
	          continue;
                }
              port = '1';
            }

           if ( FD_ISSET(wiz,&readmask))
             {
               if ( ( as = accept(wiz, &acc_addr, &size) ) == -1 )
                 {
	          /* we can not open a new file descriptor */
	          sprintf(mess,"ERROR ==== accept can't create wiz socket errno %d\n",errno);
	          print_to_syslog(mess);
	          continue;
                 }
               port = '2';
             }

           fcntl(as,F_SETFL,O_NDELAY);       /* set socket to non-blocking */

	
	   /*----------------------------------------------*/
	   /* no system access is allowed                  */
	   /*----------------------------------------------*/
	
	   if (!sys_access)
	     {
	       strcpy(mess,SYS_CLOSED);
	       write(as,mess,strlen(mess));
	       close(as);
	       continue;
	     }
			
            if ( (new_user = find_free_slot(port) ) == -1 )
              {
		sprintf(mess,SYS_FULL);
		write(as,mess,strlen(mess));
		close(as);
		continue;
	      }
	
	     ustr[new_user].sock           = as;		
	     ustr[new_user].last_input     = time(0);
	     ustr[new_user].logging_in     = 3;
	     ustr[new_user].attleft        = 3;
	     ustr[new_user].warning_given  = 0;
	     ustr[new_user].afk            = 0;
	     ustr[new_user].attach_port    = port;
	
	     cat( MOTD, new_user, -1);                  /* send message of the day */

	     /*------------------------*/
	     /* get internet site info */
	     /*------------------------*/
	
	     strcpy(ustr[new_user].site, inet_ntoa(acc_addr.sin_addr));
		
             if (resolve_names)
               {
	         ip_address = inet_addr (ustr[new_user].site);
                 net_info = gethostbyaddr ((unsigned long)(&ip_address),
                                                    sizeof(ip_address),
                                                    AF_INET);

                 if (net_info)	
	           strncpy(ustr[new_user].net_name,net_info->h_name,64);
	          else
	           sprintf(ustr[new_user].net_name,SYS_LOOK_FAILED);
	        }
	       else
	        sprintf(ustr[new_user].net_name,SYS_RES_OFF);
		
                	
	     /*---------------------------*/
	     /* Check for restricted site */
	     /*---------------------------*/
	
             if (check_restriction(new_user) == 1)
               {
                write_str(new_user,SYS_NOT_ALLOWED);
                user_quit(new_user);
                continue;
               }

             write_str_nr(new_user,SYS_LOGIN);
	   }

	/** cycle through users **/
	for (user=0; user<MAX_USERS; ++user) {
		area = ustr[user].area;
		
		if (area== -1 && !ustr[user].logging_in) continue;

		/* see if any data on socket else continue */
		if (!FD_ISSET( ustr[user].sock,&readmask)) continue;
		
		
               /*--------------------------------------------*/
               /* read the user input                        */
               /*--------------------------------------------*/

                 inpstr[0]  = 0;
                 inpchar[0] = 0;
                 dest = inpstr;

                 /*-----------------------------------------*/
                 /* see if the user is gone or has input    */
                 /*-----------------------------------------*/

                 if ( !( len = read(ustr[user].sock, inpchar, 1) ) )
                   {
                    user_quit(user);
                    continue;
                   }

                 /*-------------------------------------------*/
                 /* if there is input pending, read it        */
                 /*  (stopping on <cr>, <EOS>, or <EOF>)      */
                 /*-------------------------------------------*/
                 buff_size = 0;
                 while ((inpchar[0] != 10) &&
                        (inpchar[0] != 0)  &&
                        (len != EOF)       &&
                        (buff_size < 8000) )
                   {
                    if (inpchar[0]>0)
                      *dest++ = inpchar[0];

                    inpchar[0]=0;
                    len = read(ustr[user].sock, inpchar, 1);
                    buff_size++;
                   }

                /*----------------------------------------------------*/
                /* some nice users were doing some things that would  */
                /* intentionally kill the system.  This should trap   */
                /* that and report such incidents.                    */
                /*----------------------------------------------------*/

                if (buff_size == 8000)
                  {
                    sprintf(mess,"HACK flood from site %21.21s possibly as %12s\n",
                                  ustr[user].site,
                                  ustr[user].name);
                    print_to_syslog(mess);

                    writeall_str(mess, WIZ_ONLY, -1, 0, -1, BOLD, NONE, 0);

                    if (ustr[user].logging_in)
                      {
                        write_str(user,"----------------------------------------------------------------");
                        write_str(user,"Notice:  You are attempting to use this computer system in a way");
                        write_str(user,"         which is considered a crime under United States federal");
                        write_str(user,"         access laws.  All attempts illegally access this site are ");
                        write_str(user,"         logged.  Repeat violators of this offense will be ");
                        write_str(user,"         prosecuted to the fullest extent of the law.");
                        write_str(user,"----------------------------------------------------------------");

                        /*-----------------------------------------*/
                        /* during logins, auto restrict the site   */
                        /*-----------------------------------------*/

                        sprintf(t_mess,"%s/%s", RESTRICT_DIR, ustr[user].site);
                        strncpy(filename, t_mess, FILE_NAME_LEN);

                        if (!(fp=fopen(filename,"a")))
                          {
                           return;
                          }

                        fputs("Site access denied by auto-restrict/n",fp);
                        FCLOSE(fp);

                        user_quit(user);
                       }
                      else
                       {
                        if (ustr[user].locked == 0)
                          {
                           write_str(user,"Notice: Buffer data has been lost. ");
                           write_str(user,"        Further lose of data will result in connection termination.");
                           ustr[user].locked = 1;
                          }
                         else
                          {
                           write_str(user,"Notice: Connection terminated due to lose of data.\n");
                           user_quit(user);
                          }

                       }

                    continue;
                  }

                /*-------------------------------------*/
                /* terminate the string                */
                /*-------------------------------------*/

                *dest=0;

                /*-----------------------------------------*/
                /* see if there is anything in the string  */
                /*-----------------------------------------*/

                if (!strlen(inpstr) &&
                     inpchar[0] != 10)
                  continue;

                if (inpstr[0] == 0) continue;

                /*------------------------------------------------*/
                /* handle telnet ctrl keys (ex. ctrl-c)           */
                /*------------------------------------------------*/

                if ( ((unsigned char) inpstr[0]) == IAC )
                  {
                   if ( ((unsigned char) inpstr[1]) == IP &&
                        ((unsigned char) inpstr[2]) == IAC &&
                        ((unsigned char) inpstr[3]) == DO &&
                        ((unsigned char) inpstr[4]) == TELOPT_TM )
                          {
                            will_time_mark(user);
                          }
                   continue;  /* Drop the telnet comands */
                  }

		/* misc. operations */
		terminate(user, inpstr);
		
		ustr[user].last_input    = time(0);  /* ie now        */
		ustr[user].warning_given = 0;        /* reset warning */
		
		/*-------------------------------*/
		/* user wakes up                 */
		/*-------------------------------*/
		
		if (ustr[user].afk)
		  {
		    sprintf(mess,"- %s is back -",ustr[user].name);
                    writeall_str(mess,1,user,0,user,NORM,AFK_TYPE,0);
                    ustr[user].afk = 0;
		  }
		
		/*-------------------------------*/
		/* see if user is logging in     */
		/*-------------------------------*/
		if (ustr[user].logging_in)
		  {
		   login(user,inpstr);
		   continue;
		  }

		/*-------------------------------*/
		/* see if user is reading a file */
		/*-------------------------------*/
		if (ustr[user].file_posn)
		  {
		   if (inpstr[0] == 'q' || inpstr[0] == 'Q')
		     {
		      ustr[user].file_posn=0;
		      ustr[user].line_count=0;
		      ustr[user].number_lines=0;
		      continue;
		     }
		
		   cat(ustr[user].page_file,user, 0);
		   continue;
		  }

	       check_macro(user,inpstr);

               /*----------------------------------------------*/
               /* if user did nothing, return                  */
               /*----------------------------------------------*/

	       if (!inpstr[0] || nospeech(inpstr)) continue;

	       /*------------------------*/
	       /* deal with any commands */
	       /*------------------------*/
	       com_num=get_com_num(user,inpstr);
		
		if (com_num == -1 && inpstr[0] == '.')
		  {
		   write_str(user,SYNTAX_ERROR);
		   continue;
		  }
		
		if (com_num != -1)
		  {
		   last_user=user;
		   exec_com(com_num,user,inpstr);
			
		   last_user= -1;
		   continue;
		  }

		/*--------------------------------------------*/
		/* see if input is answer to clear_mail query */
		/*--------------------------------------------*/
		if (ustr[user].clrmail==user && inpstr[0]!='y')
		  {
		   ustr[user].clrmail= -1;
		   continue;
		  }
		
		if (ustr[user].clrmail==user && inpstr[0]=='y')
		  {
		   clear_mail(user, ustr[user].clrmail);
		   ustr[user].clrmail= -1;
		   continue;
		  }

		/*------------------------------------------*/
		/* see if input is answer to shutdown query */
		/*------------------------------------------*/
		
		if (shutd==user && inpstr[0]!='y')
		  {
		    shutd= -1;
		    continue;
		   }
		
		if (shutd==user && inpstr[0]=='y')
		  shutdown_d(user);

		/*-----------------------------------------------------*/
		/* send speech to speaker & everyone else in same area */
		/*-----------------------------------------------------*/
		commands++;
                say(user, inpstr);
	      }
	} /* end while */
	
CLEAN_EXIT:
    return(0);

}

/*------------------------------------------------------------------------*/
/*                       Utility Functions Follow                         */
/*------------------------------------------------------------------------*/

say(user, inpstr)
int user;
char * inpstr;
{
  int area;

  if (!strlen(inpstr))
    {
      write_str(user," [Default blank say action is a review]");
      review(user);
      return;
    }

  says++;
  area = ustr[user].area;

  sprintf(mess,VIS_SAYS,ustr[user].name,inpstr);
  mess[1]=toupper(mess[1]);
  write_str(user,mess);	
		
  if (!ustr[user].vis)
    sprintf(mess,INVIS_SAYS,inpstr);
			
  writeall_str(mess,1,user,0,user,NORM,SAY_TYPE,0);

/*--------------------------------*/
/* store say to the review buffer */
/*--------------------------------*/
  strncpy(conv[area][astr[area].conv_line],mess,MAX_LINE_LEN);
  astr[area].conv_line=(++astr[area].conv_line)%NUM_LINES;	
}

systime(user,dummy)
int user;
char * dummy;
{
int tm;
char stm[10];

time(&tm);
midcpy(ctime(&tm),stm,11,15);
sprintf(mess,TIME_MESSAGE,stm,TIME_ZONE);
write_str(user,mess);
}



/*** put string terminate char. at first char < 32 ***/
terminate(user, str)
int user;
char *str;
{
int u;
int bell = 7;
int tab  = 9;

/*----------------------------------------------------------------*/
/* only allow cntl-g from users rank > 5                          */
/*----------------------------------------------------------------*/

if (ustr[user].super < WIZ_LEVEL) bell = tab;

for (u = 0; u<ARR_SIZE; ++u)
  {
   if ((*(str+u) < 32 &&       /* terminate line on first control char */
       *(str+u) != bell &&     /* except for bell                      */
       *(str+u) != tab) ||     /* and tab                              */
       *(str+u) > 126  )       /* special chars over 126               */
     {
      *(str+u)=0;
      u=ARR_SIZE;
     }
  }
}



/*** convert string to lower case ***/
strtolower(str)
char *str;
{
while(*str)
  {
    *str=tolower(*str);
    str++;
  }
}



/*** check for empty string ***/
nospeech(str)
char *str;
{
while(*str)
  {
    if ((*str>' ') && (*str<=127))
       return(0);
    str++;
  }
return 1;
}



/** read in initialize data **/
read_init_data()
{
char filename[FILE_NAME_LEN],line[80];
char hide[MAX_AREAS+1];
int a;
FILE *fp;

sprintf(t_mess,"%s/%s",datadir,INIT_FILE);
strncpy(filename,t_mess,FILE_NAME_LEN);

if (!(fp=fopen(filename,"r")))
  {
   perror("IFORMS: Cannot access area data files");
   exit(0);
  }

fgets(line,80,fp);

/* read in important system data & do a check of some of it */
sscanf(line,"%d %d %d %d %d %d %s",&PORT,
                                   &NUM_AREAS,
                                   &atmos_on,
                                   &syslog_on,
                                   &MESS_LIFE,
                                   &allow_new,
                                   area_nochange);

syslog_on = 1;
/* if (PORT<IPPORT_RESEREVED || PORT>65535) */

if (PORT<1000 || PORT>65535)
  {
   sprintf(mess,"IFORMS: Bad port number (%d)",PORT);
   perror(mess);
   exit(0);
  }
	
if (NUM_AREAS>MAX_AREAS)
  {
   perror("IFORMS: No. of rooms is too great (26 max allowed)");
   exit(0);
  }

/* read in descriptions and joinings */
for (a=0; a<NUM_AREAS; ++a)
  {
   fgets(line,80,fp);
   hide[0] = 0;
   sscanf(line,"%s %s %s",astr[a].name,astr[a].move,hide);
   if (hide[0]>32) astr[a].hidden=1;
     else
      astr[a].hidden=0;
  }

FCLOSE(fp);
}



/*** init user structure ***/
init_user_struct()
{
int u,v;

puts("Initialising user structure... ");
for (u=0; u<MAX_USERS; ++u)
  {
   ustr[u].area          = -1;
   ustr[u].listen        = 1;
   ustr[u].invite        = -1;
   ustr[u].super         = 0;
   ustr[u].vis           = 1;
   ustr[u].warning_given = 0;
   ustr[u].logging_in    = 0;
   ustr[u].conv_count    = 0;
   ustr[u].cat_mode      = 0;
   ustr[u].rows          = 24;
   ustr[u].cols          = 256;
   ustr[u].car_return    = 0;
   ustr[u].abbrs         = 1;
   ustr[u].white_space   = 1;
   ustr[u].line_count    = 0;
   ustr[u].number_lines  = 0;
   ustr[u].times_on      = 0;
   ustr[u].aver          = 0;
   ustr[u].hilite        = 0;
   ustr[u].new_mail      = 0;
   strcpy(ustr[u].flags, "                        ");

   for (v=0; v<NUM_LINES; v++)
     {
      ustr[u].conv[v][0]=0;
     }
  }
}



/*** init area structure ***/
init_area_struct()
{
int a,n;

puts("Initialising area structure & file pointers... ");
for (a=0;a<NUM_AREAS;++a)
  {
   astr[a].private=0;
   astr[a].conv_line=0;
   for (n=0;n<NUM_LINES;++n)
      conv[a][n][0]=0;
  }
}



/*** count no. of messages (counts no. of newlines in message files) ***/
messcount()
{
char filename[FILE_NAME_LEN];
int a;

puts("Counting messages....");
for(a=0;a<NUM_AREAS;++a) {
	astr[a].mess_num=0;
	sprintf(t_mess,"%s/board%d",MESSDIR,a);
	strncpy(filename,t_mess,FILE_NAME_LEN);
	
        astr[a].mess_num = file_count_lines(filename);
	}
}


/*----------------------------------------------------------------------------------*/
/* Entry:       login                                                               */
/*                                                                                  */
/* Purpose:     a user, make new accounts, and initialize new users                 */
/*                                                                                  */
/* Last Modified:                                                                   */
/*               Deep       Dec. 1, 1993    Rewrote to use new account method and   */
/*                                          log bad login attempts to syslog        */
/*----------------------------------------------------------------------------------*/

login(user,inpstr)
int user;
char *inpstr;
{
  char         name[ARR_SIZE];
  char         passwd[ARR_SIZE];
  char         passwd2[NAME_LEN];
  char         z_mess[100];
  int          f=0;
  int          su;
  int          tm;

  passwd[0]=0;
  passwd2[0]=0;

  /*----------------------------------------------------------*/
  /* if this is the second time the password has been entered */
  /*----------------------------------------------------------*/

  if (ustr[user].logging_in==1)
    goto CHECK_PASS;

  /*-------------------------------------*/
  /* get login name                      */
  /*-------------------------------------*/

  if (ustr[user].logging_in==3)
    {
      name[0]=0;
      sscanf(inpstr,"%s",name);

      if (name[0]<32 || !strlen(name))
        {
	 write_str(user,SYS_LOGIN);  return;
	}
	
      if (strlen(name)<3)
        {
         write_str(user,SYS_NAME_SHORT);
	 attempts(user);
	 return;
	}
	
     if (strlen(name)>NAME_LEN-1)
       {
	write_str(user,SYS_NAME_LONG);
	attempts(user);
	return;
       }
	
	/* see if only letters in login */
     for (f=0; f<strlen(name); ++f)
       {
         if (!isalpha(name[f]) || name[f]<'A' || name[f] >'z')
           {
	     write_str(user,"Only letters are allowed in login name");
	     attempts(user);
	     return;
	   }
       }

     strcpy(ustr[user].login_name,name);
     ustr[user].logging_in=2;

     write_str_nr(user,SYS_PASSWD_PROMPT);
     return;
   }

  /*-------------------------------------*/
  /* get first password                  */
  /*-------------------------------------*/

  if (ustr[user].logging_in==2)
    {
      passwd[0]=0;
      sscanf(inpstr,"%s",passwd);

      if (passwd[0] == 0) return;

      if (passwd[0]<32 || !strlen(passwd))
        {
          write_str(user,"Invalid password.");
	  write_str_nr(user,SYS_PASSWD_PROMPT);
	  return;
        }

      if (strlen(passwd)>NAME_LEN-1)
        {
	  write_str(user,SYS_PASSWD_LONG);
	  write_str_nr(user,SYS_PASSWD_PROMPT);
	  return;
	}
     }

  /*-------------------------------------------------------------*/
  /* convert name & passwd to lowercase and encrypt the password */
  /*-------------------------------------------------------------*/

  strtolower(ustr[user].login_name);
  strtolower(passwd);

  if (strcmp(ustr[user].login_name, passwd) ==0)
    {
       write_str(user," ");
       write_str(user,"Password cannot be the login name.");
       write_str(user,"Invalid login.");
       attempts(user);
       return;
    }

  st_crypt(passwd);
  strcpy(ustr[user].login_pass,passwd);


  /*-------------------------------------------------------------------------*/
  /* check for user and login info                                           */
  /*-------------------------------------------------------------------------*/

  if (read_to_user(ustr[user].login_name,user) )
    {

     /*---------------------------------------------*/
     /* The file exists, so the user has an account */
     /*---------------------------------------------*/
     su = t_ustr.super;

     if ( strcmp(ustr[user].login_pass,ustr[user].password) )
       {
        time(&tm);
        write_str(user,"Incorrect login");
        sprintf(z_mess,"%23s password on %s from %s\n",
                ctime(&tm),
                ustr[user].login_name,
                ustr[user].site);
        ustr[user].area = -1;
        print_to_syslog(z_mess);

        attempts(user);
        return;
       }
      else
       {
        sprintf(z_mess,"Last login from %s on %16.16s",
                      ustr[user].last_site,
                      ustr[user].last_date);
        write_str(user,z_mess);

        strcpy(ustr[user].last_date,  ctime(&tm));
        strcpy(ustr[user].last_site, ustr[user].site);
        add_user(user,su);
        system_stats.logins_today++;
        system_stats.logins_since_start++;
        return;
       }
    }
   else
    {
     /*---------------------------------------------*/
     /* The file does not exists, so the user has   */
     /* no previous account                         */
     /*---------------------------------------------*/

     if (!allow_new)
        {
          write_str(user,"Incorrect login");
  	  attempts(user);
	  return;
         }

     if (system_stats.quota > 0 && system_stats.new_users_today >= system_stats.quota)
       {
         write_str(user,"=====================================================");
         write_str(user,"We are currently using a maximum quota for new users.");
         write_str(user,"The limit for today has been reached.");
         write_str(user,"This will be reset at midnight.");
         write_str(user,"=====================================================");
         attempts(user);
	 return;
       }


     write_str(user,"New user...");
     write_str_nr(user,"Please re-enter password: ");
     strcpy(ustr[user].login_pass,passwd);
     ustr[user].logging_in=1;
     return;
    }


  /*------------------------------------------------------------------------------*/
  /* For new users, double check the password to make sure they entered it right  */
  /* and save the new account if allowed                                          */
  /*------------------------------------------------------------------------------*/

  CHECK_PASS:
     sscanf(inpstr,"%s",passwd);
     st_crypt(passwd);

     if (strcmp(ustr[user].login_pass,passwd))
       {
         write_str(user,"Passwords do not match");
         ustr[user].login_pass[0]=0;
         attempts(user);
         return;
       }

     write_str(user," ");
     write_str(user,"A New account has been created for you.");
     system_stats.logins_today++;
     system_stats.logins_since_start++;
     system_stats.new_users_today++;

     sprintf(z_mess,"NEW user created %s %s - ",ustr[user].name,ustr[user].site);
     print_to_syslog(z_mess);

     strcpy(ustr[user].password,passwd);
     init_user(user,1);
     copy_from_user(user);
     write_user(ustr[user].login_name);

     add_user(user,0);
}



/*** check to see if user has had max login attempts ***/
attempts(user)
int user;
{
if (!--ustr[user].attleft) {
	write_str(user,"Maximum attempts exceeded...");
	user_quit(user);
	return;
	}
ustr[user].logging_in=3;
write_str(user,"Enter login name: ");
}
	


/*** write time system went up or down to syslog file ***/
sysud(ud,user)
int ud; /* ud=1 if sys coming up else 0 */
{
int tm;
char stm[30];


/* set up time string */
time(&tm);
strcpy(stm,ctime(&tm));
stm[strlen(stm)-1]=0;  /* get rid of nl */
strcpy(start_time,stm); /* record boot time */

if (!syslog_on) return;

if (ud) puts("Logging startup...");

/* write to file */
if (ud)  sprintf(mess,"BOOT on %s on port %d using datadir: %s",stm,PORT,datadir);
else {
	ustr[user].name[0]=toupper(ustr[user].name[0]);
	sprintf(mess,"SHUTDOWN by %s on %s\n",ustr[user].name,stm);
	}

print_to_syslog(mess);
}

write_hilite(user,str)
char *str;
int user;
{
char str2[ARR_SIZE];

if (ustr[user].hilite)
  {
   strcpy(str2,str);
   add_hilight(str2);
   write_str(user,str2);
  }
 else
  {
   write_str(user,str);
  }

}

write_hilite_nr(user,str)
char *str;
int user;
{
char str2[ARR_SIZE];

if (ustr[user].hilite)
  {
    strcpy(str2,str);
    add_hilight(str2);
    write_str_nr(user,str2);
  }
 else
  {
   write_str_nr(user,str);
  }

}

/*** write_str sends string down socket ***/
write_str(user,str)
char *str;
int user;
{
char buff[312];
int  stepper;
int  left;

/*--------------------------------------------------------*/
/* pick reasonable range for width                        */
/*--------------------------------------------------------*/

if (ustr[user].cols < 15 || ustr[user].cols > 256)
  stepper = 80;
 else
  stepper = ustr[user].cols;

/*--------------------------------------------------------*/
/* if string length = 0....print line feed                */
/*--------------------------------------------------------*/
left = strlen(str);

if (left == 0 )
  {
   if (ustr[user].car_return && ustr[user].afk<2)
     write(ustr[user].sock, "\r\n", 2);
    else
     write(ustr[user].sock, "\n", 1);
  }

/*--------------------------------------------------------*/
/* print trhough the string in stepper size chucks        */
/*--------------------------------------------------------*/

for(; left > 0; left -= stepper )
  {
   strncpy(buff, str, stepper);

   str  += stepper;
   buff[stepper] = 0;

   if (ustr[user].car_return)
     strcat(buff,"\r\n");
    else
     strcat(buff,"\n");

   if (ustr[user].afk<2)
     write(ustr[user].sock, buff, strlen(buff));
  }

}

write_str_nr(user,str)
char *str;
int user;
{
if (ustr[user].afk<2)
  write(ustr[user].sock,str,strlen(str));
}


/*** finds next free user number ***/
find_free_slot(port)
char port;
{
int u;

/*-------------------------------------------------*/
/* check for full system                           */
/*-------------------------------------------------*/

if ( (port == '1' && num_of_users >= NUM_USERS) ||
     (port == '2' && num_of_users >= MAX_USERS) )
  {
   return -1;
  }

/*-------------------------------------------------*/
/* find a free slot                                */
/*-------------------------------------------------*/

for (u=0;u<MAX_USERS;++u)
  {
   if (ustr[u].area== -1 && !ustr[u].logging_in)
     return u;
  }

return -1;
}



/*----------------------------------------------------------------------------------*/
/* Entry:       add_user                                                            */
/*                                                                                  */
/* Purpose:     initialize a user for the system                                    */
/*                                                                                  */
/* Last Modified:                                                                   */
/*               Deep       Dec. 1, 1993    Changed initialization of users         */
/*----------------------------------------------------------------------------------*/

/*** set up data for new user if he can get on ***/
add_user(user,su)
int user,su;
{
int u,v;
int tm;
char room[32];



/* see if already logged on */
if ( how_many_users(ustr[user].name) > 1)
  {
        strcpy(mess,"You are already signed on - terminating old sessions");
	write(ustr[user].sock,mess,strlen(mess));
	
	for (u=0;u<MAX_USERS;++u)
	  if (!strcmp(ustr[u].name,ustr[user].name) &&
	      ustr[u].area!= -1 &&
	      u != user )
	    {
	      write_str(u,"This session is being terminated");
	      user_quit(u);
	    }
  }


/* reset user structure */
 strcpy(ustr[user].name,ustr[user].login_name);
 copy_from_user(user);
 read_user(ustr[user].name);
 copy_to_user(user);

 if (ustr[user].attach_port == '2' && ustr[user].super <= WIZ_LEVEL)
   {
     write_str(user,"You are not permitted to enter the wizard port.");
     user_quit(user);
     return;
   }

 ustr[user].area=             INIT_ROOM;
 ustr[user].listen=           1;
 ustr[user].shout=            1;
 ustr[user].vis=              1;
 ustr[user].locked=           0;
 ustr[user].suspended=        0;

 ustr[user].igtell=           0;
 ustr[user].clrmail=          -1;
 ustr[user].time=             time(0);
 ustr[user].invite=           -1;
 ustr[user].last_input=       time(0);
 ustr[user].logging_in=       0;
 ustr[user].monitor=          0;
 ustr[user].file_posn=        0;
 ustr[user].warning_given=    0;

 ustr[user].conv_count=       0;
 ustr[user].cat_mode =        0;

 for (v=0; v<NUM_LINES; v++)
     {ustr[user].conv[v][0]=0;}

num_of_users++;


fcntl(ustr[user].sock, F_SETFL, O_NDELAY); /* set socket to non-blocking */

/* send room details and prompt to user */
sprintf(mess,SYS_WELCOME,ustr[user].name,ranks[ustr[user].super]);
write_str(user,mess);
read_to_user(ustr[user].login_name,user);
time(&tm);
strcpy(ustr[user].last_date,  ctime(&tm));
strcpy(ustr[user].last_site, ustr[user].site);

copy_from_user(user);
get_macros(user);
ustr[user].listen= 1;

write_user(ustr[user].login_name);
if (astr[ustr[user].area].private)
  {write_str(user,"You left in a room that is now private, ");
   write_str(user,"so you are entering back in the main room");
   ustr[user].area=INIT_ROOM;
  }

if (ustr[user].super < MIN_HIDE_LEVEL)
  {
    ustr[user].vis=1;
  }

if (ustr[user].vis)
  {write_str_nr(user,"Status: Visible   ");}
  else
  {write_str_nr(user,"Status: InVisible ");}

if (ustr[user].shout)
  {write_str_nr(user,"unmuzzled ");}
  else
  {write_str_nr(user,"muzzled   ");}

if (ustr[user].listen)
  {write_str(user,"listens");}
  else
  {write_str(user,"ignores");}

look(user);
check_mail(user);

if (astr[ustr[user].area ].hidden)
    sprintf(room," ? ");
else
    sprintf(room,"%s",astr[ustr[user].area].name);

/* send message to other users and to file */
if (ustr[user].super >= WIZ_LEVEL + 1)
  {
   sprintf(mess, ANNOUNCEMENT_HI, ustr[user].name, ustr[user].desc,room);
   writeall_str(mess, WIZ_ONLY, user, 0, user, NORM, LOGIO, 0);
  }
else
  {
   sprintf(mess, ANNOUNCEMENT_LO, ustr[user].name, ustr[user].desc,room);
   writeall_str(mess, 0, user, 0, user, NORM, LOGIO, 0);
  }

/* stick signon in file */
syssign(user,1);
}

/*** Load user macros ***/
get_macros(user)
int user;
{
char filename[FILE_NAME_LEN];
FILE *fp;
int i,l;

sprintf(t_mess,"%s/%s",MACRODIR,ustr[user].name);
strncpy(filename,t_mess,FILE_NAME_LEN);

if (!(fp=fopen(filename,"r"))) return;

for (i=0;i<10;++i)
  {
   fgets(ustr[user].macros[i],49,fp);
   l=strlen(ustr[user].macros[i]);
   ustr[user].macros[i][l-1]=0;
  }

FCLOSE(fp);
}



/*** page a file out to user ***/
cat( filename, user, line_num)
int user, line_num;
char *filename;
{
int num_chars=0,lines=0,retval=1;
FILE *fp;
int max_lines = 25;
int line_pos = 0;
int i = 0;
char leader[17];

if (!(fp=fopen(filename,"r")))
  {
   ustr[user].file_posn  = 0;
   ustr[user].line_count = 0;
   return(0);
  }

if (line_num == 1)
  ustr[user].number_lines = 1;


/* jump to reading posn in file */
if (line_num != -1)
  {
    fseek(fp,ustr[user].file_posn,0);
    max_lines = ustr[user].rows;
    line_pos = ustr[user].line_count;
  }
 else
  {
    max_lines = 999;
    line_num = 0;
  }

if (max_lines < 5 || max_lines > 999)
   max_lines = 25;

/* loop until end of file or end of page reached */
mess[0]=0;
fgets(mess, sizeof(mess)-25, fp);

if (!ustr[user].cols) ustr[user].cols=80;

while(!feof(fp) && lines < max_lines)
  {
   line_pos++;

   i = strlen(mess);
   lines      += i / ustr[user].cols + 1;
   num_chars  += i;

   if (ustr[user].number_lines)
     sprintf(leader,"%-4d ",line_pos);
    else
     leader[0]=0;

   mess[i-1] = 0;      /* remove linefeed */
   sprintf(t_mess,"%s%s",leader, mess);

   write_str(user,t_mess);
   fgets(mess, sizeof(mess)-25, fp);
  }

if (user== -1) goto SKIP;

if (feof(fp))
  {
   ustr[user].number_lines = 0;
   ustr[user].file_posn    = 0;
   ustr[user].line_count   = 0;
   noprompt=0;
   retval=2;
  }
else
  {
   /* store file position and file name */
   ustr[user].file_posn += num_chars;
   ustr[user].line_count = line_pos;
   strcpy(ustr[user].page_file,filename);
   write_str(user,"** Press RETURN to continue, Q to stop reading **");
   noprompt=1;
  }

SKIP:
  FCLOSE(fp);
  return retval;
}



/*** get user number using name ***/
get_user_num(i_name,user)
char *i_name;
int user;
{
int u;
int found = 0, last = -1;
char name[50];

strncpy(name,i_name,50);
strtolower(name);
t_mess[0] = 0;

for (u=0;u<MAX_USERS;++u)
	if (!strcmp(ustr[u].name,name) && ustr[u].area!= -1)
	return u;
	
for (u=0;u<MAX_USERS;++u)
  {
   if (instr(0, ustr[u].name, name) != -1)
    {
      strcat(t_mess, ustr[u].name);
      strcat(t_mess, " ");
      found++;
      last= u;
    }
  }

if (found == 0) return(-1);

if (found >1)
  {
   sprintf(mess, "Name was not unique, matched: %s", t_mess);
   write_str(user,mess);
   return(-1);
  }
 else
  return(last);
}

/*** get user number using name ***/
get_user_num_exact(i_name,user)
char *i_name;
int user;
{
int u;
char name[50];

strncpy(name,i_name,50);
strtolower(name);
t_mess[0] = 0;

for (u=0;u<MAX_USERS;++u)
	if (!strcmp(ustr[u].name,name) && ustr[u].area != -1)
	return u;

return(-1);
}

/*** get user number using name ***/
how_many_users(name)
char *name;
{
int u;
int num=0;

for (u=0;u<MAX_USERS;++u)
	if (!strcmp(ustr[u].name,name) && ustr[u].area != -1) num++;
	
return (num);
}



/*** removes first word at front of string and moves rest down ***/
remove_first(inpstr)
char *inpstr;
{
int newpos,oldpos;

newpos=0;  oldpos=0;
/* find first word */
while(inpstr[oldpos]==' ') {
	if (!inpstr[oldpos]) { inpstr[0]=0;  return; }
	oldpos++;
	}
/* find end of first word */
while(inpstr[oldpos]!=' ') {
	if (!inpstr[oldpos]) { inpstr[0]=0;  return; }
	oldpos++;
	}
/* find second word */
while(inpstr[oldpos]==' ') {
	if (!inpstr[oldpos]) { inpstr[0]=0;  return; }
	oldpos++;
	}
while(inpstr[oldpos]!=0)
	inpstr[newpos++]=inpstr[oldpos++];
inpstr[newpos]='\0';
}



/*** sends output to all users if area==0, else only users in same area ***/
/*------------------------------------------------------------------------------*/
/* str - what to print                                                          */
/* area - -1 = login, -5 = wizards only, 0 and above - any room                 */
/* user - the one who did it                                                    */
/* send_to_user = 0 all on system 1 = in room                                   */
/* who_did = user                                                               */
/* mode = normal - 0, bold = 1, colors = 2,3,4,5                                */
/* type = message type                                                          */
/*------------------------------------------------------------------------------*/

writeall_str(str, area, user, send_to_user, who_did, mode, type, sw)
char *str;
int user;
int area;
int send_to_user;
int who_did;
int mode;
int type;
int sw;
{
int u;
char str2[ARR_SIZE];

str2[0]=0;

if (!ustr[who_did].vis ||
     type == ECHO      ||
     type == BCAST     ||
     type == KILL      ||
     type == MOVE      ||
     type == PICTURE   ||
     type == GREET)
  {
   strcpy(str2,"<");
   strcat(str2,ustr[who_did].name);
   strcat(str2,"> ");
  }

strcat(str2,str);

str[0]=toupper(str[0]);

/*---------------------------------------*/
/* added for btell                       */
/*---------------------------------------*/
 str[0]=toupper(str[0]);

 if (area == WIZ_ONLY)
   {
     for (u=0;u<MAX_USERS;++u)
       {
        if (!user_wants_message(u,type)) continue;

	if (ustr[u].super > WIZ_LEVEL && !ustr[u].logging_in && u != user)
	  {
	   if (mode == BOLD)
	     {
	       write_hilite(u,str);
	     }
	    else
	     {
	      write_str(u,str);
	     }
	   }
        }
    return;
   }

/*---------------------------------------*/
/* normal write to all users             */
/*---------------------------------------*/

for (u=0;u<MAX_USERS;++u) {
	if ((!send_to_user && user==u) ||  ustr[u].area== -1) continue;
	
	if (!user_wants_message(u,type)) continue;

	if (ustr[u].area==ustr[user].area || !area)
	  {
	    if(ustr[u].monitor)
	      {
	       if (mode == BOLD)
	         {
	          write_hilite(u,str2);
	         }
	        else
	         {
	          write_str(u,str2);
	         }
	      }
	    else
	      {
	       if (mode == BOLD)
	         {
	          write_hilite(u,str);
	         }
	        else
	         {
	          write_str(u,str);
	         }
	      }
	  }
	}
}


/*** Handle macros... if any ***/
check_macro(user,inpstr)
int user;
char *inpstr;
{
int macnum,i,lng;
char line[ARR_SIZE];
char filename[FILE_NAME_LEN];
FILE *fp;

if (inpstr[0]=='.' && inpstr[1]>='0' && inpstr[1]<='9') {
   midcpy(inpstr,line,3,ARR_SIZE);
   macnum=inpstr[1]-'0';
   if (inpstr[2]=='=') {
      if (strlen(inpstr) > 45) {
         write_str(user,"Sorry, that macro is too long");
         inpstr[0]=0;
         return;
         }
      strcpy(ustr[user].macros[macnum],line);
      inpstr[0]=0;
      sprintf(t_mess,"%s/%s",MACRODIR,ustr[user].name);
      strncpy(filename,t_mess,FILE_NAME_LEN);

      if (!(fp=fopen(filename,"w"))) {
         write_str(user,"Sorry - Can't access your macro file");
         logerror("Cannot access macros files");
         return;
         }
      for (i=0;i<10;++i) {
         sprintf(mess,"%s\n",ustr[user].macros[i]);
         fputs(mess,fp);
         }
      FCLOSE(fp);
      write_str(user,"Ok");
      }
   else {
      lng=inpstr[2];
      strcpy(inpstr,ustr[user].macros[macnum]);
      if (lng) {
         strcat(inpstr," ");
         strcat(inpstr,line);
         }
      }
   }
}







/*** gets number of command entered (if any) ***/
get_com_num(user,inpstr)
int   user;
char *inpstr;
{
char comstr[ARR_SIZE];
char tstr[ARR_SIZE+25];
int f;

if (ustr[user].white_space)
 {
  while(inpstr[0] == ' ') inpstr++;
 }

comstr[0]=inpstr[0];
comstr[1] = 0;

if (!strcmp(comstr,"."))
 {
   sscanf(inpstr,"%s",comstr);
 }
 else
 {
   if (ustr[user].abbrs)
     {
      switch(comstr[0]) {
        case ';':
        case ':':
                  strcpy(comstr,".emote");
                  break;
        case '*':
                  strcpy(comstr,".cbuff");
                  break;
        case '\'':
                  strcpy(comstr,".say");
                  break;
        case '/':
                  strcpy(comstr,".semote");
                  break;
        case '<':
                  strcpy(comstr,".review");
                  break;
        case '{':
                  strcpy(comstr,".who");
                  break;
        case '@':
                  strcpy(comstr,".look");
                  break;
        case ',':
                  strcpy(comstr,".tell");
                  break;
        default:
                  return -1;
                  break;
        }
      tstr[0] = 0;
      inpstr[0] = ' ';
      strcpy(tstr,comstr);
      strcat(tstr,inpstr);
      strcpy(inpstr,tstr);
    }
 }

for (f=0; sys[f].su_com != -1; ++f)
	if (!instr(0,sys[f].command,comstr) && strlen(comstr)>1) return f;
return -1;
}




/*** logs signons and signoffs ***/
syssign(user,onoff)
int user,onoff;
{
int tm;
char stm[30];

/* send data to whofile */
/* write to file */
time(&tm);
strcpy(stm,ctime(&tm));
stm[strlen(stm)-1]=0; /* get rid of nl at end */
sprintf(mess,"%s:%1d: %s (%s :%d)\n",stm,onoff,ustr[user].name,ustr[user].site,ustr[user].sock);
print_to_syslog(mess);
}


/*** log runtime errors in LOGFILE ***/
logerror(s)
char *s;
{
char line[132];
sprintf(line,"ERROR: %s\n",s);
print_to_syslog(line);
}



/**** mid copy copies chunk from string strf to string strt
      (used in write_board & prompt) ***/
midcpy(strf,strt,fr,to)
char *strf,*strt;
int fr,to;
{
int f;
for (f=fr;f<=to;++f)
  {
   if (!strf[f])
     {
      strt[f-fr]='\0';
      return;
     }
   strt[f-fr]=strf[f];
  }
strt[f-fr]='\0';
}



/*** searches string ss for string sf starting at position pos ***/
instr(pos,ss,sf)
int pos;
char *ss,*sf;
{
int f,g;
for (f=pos;*(ss+f);++f) {
	for (g=0;;++g) {
		if (*(sf+g)=='\0' && g>0) return f;
		if (*(sf+g)!=*(ss+f+g)) break;
		}
	}
return -1;
}



/*** Finds number or users in given area ***/
find_num_in_area(area)
int area;
{
int u,num=0;
for (u=0;u<MAX_USERS;++u)
	if (ustr[u].area==area) ++num;
return num;
}


/*** COMMAND FUNCTIONS ***/

/*** Call command function or execute command directly ***/
exec_com(com_num,user,inpstr)
int com_num,user;
char *inpstr;
{
char line[132];

/* see if su command */
if (ustr[user].suspended)
  {
    return;
  }

if (ustr[user].super < sys[com_num].su_com)
  {
   write_str(user,"Sorry - you can't use that command");
   return;
  }

commands++;

remove_first(inpstr);  /* get rid of commmand word */

switch(sys[com_num].jump_vector) {
	case 0 : user_quit(user); break;
	case 1 : t_who(user,inpstr,0); break;
	case 2 : shout(user,inpstr); break;
	case 3 : tell_usr(user,inpstr); break;
	case 4 : user_listen(user,inpstr); break;
	case 5 : user_ignore(user,inpstr); break;
	case 6 : look(user);  break;
	case 7 : go(user,inpstr,0);  break;
	case 8 : room_access(user,1);  break; /* private */
	case 9 : room_access(user,0);  break; /* public */
	case 10: invite_user(user,inpstr);  break;
	case 11: emote(user,inpstr);  break;
	case 12: rooms(user);  break;
	case 13: go(user,inpstr,1);  break;  /* knock */
	case 14: write_board(user,inpstr,0);  break;
	case 15: read_board(user,0,inpstr);  break;
	case 16: wipe_board(user,inpstr,0);  break;
	case 18: set_topic(user,inpstr);  break;
	case 21: kill_user(user,inpstr);  break;
	case 22: shutdown_d(user);  break;
	case 23: search(user,inpstr);  break;
	case 24: review(user);  break;
	case 25: help(user,inpstr);  break;
	case 26: broadcast(user,inpstr);  break;
	case 27: if (!cat(NEWSFILE,user,0))
			write_str(user,"There is no news today");
                        write_str(user,"Ok");
		 break;
		
	case 28: system_status(user, " ");  break;
	case 29: move(user,inpstr);  break;
	case 30: system_access(user,0);
		 sprintf(line,"CLOSED by %s\n",ustr[user].name);
                 write_str(user,"Ok");
		 print_to_syslog(line);
		 btell(user,line);
	         break;  /* close */
	
	case 31: system_access(user,1);
		 sprintf(line,"OPENED by %s\n",ustr[user].name);
                 write_str(user,"Ok");
		 print_to_syslog(line);
		 btell(user,line);
	         break;  /* open */
	case 35: toggle_atmos(user,inpstr); break;
	case 36: echo(user,inpstr);  break;
	case 37: set_desc(user,inpstr);  break;
	case 38: toggle_allow(user,inpstr);break;
        case 40: greet(user,inpstr); break;
	case 41: arrest(user,inpstr); break;
	case 42: cbuff(user); break;
	case 43: own_msg(user,inpstr); break;
	case 44: macros(user); break;
        case 45: read_mail(user); break;
        case 46: send_mail(user,inpstr); break;
        case 47: ustr[user].clrmail= -1;
                 clear_mail(user, inpstr);
                 break;

	case 49: promote(user,inpstr); break;
	case 50: demote(user,inpstr); break;
	case 51: muzzle(user,inpstr); break;
	case 52: unmuzzle(user,inpstr); break;
	case 54: bring(user,inpstr); break;
	case 56: hide(user,inpstr); break;
	case 58: display_ranks(user); break;
	case 59: restrict(user,inpstr); break;
	case 60: unrestrict(user,inpstr); break;
	case 61: igtells(user); break;
	case 62: heartells(user); break;
	case 65: picture(user,inpstr);break;
	case 66: preview(user,inpstr);break;
	case 67: password(user,inpstr);break;
	case 68: roomsec(user,inpstr);break;
	case 69: semote(user,inpstr);break;
	case 70: tog_monitor(user);break;
	case 71: ptell(user,inpstr);break;
	case 72: follow(user,inpstr);break;
	case 73: read_init_data();
	         messcount();
	         write_str(user,"<ok>");
	         break;
	case 74: beep(user,inpstr);break;
	case 75: set_afk(user,inpstr,1);  break;
	case 76: systime(user,inpstr);  break;
	case 77: print_users(user,inpstr);  break;
	case 78: usr_stat(user,inpstr);  break;
	case 79: set(user,inpstr);  break;
	case 80: swho(user,inpstr); break;
	case 82: btell(user,inpstr); break;
	case 83: nuke(user,inpstr); break;
	case 84: cls(user,inpstr); break;
	case 85: fight_another(user,inpstr); break;
	case 86: if (resolve_names)
	           resolve_names_off(user,inpstr);
	          else
	           resolve_names_on(user,inpstr);
	          break;
	
	case 90: say(user,inpstr); break;
	case 91: meter(user, inpstr); break;
	case 95: set_quota(user, inpstr); break;
	case 96: hilight(user, inpstr); break;
	case 97: write_board(user,inpstr,1);  break; /* wiz_note */
	case 98: wipe_board(user,inpstr,1);  break;  /* wiz wipe */
	case 99: set_bafk(user,inpstr);  break;
        case 100: xcomm(user,inpstr);  break;
	case 101: t_who(user,inpstr,1); break;
	case 102: t_who(user,inpstr,2); break;
	default: break;
	}
}




/*** closes socket & does relevant output to other users & files ***/
user_quit(user)
int user;
{
int u,area=ustr[user].area;
int tm,min;

/* see is user has quit befor he logged in */
if (ustr[user].logging_in) {
	close(ustr[user].sock);
	ustr[user].logging_in=0;
        ustr[user].name[0]=0;
        ustr[user].area= -1;
        ustr[user].conv_count = 0;
        ustr[user].rows = 24;
        ustr[user].cols = 256;
        ustr[user].abbrs = 1;
        ustr[user].times_on = 0;
        ustr[user].aver = 0;
        ustr[user].white_space = 1;
        ustr[user].hilite = 0;
        ustr[user].new_mail = 0;
        strcpy(ustr[user].flags, "                        ");
	return;
	}

write_str(user,"\07Signing off...");

time(&tm);
strcpy(ustr[user].last_date,  ctime(&tm));
strcpy(ustr[user].last_site, ustr[user].site);

min=(tm-ustr[user].time)/60;

sprintf(mess,"%s %s \nFrom site %s ",
                                      ustr[user].name,
                                      ustr[user].desc,
                                      ustr[user].last_site);
write_str(user,mess);

sprintf(mess,"%16.16s after %d minutes of use",
                                      ustr[user].last_date,
                                      min);
write_str(user,mess);

close(ustr[user].sock);
if (ustr[user].aver==0)  ustr[user].aver= 1;  /* kind of a fudge factor  always have at least one */

ustr[user].aver = ( (ustr[user].aver * ustr[user].times_on) + min)
                                /
                      (ustr[user].times_on+1);
ustr[user].times_on++;

copy_from_user(user);
write_user(ustr[user].name);

/* send message to other users & conv file */
if (ustr[user].super >= WIZ_LEVEL + 1)
  {
   sprintf(mess,SYS_DEPART_HI,ustr[user].name,ustr[user].desc);
   mess[21]=toupper(mess[21]);
   writeall_str(mess, WIZ_ONLY, user, 0, user, NORM, LOGIO, 0);
  }
else
  {
   sprintf(mess,SYS_DEPART_LO,ustr[user].name,ustr[user].desc);
   writeall_str(mess, 0, user, 0, user, NORM, LOGIO, 0);
  }

if (astr[area].private && find_num_in_area(area) <= PRINUM && area != PRIROOM)
   {
    writeall_str("Room access returned to public", 1, user, 0, user, NORM, NONE, 0);
    astr[area].private=0;
    cbuff(user);
   }

/* store signoff in log file & set some vars. */
num_of_users--;
ustr[user].area       = -1;
syssign(user,0);
ustr[user].name[0]    = 0;
ustr[user].super      = 0;
ustr[user].conv_count = 0;
ustr[user].rows       = 24;
ustr[user].cols       = 256;
ustr[user].abbrs      = 1;
ustr[user].white_space= 1;
ustr[user].times_on   = 0;
ustr[user].aver       = 0;
for (u=0; u<NUM_LINES; u++)
     {ustr[user].conv[u][0]=0;}

ustr[user].macros[0][0]=0;
ustr[user].macros[1][0]=0;
ustr[user].macros[2][0]=0;
ustr[user].macros[3][0]=0;
ustr[user].macros[4][0]=0;
ustr[user].macros[5][0]=0;
ustr[user].macros[6][0]=0;
ustr[user].macros[7][0]=0;
ustr[user].macros[8][0]=0;
ustr[user].macros[9][0]=0;

ustr[user].hilite       = 0;
ustr[user].new_mail     = 0;
strcpy(ustr[user].flags, "                        ");

if (user == fight.first_user || user == fight.second_user) reset_chal(0,"");

}



/*** prints who is on the system to requesting user ***/
t_who(user,inpstr,mode)
int user;
char *inpstr;
int mode;
{
int s,u,v,tm,min,idl,invis=0;
char *list="NY";
char *super=RANKS;
char l,ud[DESC_LEN],un[NAME_LEN],an[NAME_LEN],und[80];
char temp[256];
char i_buff[5];
int  with;


/*-------------------------------------------------------*/
/* process the with command                              */
/*-------------------------------------------------------*/

with = user;
if (mode == 2)
  {
    if(strlen(inpstr) == 0)
      {
       with = ustr[user].area;
      }
     else
      {
       sscanf(inpstr,"%s ",temp);
       strtolower(temp);

       if ((u=get_user_num(temp,user))== -1)
         {
           not_signed_on(user,temp);
           return;
         }
       with = ustr[u].area;
      }
  }


/* display current time */
time(&tm);
sprintf(mess,"Current users on %s",ctime(&tm));
write_str(user,mess);

/* Give Display format */
sprintf(mess,"Room        Name/Description                     Time On - Listening - Idle");
write_str(user,mess);

for (v=0;v<NUM_AREAS;++v)
{
  /* display user list */
  for (u=0;u<MAX_USERS;++u) {
	if (ustr[u].area!= -1 && ustr[u].area == v)
	  {
		if (!ustr[u].vis && ustr[user].super <= WIZ_LEVEL)
	          {
	            invis++;
	            continue;
	          }
			
		min=(tm-ustr[u].time)/60;
		idl=(tm-ustr[u].last_input)/60;
		
		strcpy(un,ustr[u].name);
		strcpy(ud,ustr[u].desc);
		
		strcpy(und,un);
		strcat(und," ");
		strcat(und,ud);
		
		if (!astr[ustr[u].area].hidden)
		  {
		    strcpy(an,astr[ustr[u].area].name);
		  }
		 else
		  {
		    if ((ustr[user].super > 4) && SHOW_HIDDEN)
		      {
		        strcpy(an, "<");
		        strcat(an, astr[ustr[u].area].name);
		        strcat(an, ">");
		      }
		     else
		      {
		       strcpy(an, "        ");
		      }
		  }
		
		l=list[ustr[u].listen];
		
		if (ustr[user].super > WIZ_LEVEL + 1)
		   {
		    s=super[ustr[u].super];
		   }
		  else
		   s=' ';
		
		if (ustr[u].afk == 1)
		  strcpy(i_buff,"AFK ");
		 else
		  if (ustr[u].afk == 2)
		    strcpy(i_buff,"BAFK");
		   else
		    strcpy(i_buff,"Idle");
		
		sprintf(mess,"%-12s%c %-39.39s %-5.5d %c %s %3.3d",an,s,und,min,l,i_buff,idl);
	        mess[14]=toupper(mess[14]);
	
		if (!ustr[u].vis) mess[13]='~';
		
		strncpy(temp,mess,256);
		strtolower(temp);
		
		if (!strlen(inpstr) ||
		     instr(0,mess,inpstr)!= -1 ||
		     instr(0,temp,inpstr)!= -1 ||
		     mode > 0)
		 {
		   if ((mode == 1 && ustr[u].super>WIZ_LEVEL) ||
		        mode == 0 ||
		       (mode == 2 && ustr[u].area == with))
		     {
		      mess[0]=toupper(mess[0]);
		      write_str(user,mess);
		     }
                 }
		}
	}
}

if (invis) {
	sprintf(mess,"There are %d invisible users",invis);
	write_str(user,mess);
	}
	
write_str(user," ");
sprintf(mess,"Total of %d users signed on.",num_of_users);
write_str(user,mess);
write_str(user," ");
}

/*** prints who is on the system to requesting user ***/
external_who(as)
int as;
{
int s,u,tm,min,idl,invis=0;
char *list="NY";
char l,ud[DESC_LEN],un[NAME_LEN],an[NAME_LEN],und[80];
char temp[256];
char i_buff[5];

/*-------------------------------------------------------------------------*/
/* write out title block                                                   */
/*-------------------------------------------------------------------------*/
if (EXT_WHO1) write(as,EXT_WHO1, strlen(EXT_WHO1) );
if (EXT_WHO2) write(as,EXT_WHO2, strlen(EXT_WHO2) );
if (EXT_WHO3) write(as,EXT_WHO3, strlen(EXT_WHO3) );
if (EXT_WHO4) write(as,EXT_WHO4, strlen(EXT_WHO4) );

/* display current time */
time(&tm);
sprintf(mess,"Current users on %s\n",ctime(&tm));
write(as, mess, strlen(mess));

/* Give Display format */
sprintf(mess,"Room        Name/Description                     Time On - Listening - Idle\n");
write(as, mess, strlen(mess));

/* display user list */
for (u=0;u<MAX_USERS;++u) {
	if (ustr[u].area!= -1)
	  {
		if (!ustr[u].vis)
	          {
	            invis++;
	            continue;
	          }
			
		min=(tm-ustr[u].time)/60;
		idl=(tm-ustr[u].last_input)/60;
		
		strcpy(un,ustr[u].name);
		strcpy(ud,ustr[u].desc);
		
		strcpy(und,un);
		strcat(und," ");
		strcat(und,ud);
		
		if (!astr[ustr[u].area].hidden)
		  {
		    strcpy(an,astr[ustr[u].area].name);
		  }
		 else
		  {
		    strcpy(an, "        ");
		  }
		
		l=list[ustr[u].listen];
		
		s=' ';
		
		if (ustr[u].afk == 1)
		  strcpy(i_buff,"AFK ");
		 else
		  if (ustr[u].afk == 2)
		    strcpy(i_buff,"BAFK");
		   else
		    strcpy(i_buff,"Idle");
		
		sprintf(mess,"%-12s%c %-39.39s %-5.5d %c %s %3.3d\n",an,s,und,min,l,i_buff,idl);
	        mess[14]=toupper(mess[14]);
	
		if (!ustr[u].vis) mess[13]='~';
		
		strncpy(temp,mess,256);
		strtolower(temp);
		
		mess[0]=toupper(mess[0]);
		write(as,mess, strlen(mess));
	       }
	}
	
if (invis)
  {
   sprintf(mess,"There are %d invisible users.\n",invis);
   write(as,mess, strlen(mess));
  }
	
write(as, "\n", 1);
sprintf(mess,"Total of %d users signed on.\n",num_of_users);
write(as, mess, strlen(mess) );
write(as, "\n\n", 2);
}

/*** prints who is on the system to requesting user ***/
swho(user,inpstr)
int user;
char *inpstr;
{
int u;

/* Give Display format */
if ( !strlen(inpstr) )
  {
   write_str(user,"----------------------------------------------------------------");
   write_str(user,"User                    site info                               ");
   write_str(user,"----------------------------------------------------------------");
  }

/* display user list */
for (u=0; u<MAX_USERS; ++u)
  {
    if (ustr[u].area != -1)
      {
	sprintf(mess, SYS_SITE_LINE, ustr[u].name,
	                             ustr[u].site,
	                             u, ustr[u].sock,
		                     ustr[u].net_name );

	if (!strlen(inpstr) || instr(0,mess,inpstr) != -1)
	  {
	   mess[0]=toupper(mess[0]);
	   write_str(user,mess);
          }
       }
      else
       if (ustr[u].logging_in)
         {
	   sprintf(mess, SYS_SITE_LINE, " [login] ",
	                                ustr[u].site,
	                                u, ustr[u].sock,
		                        ustr[u].net_name );

	   if (!strlen(inpstr) || instr(0,mess,inpstr) != -1)
	     {
	       mess[0]=toupper(mess[0]);
	       write_str(user,mess);
             }
	  }
  }

write_str(user," ");
}


/*** shout sends speech to all users regardless of area ***/
shout(user,inpstr)
int user;
char *inpstr;
{
if (!ustr[user].shout)
  {
   write_str(user,"You can't shout right now.");
   return;
  }

if (!strlen(inpstr))
  {
    write_str(user,"Shout what?");
    return;
  }

sprintf(mess,"%s shouts: %s",ustr[user].name,inpstr);

if (!ustr[user].vis)
	sprintf(mess,"Someone shouts: %s",inpstr);
	
writeall_str(mess, 0, user, 0, user, NORM, SHOUT, 0);
sprintf(mess,"You shout: %s",inpstr);
write_str(user,mess);
}


/*** tells another user something without anyone else hearing ***/
tell_usr(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u;
char prefix[25];
int pos = ustr[user].conv_count % NUM_LINES;
int f;

if (!strlen(inpstr))
  {
    write_str(user,"Review tells:");

    for (f=0;f<NUM_LINES;++f)
      {
        if ( strlen( ustr[user].conv[pos] ) )
         {
	  write_str(user,ustr[user].conv[pos]);
	 }
	pos = ++pos % NUM_LINES;
      }

    write_str(user,"<Done>");
    return(0);
  }

tells++;

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

remove_first(inpstr);
if ((u=get_user_num(other_user,user))== -1)
  {
   not_signed_on(user,other_user);
   return(0);
  }

if (ustr[u].afk)
  {
    if (ustr[u].afk == 1)
      sprintf(t_mess,"- %s is Away From Keyboard -",ustr[u].name);
     else
      sprintf(t_mess,"- %s is blanked AFK (is not seeing this) -",ustr[u].name);

    write_str(user,t_mess);
  }

other_user[0]=toupper(other_user[0]);

if (ustr[u].igtell && ustr[user].super<WIZ_LEVEL + 1)
  {
   sprintf(mess,"%s is ignoring tells",other_user);
   write_str(user,mess);
   return(0);
  }

if (!ustr[u].listen && ustr[user].super<WIZ_LEVEL + 1)
  {
   sprintf(mess,"%s is not listening",other_user);
   write_str(user,mess);
   return(0);
  }

if (ustr[u].monitor)
  {
    strcpy(prefix,"<");
    strcat(prefix,ustr[user].name);
    strcat(prefix,"> ");
  }
 else
  {
   prefix[0]=0;
  }

/* write to user being told */
if (ustr[user].vis)
  {
    sprintf(mess," -->[%s]: %s",ustr[user].name,inpstr);
    mess[5]=toupper(mess[5]);
  }
 else
  {
  if (ustr[u].monitor)
    {
      sprintf(prefix,"? %s",ustr[user].name);
    }
   else
    {
     strcpy(prefix,"?");
    }
   sprintf(mess," -->[%s]: %s",prefix,inpstr);
  }
/*-----------------------------------*/
/* store the semote in the rev buffer*/
/*-----------------------------------*/

strncpy(ustr[u].conv[ustr[u].conv_count],mess,MAX_LINE_LEN);
ustr[u].conv_count = ( ++ustr[u].conv_count ) % NUM_LINES;

write_hilite(u,mess);

/* write to teller */
sprintf(mess," --> to %s: %s",ustr[u].name,inpstr);
/* mess[9]=toupper(mess[9]); */
write_str(user,mess);

strncpy(ustr[user].conv[ustr[user].conv_count],mess,MAX_LINE_LEN);
ustr[user].conv_count = ( ++ustr[user].conv_count ) % NUM_LINES;
return(0);
}

/*** tells another user something without anyone else hearing ***/
beep(user,inpstr)
int user;
char *inpstr;
{

if (!strlen(inpstr))
  {
    write_str(user,"Beep who?");
    return;
  }

strcat(inpstr," \07 \07*Beep*\07");
tell_usr(user,inpstr);

}



/*** not signed on - subsid func ***/
not_signed_on(user,name)
int user;
char *name;
{
sprintf(mess,"%s is not signed on",name);
mess[0]=toupper(mess[0]);
write_str(user,mess);
return;
}


/*** look decribes the surrounding scene **/
look(user)
int user;
{
int f;
int u;
int area;
int occupied=0;
char filename[FILE_NAME_LEN];

area=ustr[user].area;
write_str(user,"+-------------------------------------------+");
if (astr[area].hidden)
    sprintf(mess,"| You are in the secure room %2.2d             |",area);
else
    sprintf(mess,"| You are in the %-20.20s       |",astr[area].name);
write_str(user,mess);
write_str(user,"+-------------------------------------------+");

/* open and read room description file */
sprintf(t_mess,"%s/%s",datadir,astr[area].name);
strncpy(filename,t_mess,FILE_NAME_LEN);

cat(filename,user,0);

/* show exits from room */
write_str_nr(user,"\nYou can go to the : ");
for (f = 0; f < strlen( astr[area].move ); ++f)
  {
   if (!astr[astr[area].move[f]-'A'].hidden)
      {
       write_str_nr(user,astr[ astr[area].move[f]-'A' ].name);
       write_str_nr(user,"  ");
      }
  }
	
write_str(user,"");
for (u=0; u<MAX_USERS; ++u)
  {
   if (ustr[u].area != area || u == user || !ustr[u].vis)
     continue;

   if (!occupied)
     write_str(user,"You can see:");

   sprintf(mess,"      %s %s",ustr[u].name,ustr[u].desc);
   mess[6]=toupper(mess[6]);
   write_str(user,mess);
   occupied++;	
  }
write_str(user," ");

if (!occupied)
  write_str(user,"The room is empty");

write_str_nr(user,"\nThe room is set to ");
if ( astr[ustr[user].area].private )
  write_str_nr (user,"private");
 else
  write_str_nr(user,"public");
	
sprintf(mess," and there are %d messages ",astr[area].mess_num);
write_str(user,mess);

if (!strlen(astr[area].topic))
  write_str(user,"There is no current topic here");
 else
  {
   sprintf(mess,"Current topic is : %s",astr[area].topic);
   write_str(user,mess);
  }
}



/*** go moves user into different room ***/

go(user, inpstr, user_knock)
int      user;
int      user_knock;
char    *inpstr;
{
int f;
int new_area;
int teleport=0;
int area=ustr[user].area;
int found = 0;
char room_char;
char room_name[ARR_SIZE];
char entmess[80];

if (!strlen(inpstr))
  {
   if (user_knock)
     {
      write_str(user,"Knock where?");
      return;
     }
    else
     {
      write_str(user,"*** warp to main room ***");
      new_area = INIT_ROOM;
      teleport = 1;
     }
  }
 else
  {
   sscanf(inpstr,"%s ",room_name);

   /*--------------------*/
   /* see if area exists */
   /*--------------------*/

   found = FALSE;
   for (new_area=0; new_area < NUM_AREAS; ++new_area)
    {
     if (! instr(0, astr[new_area].name, room_name) )
       {
         found = TRUE;
         break;
       }
    }

   if (!found)
     {
      write_str(user,"There is no such room");
      return;
     }
  }

/*----------------------------------------------*/
/* check to see if the user is in that room     */
/*----------------------------------------------*/

if (ustr[user].area == new_area)
  {
    write_str(user,"You are in that room now!");
    return;
  }

/*----------------------------------------------*/
/* check for secure room                        */
/*----------------------------------------------*/

if (astr[new_area].hidden && ustr[user].security[new_area] <= ' ')
  {
   write_str(user,"There is no such room");
   return;
  }

/*-----------------------------------------------*/
/* see if user can get to area from current area */
/*-----------------------------------------------*/

room_char = new_area + 'A';                /* get char. repr. of room to move to */
strcpy(entmess,"walks in");

/*------------------------------------------*/
/* see if new room is joined to current one */
/*------------------------------------------*/

found = FALSE;
for (f=0; f<strlen(astr[area].move); ++f)
 {
  if ( astr[area].move[f] == room_char )
    {
     found = TRUE;
     break;
    }
 }

/*--------------------------------------------------------------*/
/* anyone above a 3 can teleport to non-connected rooms         */
/*--------------------------------------------------------------*/

if (!found)
  {
    if (ustr[user].super > WIZ_LEVEL - 1)
      {
        strcpy(entmess,"materializes in a blinding flash.");
        teleport=1;
        found = TRUE;
      }
  }


if (!found)
  {
   write_str(user,"That room is not adjoined to here");
   return;
  }

/*-----------------------------------------------------------*/
/* check for a user knock                                    */
/*-----------------------------------------------------------*/
if (user_knock)
  {
   knock(user,new_area);
   return;
  }

/*-----------------------------------------------------------*/
/* if the room is private abort move...inform user           */
/*-----------------------------------------------------------*/

if (astr[new_area].private && ustr[user].invite != new_area )
  {
   write_str(user,"Sorry - that room is currently private");
   return;
  }

/*--------------------------------------------------------------------*/
/* if an area is hidden and the person is trying to get to it without */
/* permission, tell them it does not exist                            */
/*--------------------------------------------------------------------*/
	
if (astr[new_area].hidden && ustr[user].security[new_area] <= ' ')
     {
      write_str(user,"There is no such room"); /* ok...it is kind of a lie */
      return;
     }
	
/* record movement */
if (teleport || astr[new_area].hidden)
  sprintf(mess,"%s dematerializes and disappears!",ustr[user].name);
 else
  sprintf(mess,"%s goes to the %s",ustr[user].name,astr[new_area].name);

mess[0]=toupper(mess[0]);

/* send output to old room & to conv file */
if (!ustr[user].vis)
	sprintf(mess,"Something is shimmering in here");
	
writeall_str(mess, 1, user, 0, user, NORM, NONE, 0);

/*-----------------------------------------------------------*/
/* return room to public     (if needed)                     */
/*-----------------------------------------------------------*/

if (astr[area].private &&
    find_num_in_area(area) <= PRINUM &&
    area != PRIROOM)
  {
   writeall_str("Room access returned to public", 1, user, 0, user, NORM, NONE, 0);
   cbuff(user);
   astr[area].private=0;
  }

/* record movement */
sprintf(mess,"%s %s",ustr[user].name,entmess);
mess[0]=toupper(mess[0]);

/* send output to new room */
if (!ustr[user].vis)
	sprintf(mess,"Something is shimmering in here");
	
ustr[user].area = new_area;

writeall_str(mess, 1, user, 0, user, NORM, NONE, 0);

/* deal with user */
if (ustr[user].invite == new_area)
  ustr[user].invite= -1;

look(user);
}



/*** knock - subsid func of go ***/
knock(user,new_area)
int user,new_area;
{
int temp;

if (!astr[new_area].private)
  {
   write_str(user,"That room is public anyway");
   return;
  }

write_str(user,"You knock on the door");
sprintf(mess,"%s knocks on the door",ustr[user].name);
if (!ustr[user].vis) sprintf(mess,"Someone knocks on the door");

/* swap user area 'cos of way output func. works */
temp=ustr[user].area;
ustr[user].area=new_area;
writeall_str(mess, 1, user, 0, user, NORM, KNOCK, 0);
ustr[user].area=temp;

/* send message to users in current room */
sprintf(mess,"%s knocks on the %s door",ustr[user].name,astr[new_area].name);
writeall_str(mess, 1, user, 0, user, NORM, KNOCK, 0);
}



/*** room_access sets room to private or public ***/
room_access(user,priv)
int user,priv;
{
int f,area=ustr[user].area;
char *noset="This rooms access cannot be set";
char pripub[2][8];
int spys = 0,u;

strcpy(pripub[0],"public");
strcpy(pripub[1],"private");

/* see if areas access can be set (PRIROOM is always private) */
if (area==PRIROOM)
  {
    write_str(user,noset);
    return;
  }

for (f = 0; f < strlen(area_nochange); ++f)
  {
   if (area_nochange[f] == area+65)
     {
       write_str(user,noset);
       return;
      }
  }

/* see if access already set to user request */
if (priv==astr[area].private)
  {
   sprintf(mess,"The room is already %s!",pripub[priv]);
   write_str(user,mess);
   return;
  }

/* set to public */
if (!priv)
  {
   write_str(user,"Room now set to public");
   sprintf(mess,"%s has set the room to public",ustr[user].name);

   if (!ustr[user].vis)
     sprintf(mess,"Someone has set the room to public");

   writeall_str(mess, 1, user, 0, user, NORM, NONE, 0);
	
   cbuff(user);
   astr[area].private=0;
   return;
  }

/* need at least PRINUM people to set room to private unless u r superuser */
if ( find_num_in_area(area) < PRINUM && ustr[user].super < PRIV_ROOM_RANK)
  {
   sprintf(mess,"You need at least %d people in the room",PRINUM);
   write_str(user,mess);
   return;
  };

write_str(user,"Room now set to private");

for (u=0; u<MAX_USERS; ++u)
 {
   if (ustr[u].area == area && !ustr[u].vis) spys++;
 }

sprintf(mess,"%s has set the room to private",ustr[user].name);

if (!ustr[user].vis)
   sprintf(mess,"Someone has set the room to private");

writeall_str(mess, 1, user, 0, user, NORM, NONE, 0);

if (spys)
  {
    sprintf(mess,"*** NOTE: There are %d invisible users in this room. ***", spys);
    writeall_str(mess, 1, user, 0, user, BOLD, NONE, 0);
    write_str(user,mess);
  }

astr[area].private=1;
}



/*** invite someone into private room ***/
invite_user(user,inpstr)
int user;
char *inpstr;
{
int u,area=ustr[user].area;
char other_user[ARR_SIZE];

if (!astr[area].private) {
	write_str(user,"The area is public anyway");  return;
	}
if (!strlen(inpstr)) {
	write_str(user,"Invite who?");  return;
	}
sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

/* see if other user exists */
if ((u=get_user_num(other_user,user))== -1) {
	not_signed_on(user,other_user);  return;
	}

if (!strcmp(other_user,ustr[user].name)) {
	write_str(user,"You cannot invite yourself!");  return;
	}
if (ustr[u].area==ustr[user].area) {
	sprintf(mess,"%s is already in the room!",ustr[u].name);
	mess[0]=toupper(mess[0]);
	write_str(user,mess);
	return;
	}
write_str(user,"Ok");
sprintf(mess,"%s has invited you to the %s",ustr[user].name,astr[area].name);
if (!ustr[user].vis)
	sprintf(mess,"Someone has invited you to the %s",astr[area].name);
mess[0]=toupper(mess[0]);
write_str(u,mess);
ustr[u].invite=area;
}



/*** emote func used for expressing emotional or visual stuff ***/
emote(user,inpstr)
int user;
char *inpstr;
{
int area;

if (!strlen(inpstr))
  {
   write_str(user,"Emote what?");
   return;
  }

if (!ustr[user].vis)
  sprintf(mess,"Someone %s",inpstr);
 else
  sprintf(mess,"%s %s",ustr[user].name,inpstr);

mess[0]=toupper(mess[0]);

/* write output */

write_str(user,mess);
writeall_str(mess, 1, user, 0, user, NORM, SAY_TYPE, 0);

/*-----------------------------------*/
/* store the emote in the rev buffer */
/*-----------------------------------*/

area = ustr[user].area;
strncpy(conv[area][astr[area].conv_line],mess,MAX_LINE_LEN);
astr[area].conv_line = ( ++astr[area].conv_line ) % NUM_LINES;
	
}

/*** emote func used for expressing emotional or visual stuff ***/
semote(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u;
char prefix[25];

if (!strlen(inpstr))
  {
   write_str(user,"Secret emote who what?");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

remove_first(inpstr);

if ((u=get_user_num(other_user,user))== -1)
  {
   not_signed_on(user,other_user);
   return;
  }

other_user[0]=toupper(other_user[0]);

if (ustr[u].igtell && ustr[user].super<WIZ_LEVEL + 1)
  {
   sprintf(mess,"%s is ignoring tells and secret emotes",other_user);
   write_str(user,mess);
   return;
  }

if (!ustr[u].listen && ustr[user].super<WIZ_LEVEL + 1)
  {
   sprintf(mess,"%s is not listening",other_user);
   write_str(user,mess);
   return;
  }

if (!strlen(inpstr))
  {
   sprintf(mess,"Secret emote %s what?",other_user);
   write_str(user,mess);
   return;
  }

if (ustr[u].monitor)
  {
    strcpy(prefix,"<");
    strcat(prefix,ustr[user].name);
    strcat(prefix,"> ");
  }
 else
  {
   prefix[0]=0;
  }


/* write to user being told */
if (ustr[user].vis)
	sprintf(mess," --> %s %s", ustr[user].name, inpstr);
else sprintf(mess,"%s --> Someone %s",prefix,inpstr);
write_hilite(u,mess);

/*-----------------------------------*/
/* store the semote in the rev buffer*/
/*-----------------------------------*/

strncpy(ustr[u].conv[ustr[u].conv_count],mess,MAX_LINE_LEN);
ustr[u].conv_count = ( ++ustr[u].conv_count ) % NUM_LINES;
	
/* write to teller */
sprintf(mess,"Secret emote %s: %s",ustr[u].name,inpstr);
write_str(user,mess);


}



/*** gives current status of rooms */
rooms(user)
int user;
{
int area;
int totl_hide;
int i,j=1;
char pripub[2][8];
char cbe[3];

strcpy(pripub[0],"public");
strcpy(pripub[1],"private");
strcpy(cbe," ");

totl_hide = 0;
write_str(user,"------------------------------------------------------------------------------");
write_str(user,"Occupied Rooms:             Usrs  Msgs  Topic");
write_str(user,"------------------------------------------------------------------------------");
for (area=0;area<NUM_AREAS;++area)
  {
   i = find_num_in_area(area);

   if (strchr(area_nochange, (char) area+65)== NULL)
     cbe[0]=' ';
    else
     cbe[0]='*';

   sprintf(mess,"%s %-15s : %-7s : %2d : %3d : ",  cbe,
                                                  astr[area].name,
                                                  pripub[astr[area].private],
                                                  i,astr[area].mess_num);

   if (!strlen(astr[area].topic))
     strcat(mess,"<no topic>");
   else
     strcat(mess,astr[area].topic);

   mess[0]=toupper(mess[0]);

   if (!astr[area].hidden )
     {
      if ( i ) write_str(user,mess);
     }
    else
     totl_hide++;
  }

write_str(user,"");
write_str(user,"------------------------------------------------------------------------------");
write_str(user,"Other Rooms: (with number of messages)");
write_str(user,"------------------------------------------------------------------------------");

for (area=0;area<NUM_AREAS;++area)
  {
   i = find_num_in_area(area);

   if (strchr(area_nochange, (char) area+65) == NULL)
     cbe[0]=' ';
    else
     cbe[0]='*';

   sprintf(mess,"%15s(%s%3d) ",astr[area].name,cbe,astr[area].mess_num);
   mess[0]=toupper(mess[0]);

   if (!astr[area].hidden)
     {
      if (!i)
        {
         write_str_nr(user,mess);
	 if (!(j++%3) )
	   {j = 1;
	    write_str(user,"");
	   }
	}
     }
  }
write_str(user," ");
write_str(user," ");

sprintf(mess,"Currently there are %d rooms",NUM_AREAS - totl_hide);
write_str(user,mess);
}



/*** save message to room message board file ***/
write_board(user,inpstr,mode)
int user;
char *inpstr;
int mode;
{
FILE *fp;
char stm[20],filename[FILE_NAME_LEN],name[NAME_LEN];
int tm;

/* process wiz notes */
if (mode == 1)
  {
   if (!strlen(inpstr))
     {
      read_board(user,1,"");
      return;
     }
   sprintf(t_mess,"%s/wizmess",MESSDIR);
  }
 else
  {
   sprintf(t_mess,"%s/board%d",MESSDIR,ustr[user].area);
  }

if (!strlen(inpstr))
  {
   write_str(user,"You forgot the message"); return;
  }

/* open board file */
strncpy(filename,t_mess,FILE_NAME_LEN);

if (!(fp=fopen(filename,"a")))
  {
   sprintf(mess,"%s : message cannot be written",syserror);
   write_str(user,mess);
   sprintf(mess,"Can't open %s message board file to write in write_board()",
                astr[ustr[user].area].name);
   logerror(mess);
   return;
  }

/* write message - alter nums. in midcpy to suit */
time(&tm);
midcpy(ctime(&tm),stm,4,15);
strcpy(name,ustr[user].name);
 /* if (!ustr[user].vis)  strcpy(name,"someone"); */
sprintf(mess,"(%s) From %s: %s\n",stm,name,inpstr);
fputs(mess,fp);
FCLOSE(fp);

/* send output */
write_str(user,"You write the message on the board");
if (mode == 0)
  {
   sprintf(mess,"%s writes a message on the board",ustr[user].name);

   if (!ustr[user].vis)
	sprintf(mess,"A ghostly hand writes a message on the board");

   writeall_str(mess, 1, user, 0, user, NORM, MESSAGE, 0);
   astr[ustr[user].area].mess_num++;
  }
 else
  {
   sprintf(mess,"added a wiz note.",ustr[user].name);
   btell(user,mess);
  }
}


/*** read the message board ***/
read_board(user,mode,inpstr)
int user;
int mode;
char *inpstr;
{
char filename[FILE_NAME_LEN];
int number = 0;
int area, found, new_area;
/* send output to user */
area = ustr[user].area;

if (mode==1)
 {
  sprintf(mess, "*** The wizards note board (important info only, non-wipeable) ***");
  sprintf(t_mess,"%s/wizmess",MESSDIR);
 }
else
 {
  if (astr[area].hidden && ustr[user].super <= WIZ_LEVEL)
    {
     write_str(user,"Secured message board, read not allowed.");
     return;
    }

  if (astr[area].hidden)
    sprintf(mess,"** A secured message board **");
   else
    sprintf(mess,"** The %s message board **",astr[area].name);

   if (strlen(inpstr))
     {
      found = FALSE;
      for (new_area=0; new_area < NUM_AREAS; ++new_area)
       {
        if (! instr(0, astr[new_area].name, inpstr) )
          {
            found = TRUE;
            area = new_area;
            sprintf(mess,"***Reading message board in %s***",astr[new_area].name);
            break;
          }
       }

      if (!found)
        {
         write_str(user,"There is no such room");
         return;
        }
     }

   sprintf(t_mess,"%s/board%d",MESSDIR,area);
 }
strncpy(filename,t_mess,FILE_NAME_LEN);

write_str(user," ");
write_str(user,mess);
write_str(user," ");

if (ustr[user].super > WIZ_LEVEL) number = 1;


if (!cat( filename, user, number) )
   write_str(user,"There are no messages on the board");

}



/*** wipe board (erase file) ***/
wipe_board(user,inpstr,wizard)
int user;
char *inpstr;
int wizard;
{
char filename[FILE_NAME_LEN];
FILE *bfp;
int lower=-1;
int upper=-1;
int mode=0;

if (wizard==0)
  {
   sprintf(t_mess,"%s/board%d",MESSDIR,ustr[user].area);
  }
 else
  {
   write_str(user,"***Wizard Note Wipe***");
   sprintf(t_mess,"%s/wizmess",MESSDIR);
  }

strncpy(filename,t_mess,FILE_NAME_LEN);

/*---------------------------------------------*/
/* check if there is any mail                  */
/*---------------------------------------------*/

if (!(bfp=fopen(filename,"r")))
  {
   write_str(user,"There are no messages to wipe off the board.");
   return;
  }
FCLOSE(bfp);

/*---------------------------------------------*/
/* get the delete parameters                   */
/*---------------------------------------------*/

get_bounds_to_delete(inpstr, &lower, &upper, &mode);

if (upper == -1 && lower == -1)
  {
   write_str(user,"No messages wiped.  Specification of what to ");
   write_str(user,"wipe did not make sense.  Type: .help wipe ");
   write_str(user,"for detailed instructions on use. ");
   return;
  }

   switch(mode)
    {
     case 0: return;
             break;

     case 1:
            sprintf(mess,"Wiped all messages.");
            upper = -1;
            lower = -1;
            break;

     case 2:
            sprintf(mess,"Wiped line %d.", lower);

            break;

     case 3:
            sprintf(mess,"Wiped from line %d to the end.",lower);
            break;

     case 4:
            sprintf(mess,"Wiped from begining of board to line %d.",upper);
            break;

     case 5:
            sprintf(mess,"Wiped all except lines %d to %d.",upper, lower);
            break;

     default: return;
              break;
    }


remove_lines_from_file(user,
                       filename,
                       lower,
                       upper);

write_str(user,mess);
if (wizard == 0)
  {
   astr[ustr[user].area].mess_num = file_count_lines(filename);
   if (!astr[ustr[user].area].mess_num)  unlink(filename);
  }
 else
  {
    if (!file_count_lines(filename)) unlink(filename);
  }
}

/*** sets room topic ***/
set_topic(user,inpstr)
int user;
char *inpstr;
{
if (!strlen(inpstr))
  {
   if (!strlen(astr[ustr[user].area].topic))
     {
      write_str(user,"There is no current topic here");
      return;
     }
    else
     {
      sprintf(mess,"Current topic is : %s",astr[ustr[user].area].topic);
      write_str(user,mess);
      return;
     }
  }
	
if (strlen(inpstr)>TOPIC_LEN)
  {
   write_str(user,"Topic description is too long");
   return;
  }

strcpy(astr[ustr[user].area].topic,inpstr);

/* send output to users */
sprintf(mess,"Topic set to %s",inpstr);
write_str(user,mess);

if (!ustr[user].vis)
  sprintf(mess,"Someone set the topic to %s",inpstr);
 else
  sprintf(mess,"%s has set the topic to %s",ustr[user].name,inpstr);

writeall_str(mess, 1, user, 0, user, NORM, TOPIC, 0);
}

/*** force annoying user to quit prog ***/
kill_user(user,inpstr)
int user;
char *inpstr;
{
char name[ARR_SIZE];
char line[132];

int victim;

if (!strlen(inpstr))
  {
   write_str(user,"Kill who?");
   return;
  }
	
sscanf(inpstr,"%s ",name);
strtolower(name);

if ((victim=get_user_num(name,user)) == -1)
  {
   not_signed_on(user,name);
   return;
  }
name[0]=toupper(name[0]);

/* can't kill master user */
if (ustr[user].super<=ustr[victim].super)
   {
    write_str(user,"That wouldn't be wise....");
    sprintf(mess,"%s actually tried to blast you, HAH!",ustr[user].name);
    mess[0]=toupper(mess[0]);
    write_str(victim,mess);
    return;
   }

sprintf(line,"KILL %s performed by %s\n",ustr[victim].name, ustr[user].name);
print_to_syslog(line);
btell(user,line);

/* kill user */
sprintf(mess, kill_text[ rand() % NUM_KILL_MESSAGES ], name);
writeall_str(mess, 1, victim, 1, user, NORM, KILL, 0);

write_str(victim,"You have been vaporized ...   HAVE A NICE DAY.");
user_quit(victim);

write_str(user,"Ok");
}



/*** shutdown talk server ***/
shutdown_d(user)
int user;
{
int u;
signal(SIGILL,SIG_IGN);
signal(SIGTRAP,SIG_IGN);
signal(SIGIOT,SIG_IGN);
signal(SIGBUS,SIG_IGN);
signal(SIGSEGV,SIG_IGN);
signal(SIGTSTP,SIG_IGN);
signal(SIGCONT,SIG_IGN);
signal(SIGHUP,SIG_IGN);
signal(SIGINT,SIG_IGN);
signal(SIGQUIT,SIG_IGN);
signal(SIGABRT,SIG_IGN);
signal(SIGFPE,SIG_IGN);
signal(SIGTERM,SIG_IGN);
signal(SIGURG,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGTTIN,SIG_IGN);
signal(SIGTTOU,SIG_IGN);

if (shutd== -1)
  {
   write_str(user,"\nAre you sure about this (y/n)? ");
   shutd=user;
   noprompt=1;
   return;
  }

write_str(user,"Quitting users...");

for (u=0;u<MAX_USERS;++u)
  {
   if (ustr[u].area == -1 && !ustr[u].logging_in) continue;
   if (u == user) continue;
   write_str(u, " ");
   write_str(u,"*** System shutting down ***");
   write_str(u, " ");
   user_quit(u);
   }

write_str(user,"Logging shutdown...");
sysud(0,user);

write_str(user,"Now quitting you...");
write_str(user,"* System is now off. *");

strtolower(ustr[user].name);

user_quit(user);

/* close listen socket */
close(ls);
close(wiz);
close(who);
exit(0);
}

shutdown_it()
{
int u;
signal(SIGILL,SIG_IGN);
signal(SIGTRAP,SIG_IGN);
signal(SIGIOT,SIG_IGN);
signal(SIGBUS,SIG_IGN);
signal(SIGSEGV,SIG_IGN);
signal(SIGTSTP,SIG_IGN);
signal(SIGCONT,SIG_IGN);
signal(SIGHUP,SIG_IGN);
signal(SIGINT,SIG_IGN);
signal(SIGQUIT,SIG_IGN);
signal(SIGABRT,SIG_IGN);
signal(SIGFPE,SIG_IGN);
signal(SIGTERM,SIG_IGN);
signal(SIGURG,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGTTIN,SIG_IGN);
signal(SIGTTOU,SIG_IGN);


for (u=0;u<MAX_USERS;++u)
  {
   if (ustr[u].area == -1 && !ustr[u].logging_in) continue;
   write_str(u, " ");
   write_str(u,"*** System shutting down ***");
   write_str(u, " ");
   user_quit(u);
  }

print_to_syslog("SHUTDOWN due to job kill\n");


/* close listen socket */
close(ls);
close(wiz);
close(who);

exit(0);
}

/*** search for specific word in the message files ***/
search(user,inpstr)
int user;
char *inpstr;
{
int b,occured=0;
char word[ARR_SIZE],filename[FILE_NAME_LEN],line[ARR_SIZE];
FILE *fp;

if (!strlen(inpstr))
  {
    write_str(user,"Search for what?");
    return;
  }

sscanf(inpstr,"%s ",word);

/* look through boards */
for (b=0;b<NUM_AREAS;++b) {
	sprintf(t_mess,"%s/board%d",MESSDIR,b);
	strncpy(filename,t_mess,FILE_NAME_LEN);

	if (!(fp=fopen(filename,"r"))) continue;
	fgets(line,300,fp);
	while(!feof(fp)) {
		if (instr(0,line,word)== -1) goto NEXT;
		sprintf(mess,"%s : %s",astr[b].name,line);
		mess[0]=toupper(mess[0]);
		if (!astr[b].hidden)
		  {
		   write_str(user,mess);	
		   ++occured;
		  }
		NEXT:
		fgets(line,300,fp);
		}
	FCLOSE(fp);
	}
if (!occured) write_str(user,"No occurences found");
else {
        write_str(user," ");
	sprintf(mess,"%d occurences found",occured);
	write_str(user,mess);
	}
}



/*** review last five lines of conversation in room ***/
review(user)
int user;
{
int area=ustr[user].area;
int pos=astr[area].conv_line % NUM_LINES;
int f;

write_str(user,"Review conversation:");

for (f = 0; f < NUM_LINES; ++f)
    {
     if (strlen(conv[area][pos]) )
       {
        write_str(user,conv[area][pos]);
       }
     pos = ++pos % NUM_LINES;
    }

write_str(user,"<Done>");
}



/*** help function ***/
help(user,inpstr)
int user;
char *inpstr;
{
int c,nl=0,d;
char filename[FILE_NAME_LEN];
char *super=RANKS;

/* help for one command */
if (strlen(inpstr)) {
	sprintf(t_mess,"%s/%s",HELPDIR,inpstr);
	strncpy(filename,t_mess,FILE_NAME_LEN);

	if (strstr(inpstr,"/")) {
	   sprintf(mess,"User %s attempted to .he %s",ustr[user].name,inpstr);
           logerror(mess);
           return;
           }
	if (!cat(filename,user,0))
		write_str(user,"Sorry - there is no help on that command at the moment");
	return;
	}

/* general help */
write_str(user,"Remember - all commands start with a '.' and can be abbreviated");
sprintf(mess,"Currently these are the commands: ");
write_str(user,mess);

nl = -1;

for (d=0;d<ustr[user].super+1;d++)
  {

    if (nl!= -1)
      {
       write_str(user," ");
      }

    sprintf(mess,"%c)",super[d]);
    write_str_nr(user,mess);
    nl=0;

    for (c=0; sys[c].su_com != -1 ;++c)
      {
        sprintf(mess,"%-11.11s",sys[c].command);
	mess[0]=' ';
	if (d!=sys[c].su_com) continue;
	if (nl== -1)
	  {write_str_nr(user, "  ");
	   nl=0;
	  }
	write_str_nr(user,mess);
	++nl;
	if (nl==5)
	  {
	    write_str(user," ");
	    nl= -1;
	  }
       }
   }
write_str(user," ");
write_str(user," ");
write_str(user,"For further help type  .help <command>");
}



/*** broadcast message to everyone without the "X shouts:" bit ***/
broadcast(user,inpstr)
int user;
char *inpstr;
{
if (!strlen(inpstr)) {
	write_str(user,"Broadcast what?");  return;
	}
sprintf(mess,"*** %s ***",inpstr);
writeall_str(mess, 0, user, 1, user, NORM, BCAST, 0);
}



/*** give system status ***/
system_status(user,inpstr)
int user;
char *inpstr;
{
char onoff[2][4];
char clop[2][7];
char newuser[2][11];

strcpy(onoff[0],"OFF");
strcpy(onoff[1],"ON");
strcpy(clop[0],"CLOSED");
strcpy(clop[1],"OPEN");
strcpy(newuser[0],"DISALLOWED");
strcpy(newuser[1],"ALLOWED");
write_str(user,"+------------------------SYSTEM STATUS----------------------+");
write_str(user,"| Overall:                                                  |");
sprintf(mess,  "|        System started: %24.24s           |",start_time);
write_str(user,mess);
write_str(user,"|                                                           |");
write_str(user,"|        Atmos    Open    New users    Max Users            |");
sprintf(mess,  "|          %1d       %1d          %1d           %3.3d               |",
                                            atmos_on, sys_access, allow_new, MAX_USERS);
write_str(user,mess);

if (atmos_on)
  {
   write_str(user,"|                                                           |");
   write_str(user,"|         Atmos:  Cycle Time    Factor    Count   Last      |");
   sprintf(mess,  "|                    %4d       %4d     %4d      %3d      |",
                                    ATMOS_RESET, ATMOS_FACTOR, ATMOS_COUNTDOWN, ATMOS_LAST);
   write_str(user,mess);
  }

write_str(user,"|                                                           |");

write_str(user,"|        New quota     New today       Message Life         |");
sprintf(mess,  "|           %3.3d            %3.3d           %3.3d  days          |",
                                            system_stats.quota,  system_stats.new_users_today,MESS_LIFE );

write_str(user,mess);
write_str(user,"|                                                           |");

write_str(user,"|        Logins Total     Logins Today                      |");
sprintf(mess,  "|         %10.10d         %6.6d                         |",
                                             system_stats.logins_since_start, system_stats.logins_today);
write_str(user,mess);

write_str(user,"+-----------------------------------------------------------+");
}



/*** move user somewhere else ***/
move(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE],area_name[260];
int area,user2;

/* check user */
if (!strlen(inpstr))
  {
   user2 = user;
   area = INIT_ROOM;
   write_str(user,"*** Warp to main room ***");
   goto FOUND;
  }

sscanf(inpstr,"%s %s",other_user,area_name);

user2 = get_user_num(other_user,user);
if ( user2 == -1)
  {
   not_signed_on(user,other_user);
   return;
  }

/* see if user is moving himself */
if (user==user2)
  {
   write_str(user,"What do you want to do that for?");
   return;
  }

/* see if user to be moved is superior */
if (ustr[user].super < ustr[user2].super)
  {
   write_str(user,"Hmm... inadvisable");
   sprintf(mess,"%s thought about moving you",ustr[user].name);
   mess[0]=toupper(mess[0]);
   write_str(user2,mess);
   return;
  }
	
/* check area */
remove_first(inpstr);
if (!strlen(inpstr))
  {
   area = INIT_ROOM;
   sprintf(mess,"%s moved to %s",ustr[user2].name, astr[area].name);
   write_str(user,mess);
   goto FOUND;
  }

for (area=0;area<NUM_AREAS;++area)
	if (!strcmp(astr[area].name,area_name)) goto FOUND;
		
write_str(user,"There is no such room");
return;

FOUND:
if (area==ustr[user2].area)
  {
   sprintf(mess,"%s is already in that room!",ustr[user2].name);
   mess[0]=toupper(mess[0]);
   write_str(user,mess);
   return;
  }
	
/** send output **/
write_str(user2,"A transporter locks on to you and takes you away!!");

/** to old area */
sprintf(mess,"%s is enveloped by a Transporter Beam and disappears!",ustr[user2].name);
writeall_str(mess, 1, user2, 0, user, NORM, MOVE, 0);

if (find_num_in_area(ustr[user2].area) <= PRINUM &&
    astr[ustr[user2].area].private &&
    ustr[user2].area!=PRIROOM)
  {
    writeall_str("Room access returned to public", 1, user2, 0, user, NORM, NONE, 0);
    astr[ustr[user2].area].private=0;
  }

ustr[user2].area=area;
look(user2);

/* to new area */
sprintf(mess,"%s appears out of a Transporter Beam!",ustr[user2].name);
writeall_str(mess, 1, user2, 0, user, NORM, MOVE, 0);

write_str(user,"Ok");
}



/*** set system access to allow or disallow further logins ***/
system_access(user,co)
int user,co;
{
char line[132];

if (!co) {
        sprintf(line,"CLOSED BY %s\n",ustr[user].name);
        print_to_syslog(line);
	writeall_str("*** System is now closed to further logins ***", 0, user, 1, user, BOLD, NONE, 0);
	sys_access=0;
	return;
	}
	
sprintf(line,"OPENED BY %s\n",ustr[user].name);
print_to_syslog(line);
writeall_str("*** System is now open to further logins ***", 0, user, 1, user, BOLD, NONE, 0);
sys_access=1;
}



/*** echo function writes straight text to screen ***/
echo(user,inpstr)
char *inpstr;
{
char fword[ARR_SIZE];
char *err="Sorry - you can't echo that";
int u=0;
int area;

if (!strlen(inpstr))
  {
   write_str(user,"Echo what?");
   return;
  }

/* get first word & check it for illegal words */
sscanf(inpstr,"%s",fword);
if (
    instr(0,fword,"SYSTEM") != -1||
    instr(0,fword,"Someone") != -1||
    instr(0,fword,"-->") != -1)
    {
     write_str(user,err);
     return;
    }

/* check for user names */
strtolower(fword);
for (u=0;u<MAX_USERS;++u) {
	if (instr(0,fword,ustr[u].name)!= -1) {
		write_str(user,err);  return;
		}
	}
/* write message */
strcpy(mess,inpstr);
mess[0]=toupper(mess[0]);
write_str(user,mess);
writeall_str(mess, 1, user, 0, user, NORM, ECHO, 0);

/*-----------------------------------*/
/* store the echo in the rev buffer  */
/*-----------------------------------*/

area = ustr[user].area;
strncpy(conv[area][astr[area].conv_line],mess,MAX_LINE_LEN);
astr[area].conv_line = ( ++astr[area].conv_line ) % NUM_LINES;

}



/*** set user description ***/
set_desc(user,inpstr)
int user;
char *inpstr;
{

if (!strlen(inpstr))
  {
   sprintf(mess,"Your description is : %s",ustr[user].desc);
   write_str(user,mess);
   return;
  }

if (strlen(inpstr) > DESC_LEN-1)
  {
    write_str(user,"Description too long");
    return;
  }

strcpy(ustr[user].desc,inpstr);
copy_from_user(user);
write_user(ustr[user].name);
sprintf(mess,"New desc: %s",ustr[user].desc);
write_str(user,mess);

}

/*** print out greeting in large letters ***/
greet(user,inpstr)
int user;
char *inpstr;
{
char pbuff[256];
int slen,lc,c,i,j;

slen = strlen(inpstr);
if (!slen)
  {
   write_str(user,"Greet whom?");
   return;
  }

if (slen>10) slen=10;

write_str(user," ");
writeall_str(" ", 1, user, 0, user, NORM, GREET, 0);

for (i=0; i<5; ++i)
  {
   pbuff[0] = '\0';
   for (c=0; c<slen; ++c)
     {
      lc = tolower(inpstr[c]) - 'a';
      if (lc >= 0 && lc < 27)
        {
         for (j=0;j<5;++j)
           {
            if(biglet[lc][i][j])
              strcat(pbuff,"#");
             else
              strcat(pbuff," ");
           }
         strcat(pbuff,"  ");
        }
      }
   /*if (i!=3) strcat(pbuff,"  ##  ##");*/
   sprintf(mess,"%s",pbuff);
   write_str(user,mess);
   writeall_str(mess, 1, user, 0, user, NORM, GREET, 0);
  }
write_str(user," ");
writeall_str(" ", 1, user, 0, user, NORM, GREET, 0);
}


/** Place a user under arrest, move him to brig **/
arrest(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE],area_name[260];
int area,user2;

/* check for user to move */
if (!strlen(inpstr))
  {
   write_str(user,"Arrest whom?");
   return;
  }

sscanf(inpstr,"%s",other_user);
if ((user2=get_user_num(other_user,user))== -1)
  {
    not_signed_on(user,other_user);
    return;
  }

/* User cannot arrest himself */
if (user==user2)
  {
   write_str(user,"What do you want to arrest yourself for?");
   return;
  }

/* See if arrest target user is an SU */
if (ustr[user].super < ustr[user2].super)
  {
   write_str(user,"That would not be wise");
   sprintf(mess,"%s thought of placing you under arrest.",ustr[user].name);
   mess[0]=toupper(mess[0]);
   write_str(user2,mess);
   return;
  }

/* Define target area */
sprintf(area_name,"%s","brig");
for (area=0; area<NUM_AREAS; ++area)
  {
   if (!strcmp(astr[area].name,area_name)) goto FOUND;
  }

write_str(user,"Unexpected Error: Brig Not Found");
return;

FOUND:
if (area==ustr[user2].area)
  {
   sprintf(mess,"%s is already under arrest!",ustr[user2].name);
   mess[0]=toupper(mess[0]);
   write_str(user,mess);
   return;
  }

/** Send output **/
write_str(user2,"An officer has just placed you under arrest!!");

/* to old area */
sprintf(mess,"%s is placed under arrest and will do some time in jail.",ustr[user2].name);
writeall_str(mess, 1, user2, 0, user, NORM, NONE, 0);

muzzle(user, mess);

if (find_num_in_area(ustr[user2].area)<=PRINUM &&
    astr[ustr[user2].area].private && ustr[user2].area != PRIROOM)
  {
   writeall_str("Room access returned to public", 1, user2, 0, user, NORM, NONE, 0);
   astr[ustr[user2].area].private=0;
  }

ustr[user2].area=area;
look(user2);

/* to brig area */
sprintf(mess,"A security team brings %s in!!",ustr[user2].name);
writeall_str(mess, 1, user2, 0, user, NORM, NONE, 0);
sprintf(mess, "ARREST: %s by %s",ustr[user2].name,ustr[user].name);
btell(user,mess);
print_to_syslog(mess);

write_str(user,"Ok");
}


/** Clear the conversation buffer in the user's room **/
cbuff(user)
int user;
{
int i,area=ustr[user].area;

for (i=0;i<NUM_LINES;++i) conv[area][i][0]=0;
write_str(user,"Conversation buffer cleared!");
}


/** Clear the conversation buffer in the user's room **/
cbtbuff()
{
int i;

for (i=0;i<NUM_LINES;++i)
  bt_conv[i][0]=0;

bt_count = 0;

}


/* Status of Owner message */
own_msg(user,inpstr)
int user;
char *inpstr;
{

/* Sun users:  here is the other spot you need to change the   */
/*             ROOT_ID"" to "some_id"                          */

if (strcmp(ustr[user].name,ROOT_ID)) {
   sprintf(mess,"orca is %s",owner_mes);
   write_str(user,mess);
   return;
   }

if (!strlen(inpstr)) {
   sprintf(mess,"%s",owner_mes);
   write_str(user,mess);
   return;
   }

strcpy(owner_mes,inpstr);
sprintf(mess,"Your new owner status message is:");
write_str(user,mess);
sprintf(mess,"%s",owner_mes);
write_str(user,mess);
}


/*** Display user macros ***/
macros(user)
int user;
{
int m;
write_str(user,"Your current macros:");
for (m=0;m<10;++m) {
   sprintf(mess,"  %d=%s",m,ustr[user].macros[m]);
   write_str(user,mess);
   }
}


/*** Read Mail ***/
read_mail(user)
int user;
{
char filename[FILE_NAME_LEN];

/* Send output to user */
sprintf(t_mess,"%s/%s",MAILDIR,ustr[user].name);
strncpy(filename,t_mess,FILE_NAME_LEN);

sprintf(mess,"\n** Your Private Mail Console **");
write_str(user,mess);

if (!cat(filename,user,1))
  {
   write_str(user,"You don't have any mail waiting");
   if (ustr[user].new_mail)
     {
       write_str(user,"");
       write_str(user,"   ** your new mail was deleted due to inactivity, sorry **");
     }
  }

ustr[user].new_mail = FALSE;

copy_from_user(user);
write_user(ustr[user].name);

}

/*-----------------------------------------------------------*/
/* Send mail routing                                         */
/*-----------------------------------------------------------*/
send_mail(user,inpstr)
int user;
char *inpstr;
{
FILE *fp;
char stm[20],filename[FILE_NAME_LEN],name[NAME_LEN];
int tm;
char other_user[ARR_SIZE];
int u;

/*-------------------------------------------------------*/
/* check for any input                                   */
/*-------------------------------------------------------*/

if (!strlen(inpstr))
  {
   write_str(user,"Who do you want to mail?");
   return;
  }

/*-------------------------------------------------------*/
/* get the other user name                               */
/*-------------------------------------------------------*/

sscanf(inpstr,"%s ",other_user);
CHECK_NAME(other_user);
strtolower(other_user);
remove_first(inpstr);

/*-------------------------------------------------------*/
/* check to see if a message was supplied                */
/*-------------------------------------------------------*/

if (!strlen(inpstr))
  {
   write_str(user,"You have not specified a message");
   return;
  }

if (!read_user(other_user))
  {
   sprintf(mess,"User %s does not exist on this system.",other_user);
   write_str(user,mess);
   return;
  }

/*--------------------------------------------------*/
/* set a new mail flag for that other user          */
/*--------------------------------------------------*/

t_ustr.new_mail = TRUE;
write_user(other_user);

/*--------------------------------------------------*/
/* prepare message to be sent                       */
/*--------------------------------------------------*/
time(&tm);
midcpy(ctime(&tm),stm,4,15);
strcpy(name,ustr[user].name);
sprintf(mess,"(%s) From %s: %s\n",stm,name,inpstr);

sprintf(t_mess,"%s/%s",MAILDIR,other_user);
strncpy(filename,t_mess,FILE_NAME_LEN);

/*---------------------------------------------------*/
/* write mail message                                */
/*---------------------------------------------------*/

if (!(fp=fopen(filename,"a")))
  {
   sprintf(mess,"%s : message cannot be written\n", syserror);
   write_str(user,mess);
   return;
  }
fputs(mess,fp);
FCLOSE(fp);

/*-------------------------------------------------------*/
/* write users to inform them of transaction             */
/*-------------------------------------------------------*/

sprintf(mess,"You write %s a message",other_user);
write_str(user,mess);
if ((u=get_user_num(other_user,user))!= -1)
  {
   sprintf(mess,"\n== You have mail waiting ==");
   write_str(u,mess);
   ustr[u].new_mail = TRUE;
  }
}


/*** Clear Mail ***/
clear_mail(user, inpstr)
int user;
char *inpstr;
{
char filename[FILE_NAME_LEN];
FILE *bfp;
int lower=-1;
int upper=-1;
int mode=0;

/*---------------------------------------------*/
/* check if there is any mail                  */
/*---------------------------------------------*/

sprintf(t_mess,"%s/%s",MAILDIR,ustr[user].name);
strncpy(filename,t_mess,FILE_NAME_LEN);

if (!(bfp=fopen(filename,"r")))
  {
   write_str(user,"You have no mail.");
   return;
  }
FCLOSE(bfp);

/* remove the mail file */
if (ustr[user].clrmail== -1)
  {
   /*---------------------------------------------*/
   /* get the delete parameters                   */
   /*---------------------------------------------*/

   get_bounds_to_delete(inpstr, &lower, &upper, &mode);

   if (upper == -1 && lower == -1)
     {
      write_str(user,"No mail deleted.  Specification of what to ");
      write_str(user,"delete did not make sense.  Type: .help cmail ");
      write_str(user,"for detailed instructions on use. ");
      return;
     }

   switch(mode)
    {
     case 0: return;
             break;

     case 1:
            sprintf(mess,"Cmail: Delete all mail messages? ");
            upper = -1;
            lower = -1;
            break;

     case 2:
            sprintf(mess,"Cmail: Delete line %d? ", lower);

            break;

     case 3:
            sprintf(mess,"Cmail: Delete from line %d to the end?",lower);
            break;

     case 4:
            sprintf(mess,"Cmail: Delete from begining to line %d?",upper);
            break;

     case 5:
            sprintf(mess,"Cmail: Delete all except lines %d to %d?",upper, lower);
            break;

     default: return;
              break;
    }

   ustr[user].lower = lower;
   ustr[user].upper = upper;

   ustr[user].clrmail=user;
   noprompt=1;
   write_str(user,mess);
   write_str_nr(user,"Do you wish to do this? (y/n) ");
   return;
  }

remove_lines_from_file(user,
                       filename,
                       ustr[user].lower,
                       ustr[user].upper);

sprintf(mess,"You deleted specified mail messages.");
write_str(user,mess);

if (!file_count_lines(filename))  unlink(filename);

}


check_mail(user)
int user;
{
struct stat stbuf;
char filename[FILE_NAME_LEN], datestr[24];

sprintf(t_mess,"%s/%s",MAILDIR,ustr[user].name);
strncpy(filename,t_mess,FILE_NAME_LEN);
if (stat(filename, &stbuf) == -1)
  {
   return;
  }

if (ustr[user].new_mail)
  {
   write_str(user,"__   __          _");
   write_str(user,"\\ \\ / /__ _  _  | |_  __ ___ _____");
   write_str(user," \\ V / _ \\ || | | ' \\/ _` \\ V / -_|");
   write_str(user,"  |_|\\___/\\_,_| |_||_\\__,_|\\_/\\___|");
   write_str(user,"                           _              _ _");
   write_str(user," _  _ _ _  _ _ ___ __ _ __| |  _ __  __ _(_) |");
   write_str(user,"| || | ' \\| '_/ -_) _` / _` | | '  \\/ _` | | |");
   write_str(user," \\_,_|_||_|_| \\___\\__,_\\__,_| |_|_|_\\__,_|_|_|");
  }

strcpy(datestr,ctime(&stbuf.st_mtime));

sprintf(mess,"==> last mail access was %s",datestr);
write_str(user,mess);

}


/*------------------------------------------------------------------------*/
/* promote a user                                                         */
/*------------------------------------------------------------------------*/

promote(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u, new_level;

if (!strlen(inpstr))
  {
   write_str(user,"Promote who?");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

CHECK_NAME(other_user);

if (!read_user(other_user))
  {
   write_str(user,NO_USER_STR);
   return;
  }

remove_first(inpstr);

new_level = t_ustr.super;

sscanf(inpstr,"%d", &new_level);

if (new_level < 0)
  {
   new_level = 0;
  }

if (new_level > MAX_LEVEL)
  {
   write_str(user,"That person is being set to the highest level in the system.");
   new_level = MAX_LEVEL;
  }
 else
  if (new_level == MAX_LEVEL)
   {
    write_str(user,"That person is already the highest level in the system.");
    return;
   }

if (ustr[user].super < new_level)
  {
    sprintf(mess,"Can't promote %s: That is beyond your authority",other_user);
    write_str(user,mess);
    return;
  }

if (new_level == t_ustr.super) new_level++;

if (ustr[user].super == new_level &&
    PROMOTE_TO_SAME == FALSE)
    {
      sprintf(mess,"Can't promote %s: That is beyond your authority",other_user);
      write_str(user,mess);
      return;
    }

t_ustr.super = new_level;
write_user(other_user);

sprintf(mess,"PROMOTION%d by %s for %s\n", t_ustr.super, ustr[user].name, other_user);
print_to_syslog(mess);

if((u=get_user_num_exact(other_user,user))>-1)
  {
   ustr[u].super=new_level;
   sprintf(mess,"%s has promoted you to %s",ustr[user].name,ranks[ustr[u].super]);
   mess[0]=toupper(mess[0]);
   write_str(u,mess);
  }

sprintf(mess,"You have promoted %s to %s",other_user,ranks[t_ustr.super]);
write_str(user,mess);
}

demote(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u;
char z_mess[132];

if (!strlen(inpstr))
  {
   write_str(user,"Demote who?");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

if (!read_user(other_user))
  {
   write_str(user,NO_USER_STR);
   return;
  }

if (t_ustr.super == 0)
  {
    sprintf(z_mess,"Can't demote %s: Bottom ranked already",other_user);
    write_str(user,z_mess);
    return;
  }

if(t_ustr.super>ustr[user].super)
  {
    sprintf(z_mess,"Can't demote %s,they hold rank over you.",other_user);
    write_str(user,z_mess);
    return;
  }

t_ustr.super--;
write_user(other_user);
sprintf(z_mess,"DEMOTION%d by %s for %s\n", t_ustr.super, ustr[user].name, other_user);
print_to_syslog(z_mess);

if ((u=get_user_num_exact(other_user,user))>-1)
  {
    ustr[u].super--;
    sprintf(z_mess,"%s has demoted you to %s",ustr[user].name,ranks[ustr[u].super]);
    z_mess[0]=toupper(z_mess[0]);
    write_str(u,z_mess);
   }

sprintf(z_mess,"You have demoted %s to %s",other_user,ranks[t_ustr.super]);
write_str(user,z_mess);
}


/** Muzzle a user, takes away his .shout capability **/
muzzle(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u;

if (!strlen(inpstr))
  {
   write_str(user,"Users Currently Muzzled and logged on");
   write_str(user,"-------------------------------------");
   for (u=0; u<MAX_USERS; ++u)
    {
     if (ustr[u].shout == 0 && ustr[u].area > -1)
       {
        write_str(user, ustr[u].name);
       }
    }
   write_str(user,"(end of list)");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

if ((u=get_user_num(other_user,user))== -1)
  {
   not_signed_on(user,other_user);
   return;
  }
if (u == user)
  {
   write_str(user,"You are definitly wierd! Trying to muzzle yourself, geesh.");
   return;
  }


if (ustr[user].super < ustr[u].super)
  {
   write_str(user,"That would not be wise...");
   sprintf(mess,"%s wanted to muzzle you!",ustr[user].name);
   mess[0]=toupper(mess[0]);
   write_str(u,mess);
   return;
  }

ustr[u].shout=0;
sprintf(mess,"%s can't shout anymore",ustr[u].name);
mess[0]=toupper(mess[0]);
writeall_str(mess, 1, u, 1, user, NORM, NONE, 0);
write_str(u,"You are no longer allowed to shout");

sprintf(mess,"MUZZLE: %s by %s\n",ustr[u].name, ustr[user].name);
print_to_syslog(mess);
btell(user, mess);

write_str(user,"Ok");
}


/** Unmuzzle a muzzled user, so they can shout again **/
unmuzzle(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u;

if (!strlen(inpstr))
  {
   write_str(user,"Users Currently Unmuzzled and logged on");
   write_str(user,"---------------------------------------");
   for (u=0;u<MAX_USERS;++u)
    {
     if (ustr[u].shout && ustr[u].area > -1)
       {
        write_str(user,ustr[u].name);
       };
    }
   write_str(user,"(end of list)");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

if ((u=get_user_num(other_user,user))== -1)
  {
   not_signed_on(user,other_user);
   return;
  }

if (ustr[user].super < ustr[u].super)
  {
   write_str(user,"Why do you want to do that?");
   return;
  }

if (ustr[u].shout)
  {
   sprintf(mess,"%s is not muzzled",ustr[u].name);
   write_str(user,mess);
   mess[0]=toupper(mess[0]);
   return;
  }

if (u == user && ustr[u].super < WIZ_LEVEL + 2)
  {
   write_str(user,"Silly user, think it would be that simple.");
   return;
  }

ustr[u].shout=1;
sprintf(mess,"%s can shout again",ustr[u].name);
mess[0]=toupper(mess[0]);
writeall_str(mess, 1, u, 1, user, NORM, NONE, 0);
write_str(u,"You are again allowed to shout");

sprintf(mess,"UNMUZZLE: %s by %s\n",ustr[u].name, ustr[user].name);
print_to_syslog(mess);
btell(user, mess);

write_str(user,"Ok");
}

bring(user,inpstr)
int user;
char *inpstr;
{
int user2,area=ustr[user].area;
char other_user[ARR_SIZE];

if (!strlen(inpstr)) {
   write_str(user,"Bring who?");
   return;
   }

sscanf(inpstr,"%s",other_user);
if ((user2=get_user_num(other_user,user))== -1) {
   not_signed_on(user,other_user);
   return;
   }

if (user==user2) {
   write_str(user,"You can't bring yourself!");
   return;
   }

/* Can't bring a master user */
if (ustr[user2].super>ustr[user].super) {
   write_str(user,"Hmm... inadvisable");
   sprintf(mess,"%s thought about bringing you to the %s",ustr[user].name,astr[ustr[user].area].name);
   mess[0]=toupper(mess[0]);
   write_str(user2,mess);
   return;
   }

if (area==ustr[user2].area) {
   sprintf(mess,"%s is already in this room!",ustr[user2].name);
   mess[0]=toupper(mess[0]);
   write_str(user,mess);
   return;
   }

/** send output **/
write_str(user2,"\nA transporter locks on to you and takes you away!!");

/** to old area **/
sprintf(mess,"%s is enveloped by a Transporter Beam and disappears!",ustr[user2].name);
writeall_str(mess, 1, user2, 0, user, NORM, MOVE, 0);
if (find_num_in_area(ustr[user2].area)<=PRINUM && astr[ustr[user2].area].private && ustr[user2].area!=PRIROOM) {
   writeall_str("Room access returned to public", 1, user2, 0, user, NORM, NONE, 0);
   astr[ustr[user2].area].private=0;
   }
ustr[user2].area=area;
look(user2);

/* To new area */
sprintf(mess,"%s appears out of a Transporter Beam!",ustr[user2].name);
writeall_str(mess, 1, user2, 0, user, NORM, MOVE, 0);
write_str(user,"Ok");
}

hide(user,inpstr)
int user;
char *inpstr;
{
char name[ARR_SIZE];
int victim;
char str2[ARR_SIZE];



if (!strlen(inpstr))
  {
   victim=user;
  }
 else
  {
   sscanf(inpstr,"%s",name);
   strtolower(name);
   if ((victim=get_user_num(name,user))== -1)
     {
      not_signed_on(user,name);
      return;
     }
  }

if (ustr[victim].super < MIN_HIDE_LEVEL)
  {
    write_str(user,"Cannot use hide on that person");
    ustr[victim].vis=1;
    return;
  }

if (ustr[victim].monitor)
  {
   strcpy(str2,"<");
   strcat(str2,ustr[user].name);
   strcat(str2,"> ");
  }
 else
  { str2[0]=0; }


if (ustr[victim].super > ustr[user].super)
  {
   write_str(user,"That would not be wise...");
   if (!ustr[victim].vis)
      sprintf(mess,"%s wanted to make you visible.",  ustr[user].name);
     else
      sprintf(mess,"%s wanted to make you invisible.",  ustr[user].name);

   mess[0]=toupper(mess[0]);
   write_str(victim, mess);
   return;
  }

if (!ustr[victim].vis)
  {
   sprintf(mess,"%s materializes before your eyes.",ustr[victim].name);
   writeall_str(mess, 1, victim, 0, user, NORM, MOVE, 0);
   sprintf(mess,"%s You suddenly become visible.",str2);
   write_str(victim,mess);
   ustr[victim].vis=1;
  }
 else
  {
   sprintf(mess,"%s suddenly vanishes.",ustr[victim].name);
   writeall_str(mess, 1, victim, 0, user, NORM, MOVE, 0);
   sprintf(mess,"%s You suddently become invisible.",str2);
   write_str(victim,mess);
   ustr[victim].vis=0;
  }

}


display_ranks(user)
int user;
{
  char fields[30];
  char z_mess[80];
  int i;
  strcpy(fields,RANKS);

write_str(user,"-----------------------------------");
write_str(user,"lvl  rank                  odds");
write_str(user,"-----------------------------------");

for(i=0;i<MAX_LEVEL;i++)
  {
    sprintf(z_mess,"%c %s     (%d)",fields[i],ranks[i],odds[i]);
    write_str(user,z_mess);
  }
}


/*----------------------------------------------------------*/
/* no-op code for disabled commands                         */
/*----------------------------------------------------------*/
command_disabled(user)
int user;
{
write_str(user,"Sorry: That command is temporarily disabled");
}





/*----------------------------------------------------------*/
/* check to see if the user attempting to get in is from a  */
/* site that has been banned.                               */
/* to make it easier, large sites can be banned by level    */
/* b c or d.                                                */
/* where ip address is a.b.c.d                              */
/*----------------------------------------------------------*/

int check_restriction(user)
int user;
{
FILE *fp;
char filename[FILE_NAME_LEN];

int i;

/* if a restriction file exists for an IP site, then the */
/* access is denied                                      */

/*-----------------------*/
/* check level d address */
/*-----------------------*/
 sprintf(t_mess,"%s/%s",RESTRICT_DIR,ustr[user].site);
 strncpy(filename,t_mess,FILE_NAME_LEN);

 if (fp=fopen(filename,"r")) {FCLOSE(fp); return(1);}

/*-----------------------*/
/* check level c address */
/*-----------------------*/
for(i=strlen(filename); filename[i]!='.' && i; filename[i--]=0);
filename[i--]=0;
if (fp=fopen(filename,"r"))  {FCLOSE(fp); return(1);}

/*-----------------------*/
/* check level b address */
/*-----------------------*/
for(i=strlen(filename); filename[i]!='.' && i; filename[i--]=0);
filename[i--]=0;
if (fp=fopen(filename,"r"))  {FCLOSE(fp); return(1);}

return(0);
}

/*----------------------------------------------------------*/
/* print out all users, or those with letter matches        */
/*----------------------------------------------------------*/

int print_users(user, inpstr)
int user;
char *inpstr;
{
char filename[FILE_NAME_LEN];

sprintf(t_mess,"%s",USERDIR);
strncpy(filename,t_mess,FILE_NAME_LEN);

if (!strcmp(inpstr," "))
   print_dir(user, filename, NULL);
  else
   print_dir(user, filename, inpstr);
return;
}



/*----------------------------------------------------------*/
/* create a file in the restrict library for an ip site     */
/* that has been banned.                                    */
/* to make it easier, large sites can be banned by level    */
/* b c or d.                                                */
/* where ip address is a.b.c.d                              */
/*----------------------------------------------------------*/

int restrict(user, inpstr)
int user;
char *inpstr;
{
FILE *fp;
char filename[FILE_NAME_LEN];

if (!strcmp(inpstr,"list") || !strlen(inpstr))
  {
   sprintf(t_mess,"%s",RESTRICT_DIR);
   strncpy(filename,t_mess,FILE_NAME_LEN);

   print_dir(user,filename,NULL);
   return;
  }

if (inpstr[0]=='.' ||
    inpstr[0]=='*' ||
    inpstr[0]=='/' ||
    inpstr[0]=='+' ||
    inpstr[0]=='-' ||
    inpstr[0]=='?' )
  {
   write_str(user,"Invalid site name.");
   return;
  }

/* open board file */
sprintf(t_mess,"%s/%s",RESTRICT_DIR,inpstr);
strncpy(filename,t_mess,FILE_NAME_LEN);

if (!(fp=fopen(filename,"a"))) {
        sprintf(mess,"%s : restriction could not be applied: ",syserror);
        write_str(user,mess);
        sprintf(mess,"Can't open %s to restrict access.",filename);
        logerror(mess);
        return;
        }

fputs("Sorry.  You are being locked out of this Chatline.",fp);
FCLOSE(fp);
sprintf(mess,"Site %s is now RESTRICTED from access.",inpstr);
write_str(user,mess);
sprintf(mess,"RESTRICT site %s by %s\n",inpstr,ustr[user].name);
print_to_syslog(mess);
btell(user,mess);
}


int unrestrict(user, inpstr)
int user;
char *inpstr;
{
char filename[FILE_NAME_LEN];
if (!strlen(inpstr)) {
        write_str(user,"You forgot the IP address"); return;
        }

/* check site name for stupid shit     */
/* like "." and ".." and "/etc/passwd" */
if (inpstr[0]=='.' ||
    inpstr[0]=='*' ||
    inpstr[0]=='/' ||
    inpstr[0]=='+' ||
    inpstr[0]=='-' ||
    inpstr[0]=='?' )
  {
   write_str(user,"Invalid site name.");
   return;
  }

sprintf(t_mess,"%s/%s",RESTRICT_DIR,inpstr);
strncpy(filename,t_mess,FILE_NAME_LEN);

remove(filename);
sprintf(mess,"Site %s is ALLOWED ACCESS again.",inpstr);
write_str(user,mess);

sprintf(mess,"UNREST site %s by %s\n",inpstr,ustr[user].name);
print_to_syslog(mess);
btell(user,mess);
}


igtells(user)
int user;
{
write_str(user,"You ignore your .tells");
ustr[user].igtell=1;
}


heartells(user)
int user;
{
write_str(user,"You listen to your .tells");
ustr[user].igtell=0;
}



/**** START OF SIGNAL FUNCTIONS ****/

/*** switching function ***/
void sigcall()
{
check_mess(0);
if (!num_of_users) goto SKIP;  /* no users so skip rest */

check_idle();
if (atmos_on)  atmospherics();

/* reset alarm */
SKIP:
reset_alarm();
signl = 1;

}



/*** reset alarm - first called from add_user ***/
reset_alarm()
{
signal(SIGALRM,sigcall);
alarm( MAX_ATIME );
}


/*-----------------------------------------------------------*/
/* atmospherics code                                         */
/*-----------------------------------------------------------*/
/* until future changes, this is how the files must be:      */
/*                                                           */
/*  example:                                                 */
/*          10                                               */
/*          line of text                                     */
/*          20                                               */
/*          line of text                                     */
/*          30                                               */
/*          line of text                                     */
/*          40                                               */
/*          line of text                                     */
/*                                                           */
/*  To have a multi line message, use '@' in the line        */
/*-----------------------------------------------------------*/
/*** atmospheric function (uses area directory) ***/
atmospherics()
{
FILE *fp;
char filename[FILE_NAME_LEN],probch[10],line[512];
int probint,area,i,j;
int rnd;

ATMOS_COUNTDOWN = ATMOS_COUNTDOWN - ((rand() % ATMOS_FACTOR) +1);

if ( ATMOS_COUNTDOWN > 0) return;

ATMOS_COUNTDOWN = ATMOS_RESET;

for (area=0; area<NUM_AREAS; ++area)
  {
   if (!find_num_in_area(area)) continue;
	
   sprintf(t_mess,"%s/atmos%d",datadir,area);
   strncpy(filename,t_mess,FILE_NAME_LEN);

   if (!(fp=fopen(filename,"r"))) continue;

   rnd=rand() % 100;
   ATMOS_LAST = rnd;
	
   fgets(probch,6,fp);
   while(!feof(fp))
    {
     probint=atoi(probch);
     fgets(line,511,fp);
		
     if (rnd<probint)
       {
        j = strlen(line);
        for(i=0;i<j;i++)
          {
           if (line[i]=='@') line[i]='\n';
          }

        write_area(area,line);
        break;
       }
		
      fgets(probch,6,fp);
     }
	
   FCLOSE(fp);
  }
}



/*** write to areas - if area= -1 write to all areas ***/
write_area(area,inpstr)
int area;
char *inpstr;
{
int u;
for (u=0;u<MAX_USERS;++u)
  {
    if (!user_wants_message(u,ATMOS)) continue;
    if (ustr[u].area==-1)             continue;

    if (ustr[u].area==area || area== -1)
      {
       write_str(u,inpstr);
      }
   }
}


/*** check to see if messages are out of date ***/
check_mess(startup)
int startup;
{
int b,tm,day,day2;
char c,line[ARR_SIZE+31],datestr[30],timestr[7],boardfile[FILE_NAME_LEN],tempfile[FILE_NAME_LEN];
char daystr[3],daystr2[3];
FILE *bfp,*tfp;

time(&tm);
strcpy(datestr,ctime(&tm));
midcpy(datestr,timestr,11,15);
midcpy(datestr,daystr,8,9);
day=atoi(daystr);

/* see if its time to check (midnight) */
if (startup) goto SKIP;
if (!strcmp(timestr,"00:01"))  {  checked=0;   return;  }
if (strcmp(timestr,"00:00") || checked) return;
checked=1;

SKIP:
if (!startup)
	write_area(-1,"\nSYSTEM:: Routine system check taking place...");
/* cycle through files */

sprintf(tempfile,"%s/temp",MESSDIR);

for(b=0;b<NUM_AREAS+1;++b) {
        if (b == NUM_AREAS)
          sprintf(boardfile,"%s/wizmess",MESSDIR);
         else
	  sprintf(boardfile,"%s/board%d",MESSDIR,b);
	
	if (!(bfp=fopen(boardfile,"r"))) continue;
	if (!(tfp=fopen(tempfile,"w"))) {
		logerror("Can't open temp file to write in check_mess()");
		FCLOSE(bfp);
		return;
		}

	/* go through board and write valid messages to temp file */
	fgets(line,ARR_SIZE+30,bfp);
	while(!feof(bfp)) {
		midcpy(line,daystr2,5,6);
		day2=atoi(daystr2);
		if (day2>day) day2 -= 30;  /* if mess from prev. month */
		if (day2>=day-MESS_LIFE)
			fputs(line,tfp);
			else astr[b].mess_num--;
		fgets(line,1050,bfp);
		}
	FCLOSE(bfp);
	FCLOSE(tfp);
	unlink(boardfile);

	/* write temp file back to board file */
	if (!(bfp=fopen(boardfile,"w"))) {
		logerror("Can't open board file for writing in check_mess()");
		astr[b].mess_num=0;
		continue;
		}
	if (!(tfp=fopen(tempfile,"r"))) {
		logerror("Can't open temp file for reading in check_mess()");
		FCLOSE(bfp);
		astr[b].mess_num=0;
		continue;
		}
	c=getc(tfp);
	while(!feof(tfp)) {
		putc(c,bfp);  c=getc(tfp);
		}
	FCLOSE(bfp);  FCLOSE(tfp);
	unlink(tempfile);
	}
	
        system_stats.logins_today       = 0;
        system_stats.new_users_today    = 0;

}



/*** see if any users are near or at idle limit ***/
check_idle()
{
int min,user;

for (user=0; user<MAX_USERS; ++user)
  {
   if (ustr[user].logging_in)
     {
       min=(int)((time(0) - ustr[user].last_input)/60);
       if (min > 3)
         {
          write_str(user,"\07Connection closed due to exceeeded login time limit");
          user_quit(user);
         }
      }
   }


for (user=0;user<MAX_USERS;++user)
  {
   if (ustr[user].super > WIZ_LEVEL - 2) continue;

   if ((ustr[user].area == -1 && !ustr[user].logging_in)) continue;

   min=(int)((time(0) - ustr[user].last_input)/60);

   if ( ( min > (IDLE_TIME - 2)) && !ustr[user].warning_given)
     {
      write_str(user,"\07*** Warning - input within 2 minutes or you will be disconnected ***");
      ustr[user].warning_given=1;
      continue;
     }

   if (min >= IDLE_TIME )
    {
     write_str(user,"\07Well you had your chance....Bye bye");
     user_quit(user);
    }
   }

says_running     = (says     + says_running)     / 2;
tells_running    = (tells    + tells_running)    / 2;
commands_running = (commands + commands_running) / 2;

tells = 0;
commands = 1;
says = 0;

if (fight.issued)
  {
    min = (int)((time(0)-fight.time)/60);
    if (min > IDLE_TIME)
      reset_chal(0," ");

  }

}	

/*----------------------------------------------------------------------*/
/* initialize a new user                                                */
/*----------------------------------------------------------------------*/

init_user(user,sw)
int user;
int sw;
{
int tm;
int i;

time(&tm);
strcpy(ustr[user].name,       ustr[user].login_name);

if (sw)
{
   strcpy(ustr[user].email_addr, " --- email not specified ---");
   strcpy(ustr[user].desc,       "-- a new user");
   strcpy(ustr[user].sex,        "no idea");
   strcpy(ustr[user].init_date,  ctime(&tm));
   strcpy(ustr[user].last_date,  ctime(&tm));
   strcpy(ustr[user].init_site, ustr[user].site);
   strcpy(ustr[user].last_site, ustr[user].site);
   for(i=0;i<MAX_AREAS;i++)
     {ustr[user].security[i]=' ';}
   strcpy(ustr[user].dir," ");
}

ustr[user].super=            0;
ustr[user].area=             INIT_ROOM;
ustr[user].listen=           1;
ustr[user].shout=            1;
ustr[user].vis=              1;
ustr[user].locked=           0;
ustr[user].suspended=        0;
ustr[user].monitor=          0;
ustr[user].rows=             24;
ustr[user].cols=             256;
ustr[user].car_return=       0;
ustr[user].abbrs =           1;
ustr[user].times_on =        0;
ustr[user].white_space =     1;
ustr[user].aver =            0;
ustr[user].hilite =          0;
ustr[user].new_mail =        0;
strcpy(ustr[user].flags, "                        ");

if (strcmp(ustr[user].login_name,ROOT_ID)==0)
  {
    ustr[user].super = MAX_LEVEL;
    for(i=0;i<MAX_AREAS;i++)
     {ustr[user].security[i]='Y';}
  }

}

/*----------------------------------------------------------------------*/
/* read the user profile                                                */
/*----------------------------------------------------------------------*/

copy_to_user(user)
int user;
{
strncpy(ustr[user].name,t_ustr.name,sizeof(t_ustr.name));
strncpy(ustr[user].password,t_ustr.password,sizeof(t_ustr.password));

ustr[user].super=t_ustr.super;

strncpy(ustr[user].email_addr,t_ustr.email_addr,sizeof(t_ustr.email_addr));
strncpy(ustr[user].desc,t_ustr.desc,sizeof(t_ustr.desc));
strncpy(ustr[user].sex,t_ustr.sex,sizeof(t_ustr.sex));
strncpy(ustr[user].init_date,t_ustr.init_date,sizeof(t_ustr.init_date));
strncpy(ustr[user].last_date,t_ustr.last_date,sizeof(t_ustr.last_date));
strncpy(ustr[user].init_site,t_ustr.init_site,sizeof(t_ustr.init_site));
strncpy(ustr[user].last_site,t_ustr.last_site,sizeof(t_ustr.last_site));
strncpy(ustr[user].dir,t_ustr.dir,sizeof(t_ustr.dir));

ustr[user].area           =t_ustr.area;
ustr[user].listen         =t_ustr.listen;
ustr[user].shout          =t_ustr.shout;
ustr[user].vis            =t_ustr.vis;
ustr[user].locked         =t_ustr.locked;
ustr[user].suspended      =t_ustr.suspended;
ustr[user].monitor        =t_ustr.monitor;
ustr[user].rows           =t_ustr.rows;
ustr[user].cols           =t_ustr.cols;
ustr[user].car_return     =t_ustr.car_return;
ustr[user].abbrs          =t_ustr.abbrs;
ustr[user].times_on       =t_ustr.times_on;
ustr[user].white_space    =t_ustr.white_space;
ustr[user].aver           =t_ustr.aver;
ustr[user].hilite         =t_ustr.hilite;
ustr[user].new_mail       =t_ustr.new_mail;

strncpy(ustr[user].security,t_ustr.security,sizeof(t_ustr.security));
strncpy(ustr[user].flags, t_ustr.flags, sizeof(t_ustr.flags));

}

/*----------------------------------------------------------------------*/
/* copy from user profile                                               */
/*----------------------------------------------------------------------*/

copy_from_user(user)
int user;
{
strncpy(t_ustr.name,ustr[user].name,sizeof(t_ustr.name));
strncpy(t_ustr.password,ustr[user].password,sizeof(t_ustr.password));

t_ustr.super = ustr[user].super;

strncpy(t_ustr.email_addr, ustr[user].email_addr, sizeof(t_ustr.email_addr));
strncpy(t_ustr.desc,       ustr[user].desc,       sizeof(t_ustr.desc));
strncpy(t_ustr.sex,        ustr[user].sex,        sizeof(t_ustr.sex));
strncpy(t_ustr.init_date,  ustr[user].init_date,  sizeof(t_ustr.init_date));
strncpy(t_ustr.last_date,  ustr[user].last_date,  sizeof(t_ustr.last_date));
strncpy(t_ustr.init_site,  ustr[user].init_site,  sizeof(t_ustr.init_site));
strncpy(t_ustr.last_site,  ustr[user].last_site,  sizeof(t_ustr.last_site));
strncpy(t_ustr.dir,        ustr[user].dir,        sizeof(t_ustr.dir));

t_ustr.area            =ustr[user].area;
t_ustr.listen          =ustr[user].listen;
t_ustr.shout           =ustr[user].shout;
t_ustr.vis             =ustr[user].vis;
t_ustr.locked          =ustr[user].locked;
t_ustr.suspended       =ustr[user].suspended;
t_ustr.monitor         =ustr[user].monitor;
t_ustr.rows            =ustr[user].rows;
t_ustr.cols            =ustr[user].cols;
t_ustr.car_return      =ustr[user].car_return;
t_ustr.abbrs           =ustr[user].abbrs;
t_ustr.times_on        =ustr[user].times_on;
t_ustr.white_space     =ustr[user].white_space;
t_ustr.aver            =ustr[user].aver;
t_ustr.hilite          =ustr[user].hilite;
t_ustr.new_mail        =ustr[user].new_mail;

strncpy(t_ustr.security, ustr[user].security, sizeof(t_ustr.security));
strncpy(t_ustr.flags, ustr[user].flags, sizeof(t_ustr.flags));

}

/*----------------------------------------------------------------------*/
/* read the user profile                                                */
/*----------------------------------------------------------------------*/

int read_user(name)
char * name;
{
FILE *f;                 /* user file*/
char filename[FILE_NAME_LEN];

sprintf(t_mess,"%s/%s",USERDIR,name);
strncpy(filename,t_mess,FILE_NAME_LEN);

f = fopen (filename, "r"); /* open for output */
if (f == NULL)
  {
    return(0);
  }

/*--------------------------------------------------------*/
/* values added after initial release must be initialized */
/*--------------------------------------------------------*/
t_ustr.monitor       = 0;
t_ustr.rows          = 24;
t_ustr.cols          = 256;
t_ustr.car_return    = 0;
t_ustr.abbrs         = 1;
t_ustr.white_space   = 1;
t_ustr.times_on      = 1;
t_ustr.aver          = 0;
t_ustr.hilite        = 0;
t_ustr.new_mail      = 0;
strcpy(t_ustr.flags, "                        ");

rbuf(t_ustr.name);
rbuf(t_ustr.password);
rval(t_ustr.super);
rbuf(t_ustr.email_addr);
rbuf(t_ustr.desc);
rbuf(t_ustr.sex);
rbuf(t_ustr.init_date);
rbuf(t_ustr.last_date);
rbuf(t_ustr.init_site);
rbuf(t_ustr.last_site);   /* last site*/
rbuf(t_ustr.dir);
rval(t_ustr.area);
rval(t_ustr.listen);
rval(t_ustr.shout);
rval(t_ustr.vis);
rval(t_ustr.locked);
rval(t_ustr.suspended);
rbuf(t_ustr.security);
rval(t_ustr.monitor);
rval(t_ustr.rows);
rval(t_ustr.cols);
rval(t_ustr.car_return);
rval(t_ustr.abbrs);
rval(t_ustr.times_on);
rval(t_ustr.white_space);
rval(t_ustr.aver);
rval(t_ustr.hilite);
rval(t_ustr.new_mail);
rbuf(t_ustr.flags);

/*---------------------------------------------------------------------*/
/* check for possible bad values in the users config                   */
/*---------------------------------------------------------------------*/

if (t_ustr.area > MAX_AREAS || t_ustr.area < 0)        t_ustr.area = 0;
if (t_ustr.listen > 1       || t_ustr.listen < 0)      t_ustr.listen = 0;
if (t_ustr.shout > 1        || t_ustr.shout < 0)       t_ustr.shout = 0;
if (t_ustr.vis > 1          || t_ustr.vis < 0)         t_ustr.vis = 0;
if (t_ustr.locked > 1       || t_ustr.locked < 0)      t_ustr.locked = 0;
if (t_ustr.monitor > 1      || t_ustr.monitor < 0)     t_ustr.monitor = 0;
if (t_ustr.rows > 256       || t_ustr.rows < 0)        t_ustr.rows = 24;
if (t_ustr.cols > 256       || t_ustr.cols < 0)        t_ustr.cols = 256;
if (t_ustr.car_return > 1   || t_ustr.car_return < 0)  t_ustr.car_return = 0;
if (t_ustr.abbrs > 1        || t_ustr.abbrs < 0)       t_ustr.abbrs = 0;
if (t_ustr.times_on > 32767 || t_ustr.times_on < 0)    t_ustr.times_on = 0;
if (t_ustr.white_space > 1  || t_ustr.white_space < 0) t_ustr.white_space = 0;
if (t_ustr.aver > 16000     || t_ustr.aver < 0)        t_ustr.aver = 16000;
if (t_ustr.hilite > 1       || t_ustr.hilite < 0)      t_ustr.hilite = 0;

FCLOSE(f);
return(1);

}

/*----------------------------------------------------------------------*/
/* read the user profile                                                */
/*----------------------------------------------------------------------*/
int
read_to_user(name,user)
char * name;
int user;
{
FILE *f;                 /* user file*/
char filename[FILE_NAME_LEN];

sprintf(t_mess,"%s/%s",USERDIR,name);
strncpy(filename,t_mess,FILE_NAME_LEN);

f = fopen (filename, "r"); /* open for output */
if (f == NULL)
  {
    return(0);
  }


/*--------------------------------------------------------*/
/* values added after initial release must be initialized */
/*--------------------------------------------------------*/
ustr[user].monitor        = 0;
ustr[user].rows           = 24;
ustr[user].cols           = 256;
ustr[user].car_return     = 0;
ustr[user].abbrs          = 1;
ustr[user].times_on       = 1;
ustr[user].white_space    = 1;
ustr[user].aver           = 0;
ustr[user].hilite         = 0;
ustr[user].new_mail       = 0;
strcpy(ustr[user].flags, "                        ");

rbuf(ustr[user].name);
rbuf(ustr[user].password);
rval(ustr[user].super);
rbuf(ustr[user].email_addr);
rbuf(ustr[user].desc);
rbuf(ustr[user].sex);
rbuf(ustr[user].init_date);
rbuf(ustr[user].last_date);
rbuf(ustr[user].init_site);
rbuf(ustr[user].last_site);   /* last site*/
rbuf(ustr[user].dir);
rval(ustr[user].area);
rval(ustr[user].listen);
rval(ustr[user].shout);
rval(ustr[user].vis);
rval(ustr[user].locked);
rval(ustr[user].suspended);
rbuf(ustr[user].security);
rval(ustr[user].monitor);
rval(ustr[user].rows);
rval(ustr[user].cols);
rval(ustr[user].car_return);
rval(ustr[user].abbrs);
rval(ustr[user].times_on);
rval(ustr[user].white_space);
rval(ustr[user].aver);
rval(ustr[user].hilite);
rval(ustr[user].new_mail);
rbuf(ustr[user].flags);

/*---------------------------------------------------------------------*/
/* check for possible bad values in the users config                   */
/*---------------------------------------------------------------------*/

if (ustr[user].area > MAX_AREAS || ustr[user].area < 0)        ustr[user].area = 0;
if (ustr[user].listen > 1       || ustr[user].listen < 0)      ustr[user].listen = 0;
if (ustr[user].shout > 1        || ustr[user].shout < 0)       ustr[user].shout = 0;
if (ustr[user].vis > 1          || ustr[user].vis < 0)         ustr[user].vis = 0;
if (ustr[user].locked > 1       || ustr[user].locked < 0)      ustr[user].locked = 0;
if (ustr[user].monitor > 1      || ustr[user].monitor < 0)     ustr[user].monitor = 0;
if (ustr[user].rows > 256       || ustr[user].rows < 0)        ustr[user].rows = 24;
if (ustr[user].cols > 256       || ustr[user].cols < 0)        ustr[user].cols = 256;
if (ustr[user].car_return > 1   || ustr[user].car_return < 0)  ustr[user].car_return = 0;
if (ustr[user].abbrs > 1        || ustr[user].abbrs < 0)       ustr[user].abbrs = 0;
if (ustr[user].times_on > 32767 || ustr[user].times_on < 0)    ustr[user].times_on = 0;
if (ustr[user].white_space > 1  || ustr[user].white_space < 0) ustr[user].white_space = 0;
if (ustr[user].aver > 16000     || ustr[user].aver < 0)        ustr[user].aver = 16000;
if (ustr[user].hilite > 1       || ustr[user].hilite < 0)      ustr[user].hilite = 0;
FCLOSE(f);
return(1);
}

/*----------------------------------------------------------------------*/
/* write the user profile                                               */
/*----------------------------------------------------------------------*/

write_user(name)
char * name;
{
FILE *f;                 /* user file*/
char filename[FILE_NAME_LEN];

sprintf(t_mess,"%s/%s",USERDIR,name);
strncpy(filename,t_mess,FILE_NAME_LEN);

f = fopen (filename, "w"); /* open for output */

if (f==NULL)
  { return; }


wbuf(t_ustr.name);
wbuf(t_ustr.password);
wval(t_ustr.super);
wbuf(t_ustr.email_addr);
wbuf(t_ustr.desc);
wbuf(t_ustr.sex);
wbuf(t_ustr.init_date);
wbuf(t_ustr.last_date);
wbuf(t_ustr.init_site);
wbuf(t_ustr.last_site);   /* last site*/
wbuf(t_ustr.dir);
wval(t_ustr.area);
wval(t_ustr.listen);
wval(t_ustr.shout);
wval(t_ustr.vis);
wval(t_ustr.locked);
wval(t_ustr.suspended);
wbuf(t_ustr.security);
wval(t_ustr.monitor);
wval(t_ustr.rows);
wval(t_ustr.cols);
wval(t_ustr.car_return);
wval(t_ustr.abbrs);
wval(t_ustr.times_on);
wval(t_ustr.white_space);
wval(t_ustr.aver);
wval(t_ustr.hilite);
wval(t_ustr.new_mail);
wbuf(t_ustr.flags);

FCLOSE(f);

}


/*-------------------------------------------------------------------------*/
/* check to see if the user exists                                         */
/*-------------------------------------------------------------------------*/

int check_for_user(filename)
char * filename;
{
FILE * bfp;
bfp = fopen(filename, "r");

if (bfp)
  {
    FCLOSE(bfp);
    return(1);
  }
return(0);
}

/*-------------------------------------------------------------------------*/
/* remove a user                                                           */
/*-------------------------------------------------------------------------*/

int remove_user(name)
char * name;
{
char filename[FILE_NAME_LEN];

sprintf(t_mess,"%s/%s",USERDIR,name);
strncpy(filename,t_mess,FILE_NAME_LEN);

remove(filename);
}


/*-------------------------------------------------------------------------*/
/* Write a buffer to a file.  If we cannot write everything, return FALSE. */
/* Also, tell the user why the write did not work if it didn't.            */
/*-------------------------------------------------------------------------*/

int xwrite (f, buf, size)
FILE *f;
char *buf;
int size;
{
	int bytes;

	bytes = fwrite (buf, 1, size, f);
	if (bytes == -1) {
		return (FALSE);
	}
	if (bytes != size) {
		return (FALSE);
	}
	return (TRUE);
}

/*----------------------------------------------------------------------*/
/* Read a buffer from a file.  If the read fails, we tell the user why  */
/* and return FALSE.                                                    */
/*----------------------------------------------------------------------*/

int xread (f, buf, size)
FILE *f;
char *buf;
int size;
{
	int bytes;

	bytes = fread (buf, 1, size, f);
	if (bytes == -1) {
		return (FALSE);
	}
	if (bytes != size) {
		return (FALSE);
	}
	return (TRUE);
}


/*--------------------------------------------------------------------*/
/* a simple attempt to encrypt a password                             */
/*--------------------------------------------------------------------*/

st_crypt(str)
char str[];
{
int i = 0;
char last = ' ';

while(str[i])
  {str[i]= (( (str[i] - 32) + lock[(i%num_locks)] + (last - 32) ) % 94) + 32;
   last = str[i];
   i++;
   }
}

/*-----------------------------------------------------------------------*/
/* the preview command                                                   */
/*-----------------------------------------------------------------------*/

preview(user, inpstr)
int user;
char *inpstr;
{
char filename[FILE_NAME_LEN];

if (!strlen(inpstr))
  {
   preview(user,"names");
   return;
   }

/* plug security hole */
if (check_fname(inpstr,user))
  {
   write_str(user,"Illegal name.");
   return;
  }

/* open board file */
sprintf(t_mess,"%s/%s",PICTURE_DIR,inpstr);
strncpy(filename,t_mess,FILE_NAME_LEN);

if (cat(filename,user,0) ==0)
  {write_str(user,"Picture file does not exist!r\n");}
}

/*-----------------------------------------------------------------------*/
/* the picture command                                                   */
/*-----------------------------------------------------------------------*/

picture(user, inpstr)
int user;
char *inpstr;
{
char filename[FILE_NAME_LEN];
char name[16], z_mess[30], x_name[256];
FILE *bfp;

if (!strlen(inpstr))
  {
   preview(user,"names");
   return;
  }

/* plug security hole */

if (check_fname(inpstr,user))
  {
   write_str(user,"Illegal name.");
   return;
  }

sprintf(t_mess,"%s/%s",PICTURE_DIR,inpstr);
strncpy(x_name,t_mess,255);

if (!( bfp=fopen(x_name,"r") ) )
  {
   write_str(user,"Picture does not exist.");
   FCLOSE(bfp);
   return;
  }

FCLOSE(bfp);

strcpy(name,ustr[user].name);
sprintf(z_mess,"(%s) shows:",name);

writeall_str(z_mess, 1, user, 0, user, NORM, PICTURE, 0);


/* open board file */
sprintf(t_mess,"%s/%s",PICTURE_DIR,inpstr);
strncpy(filename,t_mess,FILE_NAME_LEN);

if(strcmp(inpstr,"names")==0)
   {
    preview(user,inpstr);
    return;
   }

if (catall(user,filename) == 1)
  {write_str(user,"Picture file does not exist!");}
}

/*-------------------------------------------------------------*/
/* change a password                                           */
/*-------------------------------------------------------------*/

password(user, pword)
int user;
char * pword;
{

 if (pword[0]<32 || strlen(pword)< 3)
    {
     write_str(user,"Invalid password given [must be at least 3 letters].");
     return;
    }

  if (strlen(pword)>NAME_LEN-1)
    {
     write_str(user,"Password too long");
     return;
    }

  /*-------------------------------------------------------------*/
  /* convert name & passwd to lowercase and encrypt the password */
  /*-------------------------------------------------------------*/

  strtolower(pword);

  if (strcmp(ustr[user].login_name, pword) ==0)
    {
        write_str(user,"\nPassword cannot be the login name. \nPassword not changed.");
        return;
    }

  st_crypt(pword);
  strcpy(ustr[user].password,pword);
  copy_from_user(user);
  write_user(ustr[user].name);
  write_str(user,"Password is now changed.");
}

/*-------------------------------------------------------------*/
/* toggle-monitoring                                           */
/*-------------------------------------------------------------*/

tog_monitor(user)
int user;
{
 if (ustr[user].monitor)
   {
    ustr[user].monitor=0;
    write_str(user," *** monitoring is now off ***");
   }
  else
   {
    ustr[user].monitor=1;
    write_str(user," *** monitoring is now on ***");
   }
}

/*-----------------------------------------------------------*/
/* check file name for hack                                  */
/*-----------------------------------------------------------*/

check_fname(inpstr,user)
char *inpstr;
int user;
{
if (strpbrk(inpstr,".$/+*[]\\") )
  {
   sprintf(mess,"User %s: illegal file: %s",ustr[user].name,inpstr);
   logerror(mess);
   return(1);
  }
return(0);
}

/*--------------------------------------------------------------*/
/* display file to all in a room                                */
/*--------------------------------------------------------------*/

catall(user,filename)
int user;
char *filename;
{
FILE *fp;

if (!(fp=fopen(filename,"r")))
  {
   return 1;
  }

/* jump to reading posn in file */
  fseek(fp,0,0);

/* loop until end of file or end of page reached */
strcpy(mess," ");
while(!feof(fp))
   {
     writeall_str(mess, 1, user, 1, user, NORM, PICTURE, 0);
     fgets(mess,sizeof(mess)-1,fp);
     mess[strlen(mess)-1]=0;
   }

FCLOSE(fp);
return (0);
}

/*----------------------------------------------------------------------*/
/* room add security clearance                                          */
/*----------------------------------------------------------------------*/

roomsec(user,inpstr)
int user;
char * inpstr;
{
char permit, permission[81];
char other_user[ARR_SIZE];
char room[ARR_SIZE];
int u,a,b,z_stat;

if (!strlen(inpstr)) {
   write_str(user,"usage: .permission [add|sub] user room");
   return;
   }
sscanf(inpstr,"%s ",permission);
strtolower(permission);
remove_first(inpstr);

if (!(strcmp("add",permission)))
  {permit = 'Y';}
 else
  {permit = 0;}

if (!strlen(inpstr)) {
   write_str(user,"Security clear who?");
   return;
   }

strcpy(room,"*not found*");
sscanf(inpstr,"%s ",other_user);
strtolower(other_user);
CHECK_NAME(other_user);

remove_first(inpstr);

z_stat=sscanf(inpstr,"%s ",room);
strtolower(room);

if (!strlen(room) || z_stat==EOF)
  {
   write_str(user,"Clear security for what room?");
   return;
  }

if (!read_user(other_user))
  {
    write_str(user,NO_USER_STR);
   return;
  }

b= -1;

/* read in descriptions and joinings */
for (a=0;a<NUM_AREAS;++a)
  {
    if (strcmp(room,astr[a].name)==0)
      {t_ustr.security[a]=permit;
       b=a;
       a=NUM_AREAS;
       write_str(user,"Security clearenace set");
      }
  }

write_user(other_user);

if ((u=get_user_num(other_user,user))>-1)
  {
    if (b>-1)
      ustr[u].security[b]=permit;

    if (permit > ' ')
      {
       sprintf(mess,"%s has clear you to enter room %s",ustr[user].name,room);
       mess[0]=toupper(mess[0]);
       write_str(u,mess);
      }
     else
      {
       sprintf(mess,"%s has locked you from entering room %s",ustr[user].name,room);
       mess[0]=toupper(mess[0]);
       write_str(u,mess);
      }
   }

}
/*----------------------------------------------------------------------*/
/* ptell                                                                */
/*----------------------------------------------------------------------*/

ptell(user,inpstr)
int user;
char * inpstr;
{
char x_name[256],filename[100],temp[80];
char other_user[ARR_SIZE];
int u,z_stat;
FILE *bfp;

if (!strlen(inpstr)) {
   write_str(user,"Show picture to who?");
   return;
   }

strcpy(filename,"*not found*");
sscanf(inpstr,"%s ",other_user);
strtolower(other_user);


if ((u=get_user_num(other_user,user))== -1)
  {
   write_str(user,"User is not logged in.");
   return;
   }

if (ustr[u].afk)
  {
    if (ustr[u].afk == 1)
      sprintf(t_mess,"- %s is Away From Keyboard -",ustr[u].name);
     else
      sprintf(t_mess,"- %s is blanked AFK (is not seeing this) -",ustr[u].name);

    write_str(user,t_mess);
  }

remove_first(inpstr);

z_stat=sscanf(inpstr,"%s ",filename);

if (!strlen(filename) || z_stat==EOF)
  {
   write_str(user,"Show what picture?");
   return;
  }

/* plug security hole */
if (check_fname(filename,user))
  {
   sprintf(mess,"Illegal file name: %s",filename);
   write_str(user,mess);
   return;
  }

sprintf(t_mess,"%s/%s",PICTURE_DIR,filename);
strncpy(x_name,t_mess,256);

if (!(bfp=fopen(x_name,"r")))
  {
   write_str(user,"Picture does not exist.");
   FCLOSE(bfp);
   return;
  }
FCLOSE(bfp);

sprintf(temp,"Private picture sent by: %s ",ustr[user].name);
write_str(u,temp);

preview(u,filename);
sprintf(temp,"Picture request of %s to view sent to %s ",filename,other_user);
write_str(user,temp);

}


/*------------------------------------------------*/
/* follow someone to a room                       */
/*------------------------------------------------*/

follow(user,inpstr)
int user;
char * inpstr;
{
/*code to be done later */
char other_user[ARR_SIZE];
int user2;

if (!strlen(inpstr)) {
   write_str(user,"Follow who?");
   return;
   }

sscanf(inpstr,"%s",other_user);
if ((user2=get_user_num(other_user,user))== -1) {
   not_signed_on(user,other_user); return;
   }

strcpy(inpstr,astr[ustr[user2].area].name);
go(user,inpstr,0);
}


/*-------------------------------------------------*/
/* log to syslog a string                          */
/*-------------------------------------------------*/
print_to_syslog(str)
char * str;
{
FILE *fp;

if (!syslog_on) return;

 if ((fp=fopen(LOGFILE,"a")) )
   {
    fputs(str,fp);
    FCLOSE(fp);
   }
}



/*--------------------------------------------------------------------*/
/* this command basically lists out a specified directory to the user */
/* the directory is specified in the inpstr                           */
/*--------------------------------------------------------------------*/

print_dir(user,inpstr, s_search)
int user;
char *inpstr;
char *s_search;
{
int num;
char buffer[132];
char small_buff[64];
struct dirent *dp;
DIR  *dirp;

 strcpy(buffer,"    ");
 num=0;
 dirp=opendir((char *)inpstr);

 if (dirp == NULL)
   {write_str(user,"Directory information not found.");
    return;
   }

 while ((dp = readdir(dirp)) != NULL)
   {
    sprintf(small_buff,"%-18s ",dp->d_name);
    if (s_search)
      { if (strstr(small_buff,s_search))
        {
         if (small_buff[0]!='.')
          { write_str_nr(user,small_buff);
            num++;
           if (num%4==0) write_str(user,"");
          }

        }
      }
     else
      {
       if (small_buff[0]!='.')
        { write_str_nr(user,small_buff);
         num++;
         if (num%4==0) write_str(user,"");
        }
      }

  }
 write_str(user,"");
 sprintf(mess,"Displayed %d items",num);
 write_str(user,mess);

 (void) closedir(dirp);
}

/*--------------------------------------------------------------------*/
/* this command basically lists out a specified directory to the user */
/* the directory is specified in the inpstr                           */
/*--------------------------------------------------------------------*/

usr_stat(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
char sw[2][4];
int i;

strcpy(sw[0],"off");
strcpy(sw[1],"on ");

if (strlen(inpstr))
  {

   sscanf(inpstr,"%s ",other_user);
   strtolower(other_user);
   CHECK_NAME(other_user);


   if ((i = get_user_num(other_user,user)) != -1)
     if (strcmp(ustr[i].name, other_user) == 0)
     {
      write_str(user,"*** NOTE: That user is currently logged on. ***");
     }
  }
 else
  {
    strcpy(other_user, ustr[user].name);
  }

if (!read_user(other_user))
  {
   write_str(user,NO_USER_STR);
   return;
  }

write_str(user,"+--------------------------------------------------------------------------+");
write_str(user,"|                             User Status                                  |");
write_str(user,"+--------------------------------------------------------------------------+");

sprintf(mess,"| Id:    %-21.21s %-31.31s             |", t_ustr.name, t_ustr.desc );
write_str(user,mess);

if (t_ustr.new_mail)
  {
   write_str(user,"|                                      Has unread mail messages.           |");
   write_str(user,"|                                                                          |");
  }

sprintf(mess,"| Email: %-65.65s |",t_ustr.email_addr);
write_str(user,mess);

sprintf(mess,"| Gender:  %-10.10s                                                      |",t_ustr.sex);
write_str(user,mess);

write_str(user,"|                                                                          |");

sprintf(mess,"| Last login:   %-16.16s          logins to date: %-5.5d            |",
                           t_ustr.last_date,
                           t_ustr.times_on);
write_str(user,mess);

if (astr[t_ustr.area].hidden)
    sprintf(mess,"| In area:                                ave time/login: %-5.5d mins       |",
             t_ustr.aver);
  else
    sprintf(mess,"| In area:      %-16.16s          ave time/login: %-5.5d mins       |",
       astr[t_ustr.area].name, t_ustr.aver);
write_str(user,mess);

sprintf(mess,"| From site:    %-15.15s                                            |",
              t_ustr.last_site);
if (ustr[user].super > WIZ_LEVEL)      write_str(user,mess);

sprintf(mess,"| rank:         %d                                                          |",
              t_ustr.super);
if (ustr[user].super > WIZ_LEVEL)      write_str(user,mess);

write_str(user,"|                                                                          |");

sprintf(mess,"| Settings:     rows %3.3d                  cols %3.3d                         |",
             t_ustr.rows, t_ustr.cols);
write_str(user,mess);

sprintf(mess,"|               carriages %s             Abbrs %s                        |",
             sw[t_ustr.car_return], sw[t_ustr.abbrs]);
write_str(user,mess);


sprintf(mess,"|               whitespace %s            Hilite %s                       |",
             sw[t_ustr.white_space], sw[t_ustr.hilite]);
write_str(user,mess);
write_str(user,"|                                                                          |");
sprintf(mess,"|               shout  %s                listen %s                       |",
             sw[t_ustr.shout], sw[t_ustr.listen]);
write_str(user,mess);

sprintf(mess,"|               ignore tells %s          visible %s                      |",
             sw[t_ustr.igtell], sw[t_ustr.vis]);
write_str(user,mess);

write_str(user,"+--------------------------------------------------------------------------+");

write_str(user,"");

}


/*------------------------------------------------*/
/* set email address                              */
/*------------------------------------------------*/
set_email(user,inpstr)
int user;
char *inpstr;
{

  if (strlen(inpstr)>EMAIL_LENGTH)
    {
      write_str(user,"Email address trucated");
      inpstr[EMAIL_LENGTH-1]=0;
    }
  sprintf(mess,"Set user email address to: %s",inpstr);
  write_str(user,mess);

  read_user(ustr[user].login_name);
  strcpy(t_ustr.email_addr,inpstr);
  strcpy(ustr[user].email_addr,inpstr);
  write_user(ustr[user].login_name);

}


/*------------------------------------------------*/
/* set gender                                     */
/*------------------------------------------------*/
set_sex(user,inpstr)
int user;
char *inpstr;
{

  if (strlen(inpstr)>9)
    {
      write_str(user,"Gender  trucated");
      inpstr[9]=0;
    }
  sprintf(mess,"Set user gender to: %s",inpstr);
  write_str(user,mess);

  read_user(ustr[user].login_name);
  strcpy(t_ustr.sex,inpstr);
  strcpy(ustr[user].sex,inpstr);
  write_user(ustr[user].login_name);

}

/*------------------------------------------------*/
/* set rows                                       */
/*------------------------------------------------*/
set_rows(user,inpstr)
int user;
char *inpstr;
{

  int value=5;

  sscanf(inpstr,"%d", &value);

  if (value < 5 || value > 256)
    {
      write_str(user,"rows set to 25 (valid range is 5 to 256)");
      value = 25;
    }

  sprintf(mess,"Set terminal rows to: %d",value);
  write_str(user,mess);

  read_user(ustr[user].login_name);
  t_ustr.rows     = value;
  ustr[user].rows = value;
  write_user(ustr[user].login_name);
}

/*------------------------------------------------*/
/* set cols                                       */
/*------------------------------------------------*/
set_cols(user,inpstr)
int user;
char *inpstr;
{

  int value=5;

  sscanf(inpstr,"%d", &value);

  if (value < 16 || value > 256)
    {
      write_str(user,"cols set to 80 (valid range is 16 to 256)");
      value = 80;
    }

  sprintf(mess,"Set terminal cols to: %d",value);
  write_str(user,mess);

  read_user(ustr[user].login_name);
  t_ustr.cols     = value;
  ustr[user].cols = value;
  write_user(ustr[user].login_name);
}

/*------------------------------------------------*/
/* set car_return                                 */
/*------------------------------------------------*/
set_car_ret(user,inpstr)
int user;
char *inpstr;
{

  int value = -1;

  sscanf(inpstr,"%d", &value);

  if (value < 0 || value > 1)
    {
      write_str(user,"carriage returns set to 0 (valid values are 1 or 0)");
      value = 0;
    }

  sprintf(mess,"Set carriage returns to: %d",value);
  write_str(user,mess);

  read_user(ustr[user].login_name);
  t_ustr.car_return     = value;
  ustr[user].car_return = value;
  write_user(ustr[user].login_name);
}

/*------------------------------------------------*/
/* set atmos                                      */
/*------------------------------------------------*/
set_atmos(user,inpstr)
int user;
char *inpstr;
{

  int    value  = -1;
  int    factor = -1;

  sscanf(inpstr,"%d %d", &value, &factor);

  if (value < 0 || value > 1000)
    {
      value = 100;
    }

 if (factor < 0 || factor >1000)
    {
      factor = 5;
    }

  sprintf(mess,"Atmos frequency chance set to: %d %d",value, factor);
  write_str(user,mess);

  ATMOS_RESET     = value;
  ATMOS_FACTOR    = factor;
  ATMOS_COUNTDOWN = value;
}

/*------------------------------------------------*/
/* set abbrs                                      */
/*------------------------------------------------*/
set_abbrs(user,inpstr)
int user;
char *inpstr;
{


  if (ustr[user].abbrs)
    {
      write_str(user, "Abbreviations are now off for you.");
      ustr[user].abbrs = 0;
    }
   else
    {
      write_str(user, "You can now use abbreviations");
      ustr[user].abbrs = 1;
    }

  read_user(ustr[user].login_name);
  t_ustr.abbrs = ustr[user].abbrs;
  write_user(ustr[user].login_name);
}

/*------------------------------------------------*/
/* set white space                                */
/*------------------------------------------------*/
set_white_space(user,inpstr)
int user;
char *inpstr;
{


  if (ustr[user].white_space)
    {
      write_str(user, "White space removal is now off.");
      ustr[user].white_space = 0;
    }
   else
    {
      write_str(user, "White space removal is now on.");
      ustr[user].white_space = 1;
    }

  read_user(ustr[user].login_name);
  t_ustr.white_space = ustr[user].white_space;
  write_user(ustr[user].login_name);
}
/*------------------------------------------------*/
/* set hilights                                   */
/*------------------------------------------------*/
set_hilite(user,inpstr)
int user;
char *inpstr;
{


  if (ustr[user].hilite)
    {
      write_str(user, "High_lighting now off.");
      ustr[user].hilite = 0;
    }
   else
    {
      write_str(user, "High_lighting now on.");
      ustr[user].hilite = 1;
    }

  read_user(ustr[user].login_name);
  t_ustr.hilite = ustr[user].hilite;
  write_user(ustr[user].login_name);
}

/*------------------------------------------------*/
/* set user stuff                                 */
/*------------------------------------------------*/

set(user,inpstr)
int user;
char *inpstr;
{
  char command[132];

  sscanf(inpstr,"%s ",command);
  remove_first(inpstr);  /* get rid of commmand word */
  strtolower(command);

  if (!strcmp("email",command))
    {set_email(user,inpstr);
     return;
    }

  if (!strcmp("gender",command))
    {set_sex(user,inpstr);
     return;
    }

  if (!strcmp("rows",command) || !strcmp("lines",command))
    {set_rows(user,inpstr);
     return;
    }

 if (!strcmp("cols",command) || !strcmp("width",command))
    {set_cols(user,inpstr);
     return;
    }

 if (!strcmp("car",command) || !strcmp("carriage",command) )
    {set_car_ret(user,inpstr);
     return;
    }

 if (!strcmp("abbrs",command))
    {set_abbrs(user,inpstr);
     return;
    }

 if (!strcmp("space",command))
    {set_white_space(user,inpstr);
     return;
    }

 if (!strcmp("hi",command))
    {set_hilite(user,inpstr);
     return;
    }

 if (!strcmp("atmos",command))
    {set_atmos(user,inpstr);
     return;
    }
 write_str(user,"Valid options are:");
 write_str(user,"       email                  gender");
 write_str(user,"       rows (lines)           cols    (width)");
 write_str(user,"       abbrs                  car  (carriage) ");
 write_str(user,"       space                  hi             ");

}



/*------------------------------------------------------------------*/
/* the nuke command....remove user from user dir                    */
/*------------------------------------------------------------------*/

int nuke(user, inpstr)
int user;
char *inpstr;
{
char filename[FILE_NAME_LEN], other_user[ARR_SIZE];
char z_mess[132];

if (!strlen(inpstr))
  {
   write_str(user,"Nuke who?");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);
CHECK_NAME(other_user);

if (!read_user(other_user))
  {
   write_str(user,NO_USER_STR);
   return;
  }

if (t_ustr.super >= ustr[user].super)
  {
    sprintf(z_mess,"You cannot nuke a user of same or higher rank.",other_user);
    write_str(user,z_mess);
    return;
  }

sprintf(z_mess,"NUKE %s by %s\n",other_user,ustr[user].name);
print_to_syslog(z_mess);

remove_user(other_user);

sprintf(t_mess,"%s/%s", MAILDIR, other_user);
strncpy(filename, t_mess, FILE_NAME_LEN);
remove(filename);

sprintf(t_mess,"%s/%s",MACRODIR,other_user);
strncpy(filename, t_mess, FILE_NAME_LEN);
remove(filename);

btell(user,z_mess);
}

/*-----------------------*/
/*  tell all wizards     */
/*-----------------------*/

btell(user,inpstr)
int user;
char *inpstr;
{
char line[ARR_SIZE];
int pos = bt_count%NUM_LINES;
int f;

if (user==-1)
  {
   return;
  }

if (!strlen(inpstr))
  {
    write_str(user,"Review btells:");

    for (f=0;f<NUM_LINES;++f)
      {
        if ( strlen(bt_conv[pos]) )
         {
	  write_str(user,bt_conv[pos]);
	 }
	pos = ++pos % NUM_LINES;
      }

    write_str(user,"<Done>");
    return;
  }

sprintf(line,"<bt> %s: %s",ustr[user].name,inpstr);
write_hilite(user,line);

writeall_str(line, WIZ_ONLY, user, 0, user, BOLD, NONE, 0);

/*-----------------------------------*/
/* store the btell in the rev buffer */
/*-----------------------------------*/

strncpy(bt_conv[bt_count],line,MAX_LINE_LEN);
bt_count = ( ++bt_count ) % NUM_LINES;
	
}


/*----------------------------------------------*/
/* fix for telnet ctrl-c and ctrl-d *arctic9*   */
/*----------------------------------------------*/

will_time_mark(user)
int user;
{
char seq[4];

 sprintf(seq,"%c%c%c",IAC,WILL,TELOPT_TM);
 write_str(user,seq);
}


/*--------------------------------*/
/* clear screen                   */
/*--------------------------------*/
cls(user,inpstr)
int user;
char *inpstr;
{
int   i         = ustr[user].rows;
char  addem[3];

strcpy(addem,"\n\r");
mess[0] = 0;

if (!ustr[user].car_return) addem[1]=0;
if (i > 75) i = 75;

for(; i--;)
 { strcat(mess,addem); }

strcat(mess, "OK");
strcat(mess, addem);

write_str(user,mess);
}

/*-----------------------------------*/
/* get a random number based on rank */
/*-----------------------------------*/
int get_odds_value(user)
int user;
{
return( (rand() % odds[ustr[user].super]) + 1 );
}

/*----------------------------------------------------------*/
/* determine the result of a random event between two users */
/*----------------------------------------------------------*/
int determ_rand(u1, u2)
int u1;
int u2;
{
int v1, v2, v3, result;
float f_fact;

v1 = get_odds_value(u1);
v2 = get_odds_value(u2);

if (v1 == v2)  /* truely amazing, a real tie */
  { return(TIE); }

if (v1 > v2)
  {
    result = 1;
    f_fact = (float)((float)v2/(float)v1);
  }
 else
  {
    result = 2;
    f_fact = (float)((float)v1/(float)v2);
  }

v3 = (int) (f_fact * 100.0);

if (v3 > CLOSE_NUMBER)
  { if (  rand() % 2 )
      return(TIE);
     else
      return(BOTH_LOSE);
  }

return(result);
}

/*----------------------------------------------------------*/
/* issue a fight challenge to another user                  */
/*----------------------------------------------------------*/

issue_chal(user,user2)
int user;
int user2;
{
  fight.first_user  = user;
  fight.second_user = user2;
  fight.issued      = 1;
  fight.time        = time(0);

  sprintf(mess, chal_text[ rand() % num_chal_text ],
                ustr[user].name,
                ustr[user2].name);

  writeall_str(mess, 1, user, 0, user, NORM, FIGHT, 0);
  write_str(user,mess);

  write_str(user2,CHAL_LINE);
  write_str(user2,CHAL_LINE2);
  write_str(user,CHAL_ISSUED);
}

/*----------------------------------------------------------*/
/* accept a fight challenge                                 */
/*----------------------------------------------------------*/
accept_chal(user)
int user;
{
int x;
int a,b;

a=fight.first_user;
b=fight.second_user;

x = determ_rand(a, b);

if (x == TIE)
  {
   sprintf(mess, tie1_text[ rand() % num_tie1_text ],
                 ustr[a].name,
                 ustr[b].name);

   writeall_str(mess, 1, user, 0, user, NORM, FIGHT, 0);
   write_str(user,mess);
   return;
  }

if (x == BOTH_LOSE)
  {
   sprintf(mess, tie2_text[ rand() % num_tie2_text ],
                 ustr[a].name,
                 ustr[b].name);

   writeall_str(mess,1,user,0,user,NORM,FIGHT,0);
   write_str(user,mess);
   user_quit(a);
   user_quit(b);
   return;
  }

if (x == 1)
  {
   sprintf(mess, wins1_text[ rand() % num_wins1_text ],
                 ustr[a].name,
                 ustr[b].name);

   writeall_str(mess,1,user,0,user,NORM,FIGHT,0);
   write_str(user,mess);
   user_quit(b);
   return;
  }

if (x == 2)
  {
   sprintf(mess, wins2_text[ rand() % num_wins2_text ],
                 ustr[b].name,
                 ustr[a].name);

   writeall_str(mess,1,user,0,user,NORM,FIGHT,0);
   write_str(user,mess);
   user_quit(a);
   return;
  }
}

/*----------------------------------------------------------*/
/* reset the fight                                          */
/*----------------------------------------------------------*/

reset_chal(user,inpstr)
int user;
char *inpstr;
{
  fight.first_user = -1;
  fight.second_user = -1;
  fight.issued = 0;
  fight.time = 0;
}

/*----------------------------------------------------------*/
/* the fight command                                        */
/*----------------------------------------------------------*/

int
fight_another(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int user2;
int mode;

if (!strlen(inpstr))
  {
   write_str(user,"Fight status:");
   if (fight.issued)
     {
      sprintf(mess,"Aggressor:   %s",ustr[fight.first_user].name);
      write_str(user,mess);
      sprintf(mess,"Defender:    %s",ustr[fight.second_user].name);
      write_str(user,mess);
     }
    else
     write_str(user,"   No current fight is challenged.");
   return;
  }

mode = 0;
if (!strcmp(inpstr,"reset"))
  {
    reset_chal(user, inpstr);
    return;
  }

if (!strcmp(inpstr,"1"))   mode = 2;
if (!strcmp(inpstr,"yes")) mode = 2;
if (!strcmp(inpstr,"0"))   mode = 1;
if (!strcmp(inpstr,"no"))  mode = 1;

if (!mode)
  {
    sscanf(inpstr, "%s", other_user);
    user2 = get_user_num( other_user , user);

    if (ustr[user2].afk)
     {
      if (ustr[user2].afk == 1)
        sprintf(t_mess,"- %s is Away From Keyboard -",ustr[user2].name);
       else
        sprintf(t_mess,"- %s is blanked AFK (is not seeing this) -",ustr[user2].name);

       write_str(user,t_mess);
       return;
     }

    if (user2 == -1 )
     {
      not_signed_on(user,other_user);
      return;
     }
    mode = 3;
  }


if (fight.issued && mode == 3)
  {
    write_str(user, "Sorry, you must wait until the others are done.");
    return;
  }

if (!fight.issued && (mode == 1 || mode == 2) )
  {
    write_str(user, "You are not being challenged to a fight at this time.");
    return;
  }

if ((mode == 1 || mode == 2) && fight.second_user != user)
  {
    write_str(user, "You are not the challenged user...type .fight to see");
    return;
  }

if (mode == 3)
  {
   if (user == user2)
     {
       write_str(user,"You need help! (Fighting yourself...tsk tsk tsk)");
       return;
     }

/*----------------------------------------------------*/
/* check for standard fight room                      */
/*----------------------------------------------------*/


   if (FIGHT_ROOM != -1)
     {
       if (ustr[user].area != FIGHT_ROOM)
         {
           sprintf(t_mess,"To fight you must be in %s.",astr[FIGHT_ROOM].name);
           write_str(user,t_mess);
           return;
         }

       if (ustr[user2].area != FIGHT_ROOM)
         {
           sprintf(t_mess,"%s is not here to fight you.",ustr[user2].name);
           write_str(user,t_mess);

           sprintf(t_mess,"%s wanted to fight you, but you must be in %s to do that.",
                           ustr[user].name,
                           astr[FIGHT_ROOM].name);
           write_str(user2,t_mess);
           return;
         }
     }

   issue_chal(user, user2);
   return;
  }

if (mode == 1)
  {
   sprintf(mess, wimp_text[ rand() % num_wimp_text ], ustr[user].name);
   writeall_str(mess,1,user,0,user,NORM,FIGHT,0);
   write_str(user,mess);
   reset_chal(user, inpstr);
   return;
  }

if (mode == 2)
  {
   accept_chal(user);
   reset_chal(user, inpstr);
   return;
  }

}

resolve_names_on(user,inpstr)
int user;
char *inpstr;
{
  resolve_names = 1;
  btell(user," Site name resolver turned on ");
}

resolve_names_off(user,inpstr)
int user;
char *inpstr;
{
  resolve_names = 0;
  btell(user," Site name resolver turned off ");
}

toggle_atmos(user,inpstr)
int user;
char *inpstr;
{
char line[132];

  if (atmos_on)
    {
      write_str(user,"Atmospherics OFF");
      atmos_on=0;
      sprintf(line,"ATMOS disabled by %s\n",ustr[user].name);
    }
   else
    {
      write_str(user,"Atmospherics ON");
      atmos_on=1;
      sprintf(line,"ATMOS enabled by %s\n",ustr[user].name);
    }

 print_to_syslog(line);
 btell(user,line);
}	
		
toggle_allow(user,inpstr)
int user;
char *inpstr;
{
char line[132];

  if (allow_new)
    {
      write_str(user,"New users DISALLOWED");
      allow_new=0;
      sprintf(line,"NEW users disallowed by %s\n",ustr[user].name);
    }
   else
    {
      write_str(user,"New users ALLOWED");
      allow_new=1;
      sprintf(line,"NEW users allowed by %s\n",ustr[user].name);
    }

 print_to_syslog(line);
 btell(user,line);
}

/*--------------------------------------------------------------*/
/* selective line removal from files                            */
/*--------------------------------------------------------------*/

remove_lines_from_file(user, file, lower, upper)
int user;
char * file;
int lower;
int upper;
{

int mode  = 0;
char temp[FILE_NAME_LEN];
FILE *bfp,*tfp;

/*---------------------------------------------------------*/
/* determine the mode for line deletion                    */
/*---------------------------------------------------------*/

if (lower == -1 && upper == -1)       mode = 1;  /* all lines         */
else if (lower == upper)              mode = 2;  /* one line          */
else if (lower > 0 && upper == 0)     mode = 3;  /* to end of file    */
else if (upper > 0 && lower == 0)     mode = 4;  /* from beginning    */
else if (upper < lower)               mode = 5;  /* leave middle      */

/*---------------------------------------------------------*/
/* check to make sure the file exists                      */
/*---------------------------------------------------------*/

if (!(bfp=fopen(file,"r")))
  {
   write_str(user,"The file was empty, could not delete");
   return;
  }


/*-------------------------------------------*/
/* delete the entire file                    */
/*-------------------------------------------*/
if (mode == 1)
  {
   FCLOSE(bfp);
   unlink(file);
   return;
  }

/*---------------------------------------------------------*/
/* make temp file                                          */
/*---------------------------------------------------------*/

sprintf(t_mess,"%s/temp",MESSDIR);
strncpy(temp,t_mess,FILE_NAME_LEN);

if (!(tfp=fopen(temp,"w")))
  {
   write_str(user,"Sorry - Cannot open temporary file");
   logerror("Can't open temporary file");
   FCLOSE(tfp);
   return;
  }

/*------------------------------------------------------*/
/* get the right lines from the file                    */
/*------------------------------------------------------*/

switch(mode)
  {
    case 1: break;   /* already done */

    case 2:
            file_copy_lines(bfp, tfp, lower);
            file_skip_lines(bfp, (upper - lower) + 2 );
            file_copy_lines(bfp, tfp, 99999);
            break;

    case 3:
            file_copy_lines(bfp, tfp, lower);
            break;

    case 4:
            file_skip_lines(bfp, upper + 1);
            file_copy_lines(bfp, tfp, 99999);
            break;

    case 5:
            file_skip_lines(bfp, upper );
            file_copy_lines(bfp, tfp, (lower - upper) + 2 );
            break;

    default: break;
  }

FCLOSE(bfp);
FCLOSE(tfp);

unlink(file);

/*-----------------------------------------*/
/* copy temp file back into file           */
/*-----------------------------------------*/

if (!(bfp=fopen(file,"w")))
  {
   return;
  }

if (!(tfp=fopen(temp,"r")))
  {
   FCLOSE(bfp);
   return;
  }

file_copy(tfp, bfp);

FCLOSE(bfp);
FCLOSE(tfp);
unlink(temp);

}

/*---------------------------------------------*/
/* skip the number of lines specified          */
/*---------------------------------------------*/

file_skip_lines( in_file, lines)
FILE * in_file;
int lines;
{
int cnt = 1;
char c;

 while( cnt < lines )
   {
    c=getc(in_file);
    if (feof(in_file)) return;
    if (c == '\n') cnt++;
   }
}

/*-----------------------------------------------------------------*/
/* copy the number of lines specified from a file to a file        */
/*-----------------------------------------------------------------*/

file_copy_lines( in_file, out_file, lines)
FILE * in_file;
FILE * out_file;
int lines;
{
int cnt = 1;
char c;

 while( cnt < lines )
   {
    c=getc(in_file);
    if (feof(in_file)) return;

    putc(c, out_file);
    if (c == '\n') cnt++;
   }
}

/*---------------------------------------------*/
/* copy a file to another file                 */
/*---------------------------------------------*/

file_copy( in_file, out_file)
FILE * in_file;
FILE * out_file;
{
char c;
 c=getc(in_file);
 while( !feof(in_file) )
   {
    putc(c,out_file);
    c=getc(in_file);
   }
}

/*---------------------------------------------*/
/* count lines in a file                       */
/*---------------------------------------------*/

file_count_lines(file)
char * file;
{
 int lines = 0;
 char c[257];
 FILE * bfp;


 if (!(bfp=fopen(file,"r")))
   {
    return(0);
   }

 fgets(c, 256, bfp);

 while( !feof(bfp) )
   {
    if (strchr(c, '\n') != NULL) lines ++;
    fgets(c, 256, bfp);
   }

 FCLOSE(bfp);
 return(lines);
}

/*---------------------------------------------*/
/*get upper/lower bounds                       */
/*---------------------------------------------*/

get_bounds_to_delete(str, lower, upper, mode)
char * str;
int  * lower;
int  * upper;
int  * mode;

{

char token1[20];

int  val_1 = -1;
int  val_2 = -1;

lower[0] = -1;
upper[0] = -1;
mode[0]  = 0;

if (strlen(str))
  {
    sscanf(str,"%s ",t_mess);
    remove_first(str);
    strncpy(token1, t_mess, 19);
    sscanf(token1, "%d", &val_2);
    if (strlen(str))
      {
       sscanf(str,"%s ",t_mess);
       sscanf(t_mess, "%d", &val_1);
     }
    else
     {
      t_mess[0] = 0;
      val_1 = -1;
     }

    /*------------------*/
    /* delete all lines */
    /*------------------*/
    if (strcmp(token1,"all") == 0)
      {
       lower[0] = 0;
       upper[0] = 0;
       mode[0] = 1;
       return;
      }

    /*------------------*/
    /* delete to end    */
    /*------------------*/
    if (strcmp(token1,"from") == 0)
      {
       if (val_1 > 0)
        {
         lower[0] = val_1;
         upper[0] = 0;
         mode[0] = 3;
        }
       return;
      }

    /*-------------------------*/
    /* delete from begining    */
    /*-------------------------*/
    if (strcmp(token1,"to") == 0)
      {
       if (val_1 > 0)
        {
         lower[0] = 0;
         upper[0] = val_1;
         mode[0]  = 4;
        }
       return;
      }
    /*-------------------------*/
    /* delete from begining    */
    /*-------------------------*/
    if (val_2 > 0)
      {
       if (val_1 > 0)
        {
         lower[0] = val_1;
         upper[0] = val_2;
         if (val_1 == val_2)
           mode[0] = 2;
          else
           mode[0]  = 5;
         return;
        }
       lower[0] = val_2;
       upper[0] = val_2;
       mode[0]  = 2;
       return;
      }
  }
 else
  {
   /* delete the first line (default) */
   lower[0] = 1;
   upper[0] = 1;
   mode[0]  = 2;
  }

}

/*--------------------------------------------------------------*/
/* create the away from keyboard command                        */
/*--------------------------------------------------------------*/

set_afk(user, inpstr, mode)
int user;
char *inpstr;
int mode;
{
int i;

i = rand() % NUM_IDLE_LINES;

sprintf(mess, idle_text[i], ustr[user].name);
writeall_str(mess,1,user,0,user,NORM,AFK_TYPE,0);
write_str(user,mess);

ustr[user].afk = mode;
}

/*--------------------------------------------------------------*/
/* create the boss - away from keyboard command                 */
/*--------------------------------------------------------------*/

set_bafk(user, inpstr)
int user;
char *inpstr;
{
cls(user,"clear");
ustr[user].afk = 2;
set_afk(user,inpstr,2);
}


/*--------------------------------------------------------------*/
/* meter command                                                */
/*--------------------------------------------------------------*/

meter(user, inpstr)
int user;
char *inpstr;
{
char graph[26];

sprintf(mess,"+-------------------------------------------+");
write_str(user, mess);

sprintf(mess,"|         Activity meter                    |");
write_str(user, mess);

sprintf(mess,"+-------------------------------------------+");
write_str(user, mess);

sprintf(mess,"| Commands this period: %4d                |",commands);
write_str(user, mess);

sprintf(mess,"|                                           |");
write_str(user, mess);

fill_bar(says, commands, graph);
sprintf(mess,"| says  %s              |", graph);
write_str(user, mess);

fill_bar(tells, commands, graph);
sprintf(mess,"| tells %s              |", graph);
write_str(user, mess);

sprintf(mess,"|                                           |");
write_str(user, mess);

sprintf(mess,"| Commands per minute ave: %4d             |",commands_running);
write_str(user, mess);

sprintf(mess,"| Tells per minute ave:    %4d             |",tells_running);
write_str(user, mess);

sprintf(mess,"| Says per minute ave:     %4d             |",says_running);
write_str(user, mess);

sprintf(mess,"+-------------------------------------------+");
write_str(user, mess);

}

fill_bar(val1, val2, str)
int val1;
int val2;
char * str;
{
 int i, j;

  strcpy(str,"|--------------------|");

  val1 = val1 * 100;
  val2 = val2;

  if (val2 == 0) return;
  i = (val1 / val2) / 5;

  if (i > 20) i = 20;

  if (i == 0) return;

  for(j = 0; j<i; j++)
   {
     str[1+j] = '#';
   }
}

/*------------------------------------------------*/
/* set car_return                                 */
/*------------------------------------------------*/
set_quota(user,inpstr)
int user;
char *inpstr;
{

  int value = 0;

  sscanf(inpstr,"%d", &value);

  if (value < 0)
    {
      value = 0;
    }

  sprintf(mess,"The new quota is: %d",value);
  write_str(user,mess);

  system_stats.logins_today++;
  system_stats.logins_since_start++;
  system_stats.new_users_today    = 0;
  system_stats.quota              = value;
}

/*------------------------------------------------*/
/* set car_return                                 */
/*------------------------------------------------*/
/* IMPORTANT: make sure there are at least 10     */
/* extra bytes in the string to append on         */
/*------------------------------------------------*/
add_hilight(str)
char *str;
{
char buff[ARR_SIZE];
strcat(str,"\033[0m");

strcpy(buff,str);
strcpy(str,"\033[1m");
strcat(str,buff);

}


hilight(user,inpstr)
int user;
char *inpstr;
{
  int area;

  says++;
  area = ustr[user].area;

  sprintf(mess,VIS_SAYS,ustr[user].name,inpstr);
  mess[1]=toupper(mess[1]);
  write_hilite(user,mess);	
		
  if (!ustr[user].vis)
    sprintf(mess,INVIS_SAYS,inpstr);
			
  writeall_str(mess,1,user,0,user,BOLD,SAY_TYPE,0);

/*--------------------------------*/
/* store say to the review buffer */
/*--------------------------------*/
  strncpy(conv[area][astr[area].conv_line],mess,MAX_LINE_LEN);
  astr[area].conv_line=(++astr[area].conv_line)%NUM_LINES;	
}


/*-----------------------------------------------*/
/* x communicate a user                          */
/*-----------------------------------------------*/
xcomm(user,inpstr)
int user;
char *inpstr;
{
char other_user[ARR_SIZE];
int u;

if (!strlen(inpstr))
  {
   write_str(user,"      Users Currently X COMMed");
   write_str(user,"-------------------------------------");
   for (u=0;u<MAX_USERS;++u)
    {
     if (ustr[u].suspended  && ustr[u].area > -1)
       {
        write_str(user,ustr[u].name);
       };
    }
   write_str(user,"(end of list)");
   return;
  }

sscanf(inpstr,"%s ",other_user);
strtolower(other_user);

if ((u=get_user_num(other_user,user))== -1)
  {
   not_signed_on(user,other_user);
   return;
  }

if (u == user)
  {
   write_str(user,"You are definitly wierd! Trying to xcom yourself, geesh.");
   return;
  }


if (ustr[user].super < ustr[u].super)
  {
   write_str(user,"That was not wise...it backfired and now you are xcommed.");
   ustr[user].suspended = 1;

   sprintf(mess,"%s wanted to xcom you!",ustr[user].name);
   mess[0]=toupper(mess[0]);
   write_str(u,mess);
   return;
  }

if (ustr[u].suspended)
  {
    ustr[u].suspended = 0;
    write_str(u,"You have been returned to the living users. Play nice or else.");
    sprintf(mess,"XCOM OFF: %s by %s\n",ustr[u].name, ustr[user].name);
  }
 else
  {
    ustr[u].suspended = 1;
    write_str(u,"I suspect you made somebody mad. Now you are a zombie.");
    sprintf(mess,"XCOM ON: %s by %s\n",ustr[u].name, ustr[user].name);
  }

mess[0]=toupper(mess[0]);

print_to_syslog(mess);
btell(user, mess);

write_str(user,"Ok");
}

/*-------------------------------------------------------------------*/
/* set up sockets for use                                            */
/*-------------------------------------------------------------------*/
int
set_up_socket(port,
              port_offset,
              text,
              sock_addr,
              sock)
      int                   port;            /* port to base services on     */
      int                   port_offset;     /* offset from base port        */
      char                * text;            /* text line for display        */
      struct sockaddr_in  * sock_addr;       /* socksddr structure           */
      int                 * sock;            /* socket created               */
{
int on = 1;
int open_port = port + port_offset;
int size=sizeof(struct sockaddr_in);

/*---------------------------------------*/
/* status text                           */
/*---------------------------------------*/

printf("%s:\n\n",text);
printf("   use port: %d\n",open_port);

/*-------------------------------------------------*/
/* get a socket for use                            */
/*-------------------------------------------------*/

if ((*sock = socket(AF_INET,SOCK_STREAM,0))== -1)
 {
  printf("   ***CANNOT OPEN PORT***\n");
  return(1);
 }

/*-------------------------------------------------*/
/* bind the socket                                 */
/*-------------------------------------------------*/

sock_addr->sin_family      = AF_INET;
sock_addr->sin_addr.s_addr = htonl(INADDR_ANY);
sock_addr->sin_port        = htons(open_port);  /* ntohs */

setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));

if (bind(*sock, sock_addr, size)== -1)
  {
   printf("   ***CANNOT BIND TO PORT***\n");
   return(2);
  }

/*-------------------------------------------------*/
/* listen to the socket                            */
/*-------------------------------------------------*/

if (listen(*sock, 5)== -1)
  {
   printf("   ***LISTEN FAILED ON PORT***\n");
   return(3);
  }

/*----------------------------*/
/* set socket to non_blocking */
/*----------------------------*/
fcntl(*sock, F_SETFL, O_NDELAY);

/* vax users change the above line to:       */
/* socket_ioctl(listen_sock, FIONBIO, &arg); */
/* declare arg int arg = 1                   */

printf("   port created, bound, and listening\n\n");

return(0);
}


/*--------------------------------------------------------*/
/* logic to detemine if message is to be ignored          */
/*--------------------------------------------------------*/
int
user_wants_message(user,type)
  int user;
  int type;
{
 if (ustr[user].flags[type] == '1')
  return(0);
 else
  return(1);
}

/*---------------------------------------------------------*/
/* get flags position                                      */
/*---------------------------------------------------------*/
int
get_flag_num(inpstr)
char *inpstr;
{
char comstr[ARR_SIZE];
int f;

sscanf(inpstr,"%s",comstr);
if (strlen(comstr)<2) return(-1);

for (f=0; flag_names[f].text[0]; ++f)
  {
   if (!instr(0,flag_names[f].text,comstr) )
     return f;
  }

return -1;
}

/*----------------------------------------------------------*/
/* set listening flags                                      */
/*----------------------------------------------------------*/
user_listen(user,inpstr)
int user;
char *inpstr;
{
int u;

if (!strlen(inpstr))
  {
   write_str(user,"+------------------------------------+");
   write_str(user,"| You can hear:                      |");
   write_str(user,"+------------------------------------+");
   for (u=1;u<NUM_IGN_FLAGS;++u)
    {
     if (ustr[user].flags[u] != '1')
       {
        write_str(user,flag_names[u].text);
       };
    }
   write_str(user,"(end of list)");
   return;
  }

if (!instr(0,"all",inpstr) )
 {
  strcpy(ustr[user].flags,"                       ");
  write_str(user,"You will now hear all messages.");
  return;
 }

u = get_flag_num(inpstr);
if (u > -1)
  {
   ustr[user].flags[u] = ' ';

   sprintf(mess,"You are now listening to %s",flag_names[u].text);
   write_str(user,mess);
  }
 else
  {
   write_str(user,"That message type not known");
  }
		
}

/*----------------------------------------------------------*/
/* set ignoring flags                                       */
/*----------------------------------------------------------*/
user_ignore(user,inpstr)
int user;
char *inpstr;
{
int u;

if (!strlen(inpstr))
  {
   write_str(user,"+------------------------------------+");
   write_str(user,"| You are ignoring:                  |");
   write_str(user,"+------------------------------------+");
   for (u=1;u<NUM_IGN_FLAGS;++u)
    {
     if (ustr[user].flags[u] == '1')
       {
        write_str(user,flag_names[u].text);
       };
    }
   write_str(user,"(end of list)");
   return;
  }

if (!instr(0,"all",inpstr) )
 {
  strcpy(ustr[user].flags,"11111111111111111111111");
  write_str(user,"You are now ignoring all messages.");
  return;
 }

u = get_flag_num(inpstr);
if (u > -1)
  {
   ustr[user].flags[u] = '1';

   sprintf(mess,"You are now ignoring %s.", flag_names[u].text);
   write_str(user,mess);
  }
 else
  {
   write_str(user,"That message type not known");
  }
		
		
}


/*** prints who is on the system to requesting user ***/
external_www(as)
int as;
{
int s,u,tm,min,idl,invis=0;
char *list="NY";
char l,ud[DESC_LEN],un[NAME_LEN],an[NAME_LEN],und[80];
char temp[256];
char i_buff[5];

while(read(as, temp, 1));

/* print_to_syslog(temp); */

write_it(as, "HTTP/1.0 200 OK\n" );
write_it(as, "Server: Iforms/1.12\n");
write_it(as, "Date: Wednesday, 10-May-95 04:25:34 GMT\n");
write_it(as, "Last-modified: Friday, 05-May-95 23:15:01 GMT\n");
write_it(as, "Content-type: text/html\n\n" );
/* Content-length: 1998 */

write_it(as, "<HTML>\n" );

write_it(as, "<HEAD>\n" );
write_it(as, "<TITLE> IForms Mini WWW port </title>\n" );
write_it(as, "</HEAD>\n");

write_it(as, "<BODY>\n");

write_it(as, "<h1> Users currently logged on </h1> \n");
write_it(as, "");

/*-------------------------------------------------------------------------*/
/* write out title block                                                   */
/*-------------------------------------------------------------------------*/
write_it(as, "<pre>\n");
if (EXT_WHO1) {write_it(as,EXT_WHO1);}
if (EXT_WHO2) {write_it(as,EXT_WHO2);}
if (EXT_WHO3) {write_it(as,EXT_WHO3);}
if (EXT_WHO4) {write_it(as,EXT_WHO4);}

/* display current time */
time(&tm);
sprintf(mess,"Current users on %s\n",ctime(&tm));
write_it(as, mess);

/* Give Display format */
sprintf(mess,"Room        Name/Description                     Time On - Listening - Idle\n");
write_it(as, mess);

/* display user list */
for (u=0;u<MAX_USERS;++u) {
	if (ustr[u].area!= -1)
	  {
		if (!ustr[u].vis)
	          {
	            invis++;
	            continue;
	          }
			
		min=(tm-ustr[u].time)/60;
		idl=(tm-ustr[u].last_input)/60;
		
		strcpy(un,ustr[u].name);
		strcpy(ud,ustr[u].desc);
		
		strcpy(und,un);
		strcat(und," ");
		strcat(und,ud);
		
		if (!astr[ustr[u].area].hidden)
		  {
		    strcpy(an,astr[ustr[u].area].name);
		  }
		 else
		  {
		    strcpy(an, "        ");
		  }
		
		l=list[ustr[u].listen];
		
		s=' ';
		
		if (ustr[u].afk == 1)
		  strcpy(i_buff,"AFK ");
		 else
		  if (ustr[u].afk == 2)
		    strcpy(i_buff,"BAFK");
		   else
		    strcpy(i_buff,"Idle");
		
		sprintf(mess,"%-12s%c %-39.39s %-5.5d %c %s %3.3d\n",an,s,und,min,l,i_buff,idl);
	        mess[14]=toupper(mess[14]);
	
		if (!ustr[u].vis) mess[13]='~';
		
		strncpy(temp,mess,256);
		strtolower(temp);
		
		mess[0]=toupper(mess[0]);
		write_it(as,mess);
	       }
	}
write_it(as, "</pre>\n");

if (invis)
  {
   sprintf(mess,"There are %d invisible users.<br>\n",invis);
   write_it(as,mess);
  }

write_it(as, "\n");
sprintf(mess,"Total of %d users signed on.<br>\n",num_of_users);
write_it(as, mess);
write_it(as, "\n\n");
write_it(as, "</body>\n");
write_it(as, "</html>\n\n");

sprintf(temp,"%c",26);
write_it(as,temp);

}

write_it(sock, str)
int sock;
char * str;
{
write(sock, str, strlen(str));
}



    /\
 /-/**\-\
| Necros |
 \-=##=-/
