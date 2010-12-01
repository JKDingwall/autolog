From: Luis Uribe <acme@eviled.org>
Subject: Patch for autolog.c
Index: autolog/autolog.c
===================================================================
--- autolog.orig/autolog.c	2010-03-25 20:01:19.000000000 -0500
+++ autolog/autolog.c	2010-04-02 10:04:43.000000000 -0500
@@ -114,7 +114,7 @@
 int listall	= 0;
 char *confname	= "/etc/autolog.conf";
 char *logfname	= "/var/log/autolog.log";
-char *datfname	= "/tmp/autolog.data";
+char *datfname	= "/var/lib/autolog/autolog.data";
 int g_idle	= D_IDLE;
 int g_grace	= D_GRACE;
 int g_hard	= D_HARD;
@@ -202,9 +202,10 @@
     load_users();
     if (debug){
         printf("\n");
-	printf("Aus der Datei eingelesen:\n");
-	printf("=========================\n");
+	printf("From the datafile:\n");
+	printf("==================\n");
 	show_results();
+	printf("Done reading datafile\n");
     }
 
 /* now sit in an infinite loop and work */
@@ -258,6 +259,7 @@
 check_utmp(){		/* select utmp entries needing killing */
     char dev[STRLEN], name[STRLEN], prname[STRLEN];
     char *msg;
+    char tmp_str[80];
 
     struct stat status;
     time_t idle, atime;
@@ -277,8 +279,9 @@
 		    msg,utmpp->ut_user,utmpp->ut_pid,ctime(&utmpp->ut_time));
 	return(0);
     }
-    if (stat(dev, &status)){		     /* if can't get status for port */
-	bailout("Can't get status of user's terminal", 1);
+    if (dev[5] != ':' && stat(dev, &status)){		     /* if can't get status for port */
+	snprintf(tmp_str, 79, "Can't get status of user's terminal: %s", dev);
+	bailout(tmp_str, 1);
 	return(0);
     }
 
@@ -302,7 +305,7 @@
 
 /*.. Get Position of user in userlst. .......................................*/
     userpos=userfill;
-    while ( strcmp(userlst[userpos].Name,name) ) userpos--;
+    while ( strcmp(userlst[userpos].Name,name) ) userpos--; 
 	
 /*.. if not found -> add user to userlst. ...................................*/
     if ( userpos ==0 ) {
@@ -436,6 +439,11 @@
 		ps_cmd = strcpy((char *)malloc(strlen(s)+1),s+3);
 		continue;
             }
+	    s=strstr(iline,"nolostkill");
+	    if (s) {
+		lostkill = 0;
+		continue;
+	    }
             set_defs(c_idx);
             s=strtok(iline,delims);
             do{
@@ -467,8 +475,6 @@
 			c_arr[c_idx].warn=lev;
 		    else if (!strcmp(s,"log"))
 			c_arr[c_idx].log=lev;
-		    else if (!strcmp(s,"lostkill"))
-			lostkill=lev;
 		    else if (debug)
 			printf("Unknown token in file: %s: %s\n",confname,s);
 		}
@@ -477,8 +483,8 @@
 	    idle=c_arr[c_idx].idle;
 	
 /*.. Maybe it is necessary to reduce the max. sleeptime to shortest session. */
-	    if (0< c_arr[c_idx].hard && 0<idle)	 /* but not to zero seconds. */
-		if (2*60*idle < sleep_max)  sleep_max=2*60*idle;
+	    if (0<idle)				 /* but not to zero seconds. */
+		if (60*idle < sleep_max)  sleep_max=60*idle;
 	    c_idx++;
 	}
         fclose(f);
@@ -505,7 +511,7 @@
 /* I've apparently found some bug in certain LINUX versions of the */
 /* regular expressions library routines.  One of these following two */
 /* versions of the routine should work for you. */
-#define AVOID_REGEX_BUG
+/* #define AVOID_REGEX_BUG*/
 #ifdef AVOID_REGEX_BUG				/* some strange bug in re_exec */
 pat_match(char *patt, char *strg)
     {
@@ -543,7 +549,7 @@
 
 check_idle(userdata* akt_usr)
     {
-    char ddev[STRLEN],*gn = "";
+    char ddev[STRLEN],*gn = ".*";
     char dev[STRLEN], name[STRLEN], prname[STRLEN];
     int  idle;
     struct stat status;
@@ -563,6 +569,9 @@
     if (debug)
 	printf("\nuser: %-10s %-10s %-d\n",name,dev,idle);
 
+    if(dev[0] == ':')   /* ignore xdm */
+	    return(0);
+
     sprintf(ddev,"/dev/%s",dev);	  /* append /dev/ to base name    */
 
 /*.. If user has logged out, check his Session time and so. .................*/
@@ -677,7 +686,7 @@
 	if (debug)
 	  printf("Killing user, now.\n");
 	
-	if (kill_PIDs(name)==1) 	/* try to kill users' processes      */
+	if (kill_PIDs(name,dev)==1) 	/* try to kill users' processes      */
 	     mesg(LOGOFF,  name, ddev, stime/60, idle, ce); /* mail to user  */
 	else mesg(NOLOGOFF,name, ddev, stime/60, idle, ce); /* couldn't kill */
 	if (ce->hard)
@@ -699,7 +708,7 @@
 	mesg(ANGRY, name, ddev, stime/60, idle, ce);    /* angry about user. */
 	if (debug)
 	  printf("I am angry, user returned...\n");
-	if (kill_PIDs(name)==1) 		/* try to kill user, at once */
+	if (kill_PIDs(name, dev)==1) 		/* try to kill user, at once */
 	     mesg(LOGOFF,  name, ddev, stime/60, idle, ce); /* mail to user  */
 	else mesg(NOLOGOFF,name, ddev, stime/60, idle, ce); /* couldn't kill */
     }
@@ -722,13 +731,13 @@
 /*----------------------------------------------------------------------------*/
      void get_PIDs                  /* find processes of a given user.        */
 /*----------------------------------------------------------------------------*/
-     (char *u_name)                 /* name of the user.                      */
+     (char *u_name, char *u_dev)                 /* name of the user.                      */
 /*----------------------------------------------------------------------------*/
 /* this version assumes that ps -au returns one header-line and then	      */
 /* lines with usernames, pids and some other stuff			      */
 {   char   prname[LINELEN], iline[LINELEN];	
     char   *s;
-    char   *ps_name, *ps_pid;
+    char   *ps_name, *ps_pid, *ps_dev;
 
     struct stat status;
 
@@ -746,15 +755,16 @@
 	ps_name= strtok(iline,delims);   /* manual: Never use this function. */
 	ps_pid = strtok(0,delims);
 	pid    = atoi(ps_pid);
+	for(i=0;i<5;i++) ps_dev = strtok(0,delims);
 	sprintf(prname,"/proc/%d",pid);   /* append /proc/ to proclist */
 	if (stat(prname, &status))
 	   printf("Dead process:    \n");
 	uid = status.st_uid;
 	if (debug)
-	    printf("%-8s -> %5s %5d %5d\n", ps_name, ps_pid, pid,uid);
-	if ( strcmp(u_name,ps_name)==0 ){
+	    printf("%-8s/%s -> %5s %5d %5d\n", ps_name, ps_dev,ps_pid, pid,uid);
+	if ( strcmp(u_name,ps_name)==0 && strcmp(u_dev,ps_dev)==0 ){
 	    if (debug)
-		printf("     -> ok, Name = %s  => kill.\n",u_name);
+		printf("     -> ok, Name = %s, dev = %s  => kill.\n",u_name,u_dev);
 	    ids_fill++;
 	    if (ids_fill > ids_max) {
 		ids_max = 2*(ids_max);
@@ -778,7 +788,7 @@
 /*===    The following functions have a murder job.                       ===*/
 /*---------------------------------------------------------------------------*/
 
-int kill_PIDs(char *u_name){
+int kill_PIDs(char *u_name, char *u_dev){
     int  i,ok=1;
 
     ids_max  = 50;
@@ -789,7 +799,7 @@
 
 /*.. Tell processes to hang up. .............................................*/
 //    ids_fill =  0;
-    get_PIDs(u_name);
+    get_PIDs(u_name, u_dev);
     if (do_bite)
 	for (i=1; i<=ids_fill; i++) kill_HUP(ids_lst[i]);
     sleep(HUP_WAIT);
@@ -797,7 +807,7 @@
 /*.. Check for processes that still survived and now kill them. .............*/
 /*.. This will also kill a user that returns at once. Without warning. :) ...*/
     ids_fill =  0;
-    get_PIDs(u_name);
+    get_PIDs(u_name, u_dev);
     if (do_bite)
 	for (i=1; i<=ids_fill; i++) kill_KILL(ids_lst[i]);
     sleep(KILLWAIT);
@@ -921,10 +931,10 @@
       }
       fclose(fp);
       if (ce->mail){
-	sprintf(mbuf, "/bin/mail -s \"++WARNING - LOG-OFF ++\" %s", name);
+	sprintf(mbuf, "/usr/bin/mail -s \"++WARNING - LOG-OFF ++\" %s", name);
 	/* open pipe to mail program for writing */
 	if (!(mprog = popen(mbuf, "w")) ){
-	    bailout("Can't use /bin/mail program", 6);
+	    bailout("Can't use /usr/bin/mail program", 6);
 	    return(0);
 	}
 	if (ce->hard){
@@ -934,8 +944,8 @@
 	  fprintf(mprog, "   So finish up, please.\n");
 	}
 	else{
-	  fprintf(mprog, "Your have been idle for quite a long time.\n");
-	  fprintf(mprog, "   %s: You've idle for %3d min.\n", name, stime);
+	  fprintf(mprog, "You have been idle for quite a long time.\n");
+	  fprintf(mprog, "   %s: You've been idle for %3d min.\n", name, stime);
 	  fprintf(mprog, "   You'll be logged off in %3d sec.\n",ce->grace);
 	  fprintf(mprog, "   Please: Type something into a terminal \n");
 	  fprintf(mprog, "           or you'll be kicked off.\n");
@@ -946,10 +956,10 @@
     if (flag == LOGOFF){
         hint="** LOGOFF **";
         if (ce->mail){
-	    sprintf(mbuf, "/bin/mail -s \"Logged off, you were idle\" %s", name);
+	    sprintf(mbuf, "/usr/bin/mail -s \"Logged off, you were idle\" %s", name);
             /* open pipe to mail program for writing */
             if (!(mprog = popen(mbuf, "w")) ){
-                bailout("Can't use /bin/mail program", 6);
+                bailout("Can't use /usr/bin/mail program", 6);
 		return(0);
 	    }
 	    fprintf(mprog, "Subject: Excess Idle Time\n\n");
@@ -962,9 +972,9 @@
     if (flag == NOLOGOFF){
         hint="** LOGOFF FAILED **";
         if (ce->mail){
-	    sprintf(mbuf, "/bin/mail -s \"Couldn't log out [%s] \" root",name);
+	    sprintf(mbuf, "/usr/bin/mail -s \"Couldn't log out [%s] \" root",name);
             if ((mprog = popen(mbuf, "w")) == (FILE *) NULL){
-                bailout("Can't use /bin/mail program", 7);
+                bailout("Can't use /usr/bin/mail program", 7);
 		return(0);
 	    }
             fprintf(mprog, "Subject: Can't logoff %s\n", name);
@@ -1001,8 +1011,7 @@
 
 bailout(char *message, int status){		/* Handle error message.     */
 	char msg[100];                          /* Try to log the message.   */
-	sprintf(msg, "%-20s %s %-5s idle:%3d sess:%3d",
-		     "** ERROR **" , message);
+	sprintf(msg, "** ERROR ** %s", message);
 	log_msg(msg);
 }
 
