#! /bin/bash

LOG_FILE=/tmp/ipReply.log
WORK_DIR=/usr/bin

log() {
    # local date=`date`
    echo "$1" >> $LOG_FILE
    printf "$1\n"
}

log "ipReply start"
nohup $WORK_DIR/ipReply >> $LOG_FILE 2>&1 &
log "ipReply end"

exit $?
