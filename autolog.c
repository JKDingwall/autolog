/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 * version 0.42.1: James Dingwall <james.dingwall@ncrvoyix.com>
 */

/*****************************************************************************/
/*									     */
/*   Programm: autolog.c 	C-Programm to log out sleeping users	     */
/*             version 0.40						     */
/*									     */
/*   Autor:  Carsten Juerges    Erster Versuch:  06.04.2000		     */
/*	     Kurze Wanne 1      Letztes Update:  28.04.2000		     */
/*	     30926 Seelze						     */
/*	     Germany            send me a postcard, if you like. :)	     */
/*	     juerges@cip-bau.uni-hannover.de 				     */
/*									     */
/*   Rechner:  AMD K6-2		SuSE Linux 6.3				     */
/*									     */
/*   Meldung:  This version seems to worke quite good.			     */
/*									     */
/*   Auftauchende Probleme, known problems / features:			     */
/*	- a program without any login (e.g. start Netscape and log off )     */
/*	  will be kicked out as lost process.				     */
/*	- to become not idle he has to type something into a terminal	     */
/*	  when in beeps somewhere.                                           */
/*	- some lines "//##" have to be adapted to your specific linux.       */
/*	  maybe your ps has different options or a different output.         */
/*	- Looks quite dirty, should be beautified, sometimes :)		     */
/*									     */
/*   Warning:								     */
/*      - Keep in mind: Someone who is kicked off might lose some data.      */
/*      - This program is distributed in the hope that it will be useful,    */
/*        but WITHOUT ANY WARRANTY; without even the implied warranty of     */
/*        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	     */
/*        GNU General Public License for more details.			     */
/*									     */
/*****************************************************************************/
/*  "@(#) autolog.c      0.34
    Originally ported     by David Dickson"
    Modified		  by Michael C. Mitchell, 15Oct94
    Rewritten		  by Kyle Bateman,	  Nov94, Aug95
    Adapted to my System, by Carsten Juerges,	  Apr 2000
*/

#include    <stdio.h>
#include    <signal.h>
#include    <string.h>
#include    <sys/types.h>
#include    <sys/stat.h>
#include    <utmp.h>
#include    <time.h>
#include    <pwd.h>
#include    <grp.h>
#include    <regex.h>
#include    <malloc.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <unistd.h>

#define     D_IDLE      30  /* maximum idle time (minutes)		*/
#define     D_GRACE    120  /* grace time (sec) for user reply		*/
#define     D_HARD       0  /* consider connect time only		*/
#define     D_MAIL       1  /* notify user by mail			*/
#define     D_CLEAR      0  /* clear user terminal before warning	*/
#define     D_WARN       1  /* warn user before killing			*/
#define     D_LOG        1  /* log activity in logfile			*/

#define     WARNING      1  /* a warning message			*/
#define     LOGOFF       2  /* a log-off message			*/
#define     NOLOGOFF     3  /* a log-off failure message		*/
#define     ANGRY        4  /* a log-off user returned message		*/
#define     IDLEPUNISH   5  /* punish idling-users message		*/

#define     HUP_WAIT	20  /* time to wait after SIGHUP  (in sec.)	*/
#define     KILLWAIT	 3  /* time to wait after SIGKILL (in sec.)	*/
#define     Max_Sleep 1800  /* max. sleeping time for this daemon ;-)	*/
#define     ChckSleep   90  /* check whether user logged in again	*/
#define     Idle_Ban   120  /* punishment-time for idle users		*/
//#define     HUP_WAIT	 2  /* (demo / debug)				*/
//#define     KILLWAIT	 1  /* (demo / debug)				*/
//#define     Max_Sleep	10  /* (demo / debug)				*/
//#define     ChckSleep	10  /* (demo / debug)				*/


#define     STRLEN      64
#define     LINELEN     256
#define     MAXCONF     512 /* maximum lines in config file */

typedef struct {
			    /* Name of user.				*/
	char   Name[UT_NAMESIZE + 1];
			    /* Output device with the minimum idle-time	*/
	char   Device[UT_LINESIZE + 1];
			    /* "." serves as flag that user logged out, */
			    /*     when chasing lost processes.         */
	uid_t  UserID;	    /* to check whether user has changed	*/
	int    IdleTime;    /* Minimum-time the user is idle		*/
	int    SessStrt;    /* Starttime of the users session	 	*/
	int    Ban_Ends;    /* Endtime of banning user this user	*/
	int    WarnEnds;    /* End of grace-time (after warning).	*/
} userdata;		    /*   e.g. 30 Mins after hardkick.           */

struct utmp *utmpp;         /* pointer to utmp file entry */
char delims[] = "\n\t ";    /* valid delimiters in config file */
char anystrg[] = ".+";      /* matches anything */

int debug	= 0;
int resist	= 1;	    /* 0=> ordin. program 1=> behave as daemon. */
int do_bite	= 1;
int listall	= 0;
char *confname	= "/etc/autolog.conf";
char *logfname	= "/var/log/autolog.log";
char *datfname	= "/var/lib/autolog/autolog.data";
int g_idle	= D_IDLE;
int g_grace	= D_GRACE;
int g_hard	= D_HARD;
int g_mail	= D_MAIL;
int g_clear	= D_CLEAR;
int g_warn	= D_WARN;
int g_log	= D_LOG;

typedef struct conf {
	char	*name;	/* user name			*/
	char	*group;	/* group name			*/
	char	*line;	/* tty line			*/
	int	idle;	/* minutes of idle time		*/
	int	grace;	/* minutes of grace time	*/
	int	ban;	/* ban user for ... minutes	*/
	int	hard;	/* consider connect time only	*/
	int	mail;	/* notify by mail		*/
	int	clear;	/* clear screen before warn	*/
	int	warn;	/* warn before kill		*/
	int	log;	/* log actions to logfile	*/
} conf_el;
conf_el c_arr[MAXCONF];

int sleeptime	= Max_Sleep; /* maximum amount of time	*/
int sleep_max	= Max_Sleep; /*  to sleep for daemon.	*/
int c_idx	= 0;

int	  userfill = 0;	 /* different users found.	*/
int	  usermax = 0;	 /* current size of List.	*/
userdata* userlst;	 /* Store user-Data here.	*/

int  	  ids_fill;
int	  ids_max;
int	  *ids_lst;

time_t    time();	 /* they are used so often      */
time_t    pres_time;     /* current time.		*/
time_t    chck_pid = 0;	 /* don't check pids before ... */
int       lostkill = 1;	 /* 1 => kill lost processes.   */

char      *ps_cmd;

static int check_utmp();
static int show_results();
static int save_users();
static int load_users();
static int set_defs(int);
static int eat_confile();
static int pat_match(char *, char *);
static int check_idle(userdata *);
static void get_PIDs(char *, char *);
static int kill_PIDs(char *, char *);
static int kill_HUP(int);
static int kill_KILL(int);
static int kill_Res(int);
static int mesg(int, char *, char *, int, int, conf_el *);
static int log_msg(char *);
static int bailout(char *, int);
static int lower_sleep(int);
static int kill_lost_PIDs();

extern int
main(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch(tolower(argv[i][1])) {
			case 'r': resist  = 1;			   break; /* debug */
			case 'o': resist  = 0;			   break;
			case 'a': listall = 1;			   break;
			case 'b': do_bite = 0;/* I got used to */  break;
			case 'd': debug   = 1;			   break;
			case 'h': g_hard  = 1;			   break;
			case 'n': do_bite = 0;			   break;
			case 'l': logfname = argv[++i];		   break;
			case 'f': confname = argv[++i];		   break;
			case 't': g_idle = atoi(argv[++i]);	   break;
			case 'g': g_grace = atoi(argv[++i]);	   break;
			case 'm': g_mail  = (argv[++i][0] == 'y'); break;
			case 'c': g_clear = (argv[++i][0] == 'y'); break;
			case 'w': g_warn  = (argv[++i][0] == 'y'); break;
			case 'L': g_log   = (argv[++i][0] == 'y'); break;
			default:
				fprintf(stderr, "autologout: illegal switch: %s\n", argv[i]);
			}
		} else {
			fprintf(stderr, "autologout: illegal parameter: %s\n", argv[i]);
		}
	} /* for */
	ps_cmd = "ps axo user:32,pid,tty";

	time(&pres_time);		  /* get current time for log-file */
	log_msg("Starting Service");
	eat_confile();			  /* read config file		    */
	if (!debug)			  /* if not in debug mode,	    */
		if (fork())
			exit(0);	  /* the parent process exits here. */
//	if (!debug && resist)		  /* if not in debug mode,	    */
//		if (fork())
//			 exit(0);	  /* the parent process exits here. */

	userfill = 0;
	usermax  = 8;
	userlst  = (userdata *)calloc(sizeof(userdata), 1 + usermax);

	load_users();
	if (debug) {
		printf("\n");
		printf("From the datafile:\n");
		printf("==================\n");
		show_results();
		printf("Done reading datafile\n");
	}

	/* now sit in an infinite loop and work */
	do {
		time(&pres_time);			   /* get current time       */
		sleeptime = sleep_max;			   /* assume max. sleeptime. */
		setutent();
		if (debug)
			printf("\n");
		while ((utmpp = getutent()) != (struct utmp *) NULL)
			check_utmp();
		if (debug)
			show_results();
		for (i = 1; i <= userfill; i++) {
			if (check_idle(&userlst[i]) == 2) {
				if (debug)
					printf("knock out.\n");
				userlst[i] = userlst[userfill];
				userfill--;
			}
		}

		/*.. check for lost processes, from time to time. ...........................*/
		if (lostkill && pres_time>chck_pid) {
			if (debug)
				printf("check for dead processes.\n");
			kill_lost_PIDs();
			chck_pid = pres_time + 10 * 60;

			if (debug)
				printf("next check for processes after %10ld.\n", chck_pid);
		}
		if (debug) {
			//show_results();
			printf("daemons maximum sleeptime %7d seconds, today.\n", sleep_max);
			printf("daemon is falling asleep for %5d seconds, now.\n", sleeptime);
		}
		if  (resist || (sleeptime < sleep_max) )
			sleep(sleeptime);
	} while (resist ||                 /* Stay when in daemon-mode           */
            (sleeptime < sleep_max) ); /*      or users are to be kicked off */
	save_users();
	log_msg("Stopped service");
}

/***************************************************************
* This function makes sure we will find in our table:          *
*    - shortest idle-time and device of this.                  *
*    - earliest start-time of all logins.                      *
***************************************************************/

static int
check_utmp()
{
	/* select utmp entries needing killing */
	char dev[STRLEN], name[STRLEN], prname[STRLEN];
	char *msg;
	char tmp_str[80];

	struct stat status;
	time_t idle, atime;

	int userpos = 0;	/* position of user found, 0 => not found */

	snprintf(prname, sizeof(prname), "/proc/%d", utmpp->ut_pid);   /* append /proc/ to proclist */
	snprintf(dev, sizeof(dev), "/dev/%s", utmpp->ut_line);  /* append /dev/ to base name */
	msg = "";

	/*.. Get information about the current process. .............................*/
	if (stat(prname, &status))
		msg = "Dead process:    ";
	if (utmpp->ut_type != USER_PROCESS)
		msg = "Non-user process:";
	if (strlen(msg)) {
		if (listall)
			printf("%s N:%-8s P:%5d Login:%s",
			       msg,
			       utmpp->ut_user,
			       utmpp->ut_pid,
			       ctime(&utmpp->ut_time));
		return 0;
	}
	if (dev[5] != ':' && stat(dev, &status)) {
		/* if can't get status for port */
		snprintf(tmp_str, 79, "Can't get status of user's terminal: %s", dev);
		bailout(tmp_str, 1);
		return 0;
	}

	/*.. Find out everything about current time, sleep time and session time. ...*/
	if (utmpp->ut_time > status.st_atime)	   /* get last access time   */
		atime = utmpp->ut_time;		   /* proc - starttime 	     */
	else
		atime = status.st_atime;	   /* proc - accesstime      */
	idle = (pres_time - atime) / 60;	   /* total idle minutes     */

	/*.. Find out username and groupname of this login. .........................*/
	strncpy(name, utmpp->ut_user, UT_NAMESIZE); /* get user name         */
	name[UT_NAMESIZE] = '\0';         /* null terminate user name string */

	if (listall)
		printf("Checking: %-11s on %-12s I:%4ld  T:%d Login: %s",
		       name,dev,
		       idle,
		       utmpp->ut_type,
		       ctime(&utmpp->ut_time));

	strncpy(userlst[0].Name, name, UT_NAMESIZE);
	userlst[0].Name[UT_NAMESIZE] = '\0';
	strncpy(userlst[0].Device, utmpp->ut_line, UT_LINESIZE);
	userlst[0].Device[UT_LINESIZE] = '\0';
	userlst[0].IdleTime = idle;

	/*.. Get Position of user in userlst. .......................................*/
	userpos = userfill;
	while ( strncmp(userlst[userpos].Name, name, UT_NAMESIZE) )
		userpos--;

	/*.. if not found -> add user to userlst. ...................................*/
	if ( userpos == 0 ) {
		userfill++;
		if (userfill > usermax) {
			usermax = 2 * usermax;
			userlst = (userdata *)realloc(userlst, sizeof(userdata) * (1 + usermax));
		}
		strncpy(userlst[userfill].Name, name, UT_NAMESIZE);
		userlst[userfill].Name[UT_NAMESIZE] = '\0';
		strncpy(userlst[userfill].Device, utmpp->ut_line, UT_LINESIZE);
		userlst[userfill].Device[UT_LINESIZE] = '\0';
		userlst[userfill].IdleTime = idle;
		userlst[userfill].UserID = status.st_uid;
		userlst[userfill].SessStrt = utmpp->ut_time;
		userlst[userfill].WarnEnds = 0;
	} else {
		if (userlst[userpos].IdleTime > idle) {  /* less idle-time found. ....*/
			userlst[userpos].IdleTime = idle;
			strncpy(userlst[userpos].Device, utmpp->ut_line, UT_LINESIZE);
			userlst[userpos].Device[UT_LINESIZE] = '\0';
		}
	    	if (userlst[userpos].SessStrt > utmpp->ut_time) {  /* earlier start. */
			userlst[userpos].SessStrt = utmpp->ut_time;
		}
	}
}

static int
show_results()
{
	int userpos = 0;	/* position of user found, 0 => not found */

	printf("\nfound: %2d\n\n", userfill);
	printf("     Username  UserID Terminal     idle Starttime  BanningEnd WarningEnd\n");

	for (userpos = 1; userpos<=userfill; userpos++) {
		printf(" -->  %-10s %4d  %-10s %5d %10d %10d %10d\n",
		       userlst[userpos].Name,
		       userlst[userpos].UserID,
		       userlst[userpos].Device,
		       userlst[userpos].IdleTime,userlst[userpos].SessStrt,
		       userlst[userpos].Ban_Ends,userlst[userpos].WarnEnds);
	}
}

static int
save_users()
{
	int userpos = 0;	/* position of user found, 0 => not found */
	FILE* f;

	if (!(f = fopen(datfname, "w+"))) {
		bailout("Can't create data-file.", 6);
		return 0;
	}

	fprintf(f, "%d\n", userfill);

	for (userpos = 1; userpos <= userfill; userpos++) {
		fprintf(f, " %-10s %4d %-10s %5d %10d %10d %10d\n",
			userlst[userpos].Name,
			userlst[userpos].UserID,
			userlst[userpos].Device,
			userlst[userpos].IdleTime,
			userlst[userpos].SessStrt,
			userlst[userpos].Ban_Ends,
			userlst[userpos].WarnEnds);
	}
	fclose(f);
}

static int
load_users()
{
	int userpos = 0;	/* position of user found, 0 => not found */
	FILE* f;

	/*.. if no table exists, no problem, just return. ...........................*/
	if (!(f = fopen(datfname, "r")))
		return 0;

	/*.. check, how many lines with data will follow. ...........................*/
	fscanf(f, "%d", &userfill);

	/*.. if necessary extend list. ..............................................*/
	if (userfill > usermax) {
		usermax = 2 * userfill;
		userlst = (userdata *)realloc(userlst, sizeof(userdata) * (1 + usermax));
	}

	/*.. read all the data-lines. ...............................................*/
	for (userpos = 1; userpos <= userfill; userpos++) {
		fscanf(f, " %s %d %s %d %d %d %d",
		       userlst[userpos].Name,
		       &userlst[userpos].UserID,
		       userlst[userpos].Device,
		       &userlst[userpos].IdleTime,
		       &userlst[userpos].SessStrt,
		       &userlst[userpos].Ban_Ends,
		       &userlst[userpos].WarnEnds);
	}
	fclose(f);
}

/*---------------------------------------------------------------------------*/
/*===    The following functions cope with the config-file.               ===*/
/*---------------------------------------------------------------------------*/

static int
set_defs(int i)
{
	c_arr[i].name  = anystrg;
	c_arr[i].group = anystrg;
	c_arr[i].line  = anystrg;
	c_arr[i].idle  = g_idle;
	c_arr[i].ban   = 0;
	c_arr[i].grace = g_grace;
	c_arr[i].hard  = g_hard;
	c_arr[i].mail  = g_mail;
	c_arr[i].clear = g_clear;
	c_arr[i].warn  = g_warn;
	c_arr[i].log   = g_log;
}

static int
eat_confile()
{
	FILE *f;
	char *s, iline[LINELEN];
	int i, lev;
	int idle;

	if (!(f=fopen(confname, "r")) ) {
		if (debug)
			printf("Can't find file: %s\n", confname);
	} else {
		while (fgets(iline, LINELEN, f)) {
			if (*iline == '#' || strlen(iline) <= 1)
				continue;

			/*.. check for  ps=command in config-file, extract command. ...............*/
			s = strstr(iline, "ps=");
			if (s) {
				ps_cmd = strcpy((char *)malloc(strlen(s) + 1), s + 3);
				continue;
			}
			s = strstr(iline, "nolostkill");
			if (s) {
				lostkill = 0;
				continue;
			}
			set_defs(c_idx);
			s = strtok(iline, delims);
			do {
				lev = 1;
				if (!strncmp(s, "name=", 5) && *(s += 5))
					c_arr[c_idx].name = strcpy((char *)malloc(strlen(s) + 1), s);
				else if (!strncmp(s, "group=", 6) && *(s += 6))
					c_arr[c_idx].group = strcpy((char *)malloc(strlen(s) + 1), s);
				else if (!strncmp(s, "line=", 5) && *(s += 5))
					c_arr[c_idx].line = strcpy((char *)malloc(strlen(s) + 1), s);
				else if (!strncmp(s, "idle=", 5) && *(s += 5))
					c_arr[c_idx].idle = atoi(s);
				else if (!strncmp(s, "grace=", 6) && *(s += 6))
					c_arr[c_idx].grace = atoi(s);
				else if (!strncmp(s, "ban=", 4) && *(s += 4))
					c_arr[c_idx].ban =atoi(s);
				else {
					if (!strncmp(s, "no", 2)) {
						lev = 0;
						s += 2;
					}
					if (!strcmp(s, "hard"))
						c_arr[c_idx].hard = lev;
					else if (!strcmp(s, "clear"))
						c_arr[c_idx].clear = lev;
					else if (!strcmp(s, "mail"))
						c_arr[c_idx].mail = lev;
					else if (!strcmp(s, "warn"))
						c_arr[c_idx].warn = lev;
					else if (!strcmp(s, "log"))
						c_arr[c_idx].log = lev;
					else if (debug)
						printf("Unknown token in file: %s: %s\n", confname, s);
				}
			} while (s = strtok(0, delims));
			idle = c_arr[c_idx].idle;

			/*.. Maybe it is necessary to reduce the max. sleeptime to shortest session. */
			if (0 < idle)				 /* but not to zero seconds. */
				if (60 * idle < sleep_max)
					sleep_max = 60 * idle;
			c_idx++;
		}
		fclose(f);
	}
	if (!c_idx)                 /* if no entries made yet */
		set_defs(c_idx++);      /* make one */
	if (debug) {
		printf("\nConfig File:\n");
		printf("name     group    line  idle grace mail warn log hard  ban\n");
		printf("----------------------------------------------------------\n");
		for(i = 0; i < c_idx; i++)
			printf("%-8s %-8s %-6s %3d %5d %4d %4d %3d %4d %4d\n",
			       c_arr[i].name,
			       c_arr[i].group,
			       c_arr[i].line,
			       c_arr[i].idle,
			       c_arr[i].grace,
			       c_arr[i].mail,
			       c_arr[i].warn,
			       c_arr[i].log,
			       c_arr[i].hard,
			       c_arr[i].ban);
		printf("\n");
		printf("[%s]\n", ps_cmd);
		if (lostkill)
			printf("Lost processes will be killed.\n");
		else
			printf("Lost processes stay alive.\n");

	}
}

/* I've apparently found some bug in certain LINUX versions of the */
/* regular expressions library routines.  One of these following two */
/* versions of the routine should work for you. */
/* #define AVOID_REGEX_BUG*/
#ifdef AVOID_REGEX_BUG				/* some strange bug in re_exec */
static int
pat_match(char *pattern, char *strg)
{
	struct re_pattern_buffer rpb;
	int len, retval = 0;

	/*if (debug)
		printf("pat_match:%s:%s:\n", pattern, strg);*/
	len = strlen(pattern + 256);
	rpb.buffer = malloc(len);
	rpb.allocated = len;
	rpb.fastmap = 0;
	rpb.translate = 0;
	if (!re_compile_pattern(pattern, strlen(pattern), &rpb)) {
		if (re_match(&rpb, strg, strlen(strg), 0, 0) == strlen(strg))
			retval = 1;
        }
	free(rpb.buffer);
	return retval;
}
#else
/* return true if strg matches the regex in pattern */
static int
pat_match(char *pattern, char *strg)
{
	re_comp(pattern);
	return re_exec(strg);
}
#endif

/*---------------------------------------------------------------------------*/
/*===    This function checks whether we have to kill a session.          ===*/
/*---------------------------------------------------------------------------*/

static int
check_idle(userdata *akt_usr)
{
	char ddev[STRLEN], *gn = ".*";
	char dev[STRLEN], name[STRLEN], prname[STRLEN];
	int idle;
	struct stat status;
	time_t start, stime;
	struct passwd *passwd_entry;
	struct group *group_entry;
	conf_el *ce;
	int i;

	strncpy(name, akt_usr->Name, UT_NAMESIZE);
	strncpy(dev, akt_usr->Device, UT_LINESIZE);
	idle = akt_usr->IdleTime;
	stime = pres_time - akt_usr->SessStrt;

	akt_usr->IdleTime = 30000;	     /* 20 days will "clear" the idletime */

	if (debug)
		printf("\nuser: %-10s %-10s %-d\n", name, dev, idle);

	if(dev[0] == ':')   /* ignore xdm */
		return 0;

	snprintf(ddev, sizeof(ddev), "/dev/%s", dev);	  /* append /dev/ to base name */

	/*.. If user has logged out, check his Session time and so. .................*/

	if (debug)
		printf("now:  %ld = %s", pres_time, ctime(&pres_time));

	if (listall)
		printf("\nChecking: %-11s on %-12s I:%-4d\n", name, dev, idle);

	/* now try to find the group of this person */
	/* if usernames in utmp are limited to 8 chars, we will may fail on */
	/* names that are longer than this, so we'll try to find it by uid */
	if (!(passwd_entry = getpwnam(name)))         /* If can't find by name */
		passwd_entry = getpwuid(akt_usr->UserID); /* try by uid */
	if (passwd_entry) {
		strncpy(name, passwd_entry->pw_name, STRLEN);
		if (group_entry = getgrgid(passwd_entry->pw_gid))
			gn = group_entry->gr_name;
		else if (debug)
			printf("Can't find group entry for user: %s\n", name);
	}
	else if (debug) {
		printf("Can't find password entry for user: %s\n", name);
	}

	for(i = 0; i < c_idx; i++) {
		if (pat_match(c_arr[i].name, name) &&
		    pat_match(c_arr[i].group, gn) &&
		    pat_match(c_arr[i].line, dev)) {
//			if (debug)
//				printf("Match #%2d: U:%-12s Grp:%-8s Line:%-9s Pid:%-6d Sess:%3d:%02d\n",
//				       i+1, name, gn, utmpp->ut_line, utmpp->ut_pid, stime / 60, stime % 60);
			if (c_arr[i].idle < 0) {     /* if user exempt (idle<0) */
				if (debug) printf("User exempt\n");
				return 0;	/* then don't kill him */
			}
			ce = &c_arr[i];	/* get address of matched record */
			break;
		}
	}
	if (i >= c_idx) {
		if (debug)
			printf("No match for process\n");
		return 0;
	}

	/* device empty or new user => check whether banning is over. ...............*/
	if (stat(ddev, &status) || status.st_uid != akt_usr->UserID) {
	        if (pres_time>akt_usr->Ban_Ends) {	/* End of banning-time       */
        	    akt_usr->SessStrt = pres_time;	/* allow new session. 	     */
	            akt_usr->Ban_Ends = 0;		/*  (just for debugging).    */
		    akt_usr->WarnEnds = 0;	        /* user may return, now.     */
	            return 2;				/*  0 => user done.	     */
	        } else {
			lower_sleep(ChckSleep);		/* user might come back      */
		}

		strncpy(akt_usr->Device, ".", UT_LINESIZE); /* "Flag": user logged out.  */

		if (debug) {
			if (stat(ddev, &status)) {
				printf("user has logged out.\n");
			}
			else if (status.st_uid != akt_usr->UserID) {
				printf("%4d <> %4d\n", status.st_uid, akt_usr->UserID);
				printf("user has changed -> do not disturb.\n");
			}
		}
		return 0;
	}

	/* Check whether to knock out this user. ....................................*/
	if (!c_arr[i].hard) {	/* if considering idle time */
		if (akt_usr->Ban_Ends < pres_time) {
			if (idle < ce->idle) {		      /* if user was recently active */
				akt_usr->WarnEnds = 0;	      /* set to not warned.          */
				return 0;
			}
			if (debug)
				printf("Subject to logout   Idle time: %4d (%2d allowed)\n",
				       idle,ce->idle);
		}
		else if (debug) {
			printf("User temporary banned.\n");
		}
	} else {
	        int time_left;
		/*.. In case, he leaves now, his ban will end after his banning-time. .......*/
		/*.. This will also increase his banning-time in case he returnes to early. .*/
		/*.. Set end of banning to  current time + time to ban user. ................*/
		akt_usr->Ban_Ends = pres_time + 60 * ce->ban;
		if (debug)
			printf("Banning should end: %10d\n", akt_usr->Ban_Ends);

		/*.. Make sure, daemon doesn't sleep too long from now. .....................*/
		if (debug)
			printf("idle-state: %3d / %d:\n", idle, ce->idle);
		if (debug)
			printf("sess-state: %3ld / %d:\n", stime / 60, ce->idle);

		time_left = 60 * ce->idle - stime;
		if (debug)
			printf("time left: %4d sec.\n", time_left);
		if (0 < time_left) {			/* there is some time left   */
			lower_sleep(time_left);
			 return 0;
		} else {
			lower_sleep(ce->grace);		/* process is to be killed   */
		}

		if (debug)
			printf("Subject to logout   Total time: %4ld (%2d allowed)\n",
			       stime / 60, ce->idle);
	}
	if (stat(ddev, &status)) {
		bailout("Can't get status of user's terminal", 2);
		return 0;
	}


	/*.. action either warning or killing. */
	//    if (akt_usr->WarnEndsed && (pres_time > akt_usr->Ban_Ends) ) {
	if (0 < akt_usr->WarnEnds && (pres_time > akt_usr->WarnEnds) ) {
		if (debug)
			printf("Killing user, now.\n");

		if (kill_PIDs(name, dev) == 1) 	/* try to kill users' processes      */
			mesg(LOGOFF,  name, ddev, stime / 60, idle, ce); /* mail to user  */
		else
			mesg(NOLOGOFF, name, ddev, stime / 60, idle, ce); /* couldn't kill */
		if (ce->hard) {
			akt_usr->WarnEnds =-1;	/* if user returnes at ones -> angry */
		} else {
			akt_usr->WarnEnds =-2;
			akt_usr->Ban_Ends = pres_time /* give other users a chance     */
				+ Idle_Ban; /*  to catch this computer.      */
			lower_sleep(ChckSleep);
		}
	} else if (akt_usr->WarnEnds == -2) {    /* punish being idle for too long.   */
		mesg(IDLEPUNISH, name, ddev, stime / 60, idle, ce);
		akt_usr->WarnEnds = pres_time + 5;   /* give some time to read message */
		lower_sleep(5);
		if (debug)
			printf("idle-punish\n");
	} else if (akt_usr->WarnEnds==-1) {
		mesg(ANGRY, name, ddev, stime / 60, idle, ce);    /* angry about user. */
		if (debug)
			printf("I am angry, user returned...\n");
		if (kill_PIDs(name, dev) == 1) 		/* try to kill user, at once */
			mesg(LOGOFF, name, ddev, stime / 60, idle, ce); /* mail to user  */
		else
			mesg(NOLOGOFF, name, ddev, stime / 60, idle, ce); /* couldn't kill */
	} else {
		if (akt_usr->WarnEnds == 0) {
			mesg(WARNING, name, ddev, stime / 60, idle, ce);  /* try to warn user. */
			akt_usr->WarnEnds = pres_time + ce->grace;    /* wait until grace ends */
			if (debug)
				printf("Warning user:%s Line:%s  Sleep %d sec\n", name, dev, ce->grace);
		}
		lower_sleep(ce->grace);
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
/*===    This function finds out the PIDs of the current user.            ===*/
/*---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void
get_PIDs(char *u_name, char *u_dev) /* find processes of a given user.        */
/*----------------------------------------------------------------------------*/
/* this version assumes that ps -au returns one header-line and then	      */
/* lines with usernames, pids and some other stuff			      */
{
	char prname[LINELEN], iline[LINELEN];
	char *s;
	char *ps_name, *ps_pid, *ps_dev;

	struct stat status;

	FILE  *ps;
	int i;
	pid_t pid;
	uid_t uid;

	if (!(ps = popen(ps_cmd, "r")) ) {
		bailout("Can't use ps program", 6);
		return;
	}
	fgets(iline, LINELEN, ps);		 /* get header-line */

	ids_fill =  0;
	while (fgets(iline, LINELEN, ps)) {
		ps_name = strtok(iline, delims);   /* manual: Never use this function. */
		ps_pid = strtok(0, delims);
		pid = atoi(ps_pid);
// TODO: this is not flexible about the terminal position in the ps output
//		for(i = 0; i < 5; i++)
			ps_dev = strtok(0, delims);
		snprintf(prname, sizeof(prname), "/proc/%d", pid);   /* append /proc/ to proclist */
		if (stat(prname, &status))
			printf("Dead process:    \n");
		uid = status.st_uid;
		if (debug)
			printf("%-8s/%s -> %5s %5d %5d\n", ps_name, ps_dev, ps_pid, pid, uid);
		if ( strcmp(u_name, ps_name) == 0 && strcmp(u_dev, ps_dev) == 0 ) {
			if (debug)
				printf("     -> ok, Name = %s, dev = %s  => kill.\n", u_name, u_dev);
			ids_fill++;
			if (ids_fill > ids_max) {
				ids_max = 2 * (ids_max);
				ids_lst = (int *)realloc(ids_lst, sizeof(int) * (1 + (ids_max)));
			}
			ids_lst[ids_fill] = pid;
		}
	}
	fclose(ps);

	if (debug) {
		printf (" |-> ");
		for (i = 1; i <= ids_fill; i++)
			printf (" %5d", ids_lst[i]);
		printf ("\n");
	}
}

/*---------------------------------------------------------------------------*/
/*===    The following functions have a murder job.                       ===*/
/*---------------------------------------------------------------------------*/

static int
kill_PIDs(char *u_name, char *u_dev)
{
	int i, ok = 1;

	ids_max = 50;
    	ids_lst = (int *)calloc(sizeof(int), 1 + (int)(ids_max));

	if (debug)
		printf("\nget PIDs for %s\n", u_name);

	/*.. Tell processes to hang up. .............................................*/
//	ids_fill =  0;
	get_PIDs(u_name, u_dev);
	if (do_bite)
		for (i = 1; i <= ids_fill; i++)
			kill_HUP(ids_lst[i]);
	sleep(HUP_WAIT);

	/*.. Check for processes that still survived and now kill them. .............*/
	/*.. This will also kill a user that returns at once. Without warning. :) ...*/
	ids_fill =  0;
	get_PIDs(u_name, u_dev);
	if (do_bite)
		for (i = 1; i <= ids_fill; i++)
			kill_KILL(ids_lst[i]);
	sleep(KILLWAIT);

	/*.. Check for processes that survived killing. .............................*/
	if (do_bite)
		for (i = 1; i <= ids_fill; i++) {
	 		if (kill_Res(ids_lst[i]))
				ok = 0;
		}
	free(ids_lst);
	return ok;
}

/* terminate process using SIGHUP, first. */
static int
kill_HUP(int pid)
{
	kill(pid, SIGHUP); 	/* first send "hangup" signal     */
}

/* terminate process using SIGKILL, later.*/
static int
kill_KILL(int pid)
{
	kill(pid, SIGKILL);	/* then send sure "kill" signal.  */
}

/* Check for processes that refused to die.*/
static int
kill_Res(int pid)
{
	if (!kill(pid, 0))
		return 1;		/* failure--refuses to die!	   */
	else
		return 0;		/* successful kill with SIGKILL    */
}

/*---------------------------------------------------------------------------*/
/*===    The following functions spread the messages to everywhere.       ===*/
/*---------------------------------------------------------------------------*/

static int
mesg(int flag, char *name, char *dev, int stime, int idle, conf_el *ce)
{
	char mbuf[LINELEN];          /* message buffer */
	char *hint;
	time_t tvec;
	FILE *fp, *mprog;

	time(&tvec);				/* tvec = current time. .....*/
	if (flag == IDLEPUNISH && ce->warn) {	/* process warning message.  */
		hint = "** IDLE-PUNISH **";
		if (!(fp = fopen(dev, "w")) ) {
			bailout("Can't open user's terminal", 5);
			return 0;
		}

		if (ce->clear) {
			snprintf(mbuf, sizeof(mbuf), "clear >%s", dev);
			system (mbuf);
		}
		fprintf(fp, "\n\n\r"); /* \r is sometimes needed...*/
		fprintf(fp, ".--------------------------------------------.\n\r");
	        fprintf(fp, "|  %10s, you have been idle too long.  |\n\r", name);
		fprintf(fp, "|  Punishment: Log out for %3d seconds!      |\n\r", Idle_Ban);
		fprintf(fp, "`--------------------------------------------'\n\r");
		fprintf(fp, "\07\n");
		sleep(1);
		fprintf(fp, "\07\n");
		sleep(1);
		fprintf(fp, "\07\n\r");
		sleep(10);		      /* just some time to read this message */
		fclose(fp);
	}
	if (flag == ANGRY && ce->warn) {		/* process warning message.  */
		hint = " ** ANGRY **";
		if (!(fp = fopen(dev, "w")) ) {
			bailout("Can't open user's terminal", 5);
			return 0;
		}
		if (ce->clear) {
			snprintf(mbuf, sizeof(mbuf), "clear >%s", dev);
			system (mbuf);
		}
		if (ce->hard) {
			fprintf(fp, "\n\n\r"); /* \r is sometimes needed...*/
			fprintf(fp, ".--------------------------------------------------.\n\r");
			fprintf(fp, "|  %10s, you have just been kicked off.      |\n\r", name);
			fprintf(fp, "|  Make off, don't try it again, today.            |\n\r");
			fprintf(fp, "`--------------------------------------------------'\n\r");
			fprintf(fp, "\07\n");
			sleep(1);
			fprintf(fp, "\07\n");
			sleep(1);
			fprintf(fp, "\07\n\r");
			sleep(10);		      /* just some time to read this message */
		}
		fclose(fp);
	}
	if (flag == WARNING && ce->warn) {		/* process warning message.  */
		hint = "** NOTIFIED **";
		if (!(fp = fopen(dev, "w")) ) {
			bailout("Can't open user's terminal", 5);
			return 0;
		}
		if (ce->clear) {
			snprintf(mbuf, sizeof(mbuf), "clear >%s", dev);
			system(mbuf);
		}
		if (ce->hard) {
			fprintf(fp, "\n\n\r"); /* \r is sometimes needed...*/
			fprintf(fp, ".--------------------------------------------------.\n\r");
			fprintf(fp, "|  %10s: You've been on for %3d min.         |\n\r", name, stime);
			fprintf(fp, "|  You'll be logged off in %3d sec. so finish up.  |\n\r", ce->grace);
			fprintf(fp, "`--------------------------------------------------'\n\r");
			fprintf(fp, "\07\n");
			sleep(1);
			fprintf(fp, "\07\n");
			sleep(1);
			fprintf(fp, "\07\n\r");
		} else {
			fprintf(fp, "\n\n\r"); /* \r is sometimes needed...*/
			fprintf(fp, ".----------------------------------------------.\n\r");
			fprintf(fp, "| %10s: You've been idle for %3d min.    |\n\r", name, idle);
			fprintf(fp, "|        You'll be logged off in %3d sec       |\n\r", ce->grace);
			fprintf(fp, "|             unless you hit a key.            |\n\r");
			fprintf(fp, "`----------------------------------------------'\n\r");

			/*.. Try to wake up sleeping user by beeping 3 times. . .....................*/
			fprintf(fp, "\07\n");
			sleep(1);
			fprintf(fp, "\07\n");
			sleep(1);
			fprintf(fp, "\07\n\r");
		}
		fclose(fp);
		if (ce->mail) {
			snprintf(mbuf, sizeof(mbuf), "/usr/bin/mail -s \"++WARNING - LOG-OFF ++\" %s", name);
			/* open pipe to mail program for writing */
			if (!(mprog = popen(mbuf, "w")) ) {
				bailout("Can't use /usr/bin/mail program", 6);
				return 0;
			}
			if (ce->hard) {
				fprintf(mprog, "Your session-limit will be reached soon.\n");
				fprintf(mprog, "   %s: You've been on for %3d min.\n", name, stime);
				fprintf(mprog, "   You'll be logged off in %3d sec.\n", ce->grace);
				fprintf(mprog, "   So finish up, please.\n");
			} else {
				fprintf(mprog, "You have been idle for quite a long time.\n");
				fprintf(mprog, "   %s: You've been idle for %3d min.\n", name, stime);
				fprintf(mprog, "   You'll be logged off in %3d sec.\n", ce->grace);
				fprintf(mprog, "   Please: Type something into a terminal \n");
				fprintf(mprog, "           or you'll be kicked off.\n");
			}
			fclose(mprog);
		}
	}
	if (flag == LOGOFF) {
	        hint = "** LOGOFF **";
	        if (ce->mail) {
			snprintf(mbuf, sizeof(mbuf), "/usr/bin/mail -s \"Logged off, you were idle\" %s", name);
			/* open pipe to mail program for writing */
			if (!(mprog = popen(mbuf, "w")) ) {
				bailout("Can't use /usr/bin/mail program", 6);
				return 0;
			}
			fprintf(mprog, "Subject: Excess Idle Time\n\n");
			fprintf(mprog, "  Logged off - %s - %s\n", name, ctime(&tvec));
			fprintf(mprog, "    - excess idle time - \n");
			fprintf(mprog, "      on tty = %s\n",dev+5);
			fclose(mprog);
		}
	}
	if (flag == NOLOGOFF) {
		hint = "** LOGOFF FAILED **";
		if (ce->mail) {
			snprintf(mbuf, sizeof(mbuf), "/usr/bin/mail -s \"Couldn't log out [%s] \" root", name);
			if ((mprog = popen(mbuf, "w")) == (FILE *)NULL) {
				bailout("Can't use /usr/bin/mail program", 7);
				return 0;
			}
			fprintf(mprog, "Subject: Can't logoff %s\n", name);
			fprintf(mprog, "Can't Log off - %s %s\n", name, ctime(&tvec));
			fprintf(mprog, "tty = %s\n", dev + 5);
			fclose(mprog);
		}
	}
	if (ce->log) {				/* Generate logfile message */
		char msg[100];
		snprintf(msg, sizeof(msg), "%-20s %-8s %-5s idle:%3d sess:%3d",
			hint, name, dev + 5, idle, stime);
		log_msg(msg);
	}
	return 0;
}

/* This function adds something to a logfile. ......*/
static int
log_msg(char *message)
{
	FILE *log = 0;
	struct stat status;
	char str_time[30];

	if (stat(logfname, &status) >= 0) {			/* if logfile exists */
		log = fopen(logfname, "a");		 	/* open to append    */
		if (log == NULL)
			return 1;			/* fopen failed.     */

		snprintf(str_time, sizeof(str_time), "%s", ctime(&pres_time) + 3);
		str_time[strlen(str_time) - 1] = 0;

		fprintf(log, "%s - %s\n", str_time, message);
		fclose(log);
    		return 0;
	}
}

/* Handle error message.     */
static int
bailout(char *message, int status)
{
	char msg[100];                          /* Try to log the message.   */

	snprintf(msg, sizeof(msg), "** ERROR ** %s", message);
	log_msg(msg);
}

/*.. reduce time to sleep for the next time the daemon falls asleep. ........*/
static int
lower_sleep(int sleep_left)
{
	if (0 < sleep_left && sleep_left < sleeptime)
		sleeptime = sleep_left;
}

/*---------------------------------------------------------------------------*/
/*===    By the way have a look for forgotten user-processes              ===*/
/*---------------------------------------------------------------------------*/
static int
kill_lost_PIDs()
{
	char prname[LINELEN], iline[LINELEN];
	char *s;
	char *ps_name, *ps_pid;

	struct stat status;

	FILE *ps;
	int i;
	pid_t pid;
	uid_t uid;
	char mbuf[LINELEN];	   /* message buffer */

	int userpos = 0;	   /* position of user found, 0 => not found */

	/*.. have ps tell us all current users, uids and pids. ......................*/
	if (!(ps = popen(ps_cmd, "r")) ) {
		bailout("Can't use ps program", 6);
		return 0;
	}
	fgets(iline, LINELEN, ps);		 /* get header-line */

	ids_fill =  0;
	while (fgets(iline, LINELEN, ps)) {
		ps_name = strtok(iline, delims);   /* manual: Never use this function. */
		ps_pid = strtok(0, delims);
		pid = atoi(ps_pid);

		snprintf(prname, sizeof(prname), "/proc/%d", pid);   /* append /proc/ to proclist */
		if (stat(prname, &status)) {
			userpos = -1;			  /* => process will be killed. */
			if (do_bite)
				kill(pid, SIGKILL); /* send the "kill" signal */
			snprintf(mbuf, sizeof(mbuf), "Dead , killed: %-10s %5d : %5d", ps_name, uid, pid);
			log_msg(mbuf);
		} else {
			uid = status.st_uid;
			if (debug)
				printf("%-8s -> %5s %5d %5d\n", ps_name, ps_pid, pid, uid);

			if (1000 <= uid && uid != 65534) {	  /* neither system accounts nor nobody */
				/*.. Get Position of user in userlst. .......................................*/
				strncpy(userlst[0].Name, ps_name, UT_NAMESIZE);
				userlst[0].Name[UT_NAMESIZE] = '\0';
				userpos = userfill;
				while ( strncmp(userlst[userpos].Name, ps_name, UT_NAMESIZE) )
					userpos--;

				if (userpos == 0 ) {	     /* user not found => not active */
					if (do_bite)
						kill(pid, SIGKILL); /* send the "kill" signal */
					 snprintf(mbuf, sizeof(mbuf), "Lost, killed: %-10s %5d : %5d", ps_name, uid, pid);
					 log_msg(mbuf);
				} else if(strlen(userlst[userpos].Device) == 1) { /* "." */
					if (do_bite)
						kill(pid, SIGKILL); /* send the "kill" signal */
					snprintf(mbuf, sizeof(mbuf), "Left, killed: %-10s %5d : %5d", ps_name, uid, pid);
					log_msg(mbuf);
				}
			}
		}
		/* Read to the end of line to avoid parsing the rest of the command
		 * line in next round and getting a segfault as ps_pid will be null!!
		 */
		i = strlen(iline);
		while (iline[i-1] != '\n') {
			if (!fgets(iline, LINELEN, ps)) {
				/* end of file, exit*/
				fclose(ps);
				return 0;
			}
			i = strlen(iline);
		}
	}
	fclose(ps);
}
