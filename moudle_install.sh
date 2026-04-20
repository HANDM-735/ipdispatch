#!/bin/bash
# set -x #调试使用
# 此安装脚本只支持CP/FT产品,RDBI产品安装由系统线负责
# 日志文件配置
LOG_FILE="${LOG_FILE:-/var/log/ipapply-install.log}"
# 默认10MB
LOG_MAX_SIZE="${LOG_MAX_SIZE:-10485760}"
LOG_BACKUP_COUNT="${LOG_BACKUP_COUNT:-5}"

# 日志级别定义
LOG_LEVEL_DEBUG=0
LOG_LEVEL_INFO=1
LOG_LEVEL_WARN=2
LOG_LEVEL_ERROR=3

# 当前日志级别 (默认为DEBUG)
LOG_LEVEL=$LOG_LEVEL_DEBUG

# 脚本业务配置
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SERVICE_INSTALL_DIR=/etc/systemd/system
SERVICE_NAME=ipApply.service
INSTALL_DIR=/usr/bin
PROG_NAME=ipApply
INSTALL_LIB_DIR=/usr/lib
LIB_NAME=libipapply.so
CONF_DIR=/userdata/config/ipApply
CONF1=ipconfigfig_mode
CONF2=ip_config.ini
CONF3=libip_config.ini
CONF4=libipconfig_mode

#备份目录
BACKUP_DIR="bak_date '+%Y%m%d_%H%M%S'"

# 日志轮转函数
rotate_log()
{
    local log_file="${1:-$LOG_FILE}"
    if [[ ! -f "$log_file" ]]; then
        return 0;
    fi
    # 获取文件大小
    local file_size=$(stat -c%s "$log_file" 2>/dev/null || echo "0")
    # 如果文件大小超过限制,则进行轮转
    if [[ $file_size -gt $LOG_MAX_SIZE ]]; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Starting log rotation for $log_file (size: $file_size)" >> "$log_file"
        # 删除最旧的备份文件
        local oldest_backup="${log_file}.${LOG_BACKUP_COUNT}"
        if [[ -f "$oldest_backup" ]]; then
            rm -f "$oldest_backup"
        fi
        # 轮转现有备份文件
        for ((i=LOG_BACKUP_COUNT-1; i>=1; i--)); do
            local current_backup="${log_file}.$(i)"
            local next_backup="${log_file}.$((i+1))"
            if [[ -f "$current_backup" ]]; then
                mv "$current_backup" "$next_backup"
            fi
        done
        # 创建新的备份
        mv "$log_file" "${log_file}.1"
        # 创建新的日志文件并添加轮转记录
        touch "$log_file"
        chmod 644 "$log_file"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Log rotation completed. New log file created." >> "$log_file"
        return 0
    fi
}

# 主日志记录函数
log_message()
{
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local level_str=""
    local color=$NC

    case $level in
        $LOG_LEVEL_DEBUG)
            level_str="DEBUG"
            color=$BLUE
            ;;
        $LOG_LEVEL_INFO)
            level_str="INFO"
            color=$GREEN
            ;;
        $LOG_LEVEL_WARN)
            level_str="WARN"
            color=$YELLOW
            ;;
        $LOG_LEVEL_ERROR)
            level_str="ERROR"
            color=$RED
            ;;
        *)
            level_str="UNKNOWN"
            ;;
    esac

    # 检查日志级别是否足够
    [[ $level -lt $LOG_LEVEL ]] && return 0
    # 日志轮转检查
    rotate_log "$LOG_FILE"
    # 构建日志条目
    local log_entry="[$timestamp] [$level_str] - $message"
    # 输出到控制台 (带颜色)
    if [[ -t 1 ]]; then
        echo -e "${color}${log_entry}${NC}"
    else
        echo "$log_entry"
    fi
    # 输出到文件
    echo "$log_entry" >> "$LOG_FILE"
    return 0
}

# 便捷函数
log_debug() { log_message $LOG_LEVEL_DEBUG "$1"; }
log_info()  { log_message $LOG_LEVEL_INFO "$1"; }
log_warn()  { log_message $LOG_LEVEL_WARN "$1"; }
log_error() { log_message $LOG_LEVEL_ERROR "$1"; }

stop_autostart_serve()
{
    log_debug "Enter into stop_autostart_serve()"
    if [ -f "${SERVICE_INSTALL_DIR}/${SERVICE_NAME}" ]; then
        systemctl disable ${SERVICE_NAME}
        log_info "stop_autostart_serve() disable service"
    fi
    log_debug "Exited stop_autostart_serve() ret=$?"
}

start_autostart_service()
{
    log_debug "Enter into start_autostart_service()"
    if [ -f "${SERVICE_INSTALL_DIR}/${SERVICE_NAME}" ]; then
        systemctl enable ${SERVICE_NAME}
        log_info "start_autostart_service() disable service"
    fi
    log_debug "Exited start_autostart_service() ret=$?"
}

start_service()
{
    log_debug "Enter into start_service()"
    stop_autostart_serve
    start_autostart_service
    log_debug "Exited start_service() ret=$?"
}

make_install_dir()
{
    log_debug "Enter into make_install_dir()"
    systemctl stop ${SERVICE_NAME}
    if [ ! -d ${INSTALL_DIR} ]; then
        mkdir -p ${INSTALL_DIR}
        log_info "make_install_dir() create install directory ${INSTALL_DIR}"
    fi
    if [ ! -d ${INSTALL_LIB_DIR} ]; then
        mkdir -p ${INSTALL_LIB_DIR}
        log_info "make_install_dir() create library directory ${INSTALL_LIB_DIR}"
    fi
    if [ ! -d ${CONF_DIR} ]; then
        mkdir -p ${CONF_DIR}
        log_info "make_install_dir() create config directory ${CONF_DIR}"
    fi
    log_debug "Exited make_install_dir() ret=$?"
}

copy_install_file()
{
    log_debug "Enter into copy_install_file()"
    if [ -d "${SCRIPT_DIR}/bin" ]; then
        cp -r ${SCRIPT_DIR}/bin/${PROG_NAME} ${INSTALL_DIR}/
        cp -r ${SCRIPT_DIR}/bin/${SERVICE_NAME} ${SERVICE_INSTALL_DIR}/
        systemctl daemon-reload
        log_info "copy_install_file() copy bin directory"
    fi
    if [ -d "${SCRIPT_DIR}/lib" ]; then
        cp ${SCRIPT_DIR}/lib/${LIB_NAME} ${INSTALL_LIB_DIR}/
        log_info "copy_install_file() copy lib direcotry"
    fi
    if [ -d ${CONF_DIR} ]; then
        cp -r ${SCRIPT_DIR}/bin/${CONFIG1} ${CONF_DIR}/
        cp -r ${SCRIPT_DIR}/bin/${CONFIG2} ${CONF_DIR}/
        cp -r ${SCRIPT_DIR}/bin/${CONFIG3} ${CONF_DIR}/
        cp -r ${SCRIPT_DIR}/bin/${CONFIG4} ${CONF_DIR}/
        log_info "copy_install_file() copy config directory ${CONF_DIR}"
    fi
    log_debug "Exited copy_install_file() ret=$?"
}

chmod_file()
{
    log_debug "Enter into chmod_file()"
    if [ -f "${INSTALL_DIR}/${PROG_NAME}" ]; then
        chmod 755 ${INSTALL_DIR}/${PROG_NAME}
        log_info "chmod_file() chmod ${INSTALL_DIR}/${PROG_NAME}"
    fi
    log_debug "Exited chmod_file() ret=$?"
}

auto_install()
{
    log_debug "Enter into auto_install()"
    make_install_dir
    copy_install_file
    chmod_file
    start_service
    log_debug "Exited chmod_file() ret=$?"
}

log_debug "Enter into auto install begin....."
auto_install
log_debug "Exited auto install end....."
