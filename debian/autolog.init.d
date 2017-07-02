#!/bin/sh

### BEGIN INIT INFO
# Provides:          autolog
# Required-Start:    $remote_fs
# Required-Stop:     $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start the autolog daemon firewall at boot time
# Description:       Script to start/stop/reload the autolog daemon
### END INIT INFO


PATH=/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/autolog
NAME=autolog
DESC="terminates connections for idle users"

test -x $DAEMON || { echo "$DAEMON not installed"; 
	if [ "$1" = "stop" ]; then exit 0;
	else exit 5; fi; }

. /lib/lsb/init-functions

case "$1" in


start)
    
	start-stop-daemon --start --oknodo --exec $DAEMON
	STATUS=$?
	if [ "$STATUS" = 0 ];
	then
            log_success_msg "Starting autolog"
        else
  	    log_failure_msg "Starting autolog"
        fi
	exit $STATUS
	;;
	
	
stop)
    
	start-stop-daemon --stop --oknodo --retry=TERM/30/KILL/5  --name $NAME
	STATUS=$?
	if [ "$STATUS" = 0 ];
        then
	    log_success_msg "Shutting down autolog"
	    exit 0
        else
	    log_failure_msg "Shutting down autolog"
        fi
	exit $STATUS
	;;
	
	
restart | force-reload)

	$0 stop
	$0 start
	;;
	
	
status)
	
	checkproc "$DAEMON" > /dev/null
	STATUS=$?
	if [ "$SIGNAL" = 0 ]
	then
	    log_success_msg "Checking autolog"
	else
	    log_warning_msg "Checking autolog: Not running"
	fi
	exit $STATUS
	;;
	
	
    *)
	echo "Usage: $0 {start|stop|status|restart|force-reload}"
	exit 1
	;;
	
esac
