#! /bin/bash

LOG_FILE=/tmp/ipApply.log
WORK_DIR=/usr/bin

log() {
    # local date=`date`
    echo "$1" >> $LOG_FILE
    printf "$1\n"
}

log "ipApply start"
nohup $WORK_DIR/ipApply >> $LOG_FILE 2>&1 &
log "ipApply end"

exit $?
