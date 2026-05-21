#include "netconfig.h"
#include "xconfig.hpp"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <utility>
#include <sstream>
#include <fstream>
#include <condition_variable>

#ifndef _WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifndef _WINDOWS //WINDOWS平台
#include <libdriver/libMciDrv.h>
#endif

#include "message.h"
#include "log_format.hpp"

#define ssleep(a)      sleep(a)
#define ussleep(a)     usleep(a)

#if defined(DEV_TYP_CP_SYNC)
#include <libdriver/lib_drv.h>
#elif defined(DEV_TYP_CP_RK3588)
#include <librca_get_slots/lib_rca_get_slots.h>
#endif

std::mutex mtx;
std::condition_variable cv;
std::atomic<bool> stopFlag(false);

static uint16_t slot_type = 0;
static uint16_t slot_id = 0;

#if defined(DEV_TYP_RDBI)
std::string IP_CONFIG_FILE = "/userdata/config/ip_config.ini";
#else
std::string IP_CONFIG_FILE = "/userdata/config/ipApply/ip_config.ini";
#endif
std::string COMMON_CONFIG_FILE = "/userdata/config/config_common.ini";

// std::string MODE_CONFIG_FILE = "./ipconfig_mode";
// std::string IP_CONFIG_FILE = "./ip_config.ini";
// std::string COMMON_CONFIG_FILE = "./config_common.ini";
#define LOG_FILE    "/userdata/log/ipApply"

const char* APP_VERSION = "version_module:V1.0.8";
const char* BUILD_TIME = "build_time:" __DATE__ " " __TIME__;

//split string
static std::vector<std::string> split_single_char_delim(const std::string& input, char delim)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string create_dispatch_ip(const std::string& ip1, const std::string& ip2)
{
    std::vector<std::string> ip1_parts = split_single_char_delim(ip1, '.');
    std::vector<std::string> ip2_parts = split_single_char_delim(ip2, '.');
    if (ip1_parts.size() != 4 || ip2_parts.size() != 4) {
        LOG_MSG(ERROR, "[%s:%d]Invalid IP address format", __FUNCTION__, __LINE__);
        throw std::invalid_argument("Invalid IP address format");
    }
    if (ip1_parts[3] == ip2_parts[3]) {
        int last_part = std::stoi(ip2_parts[3]);
        if (last_part - 100 < 0 || last_part - 100 > 255) {
            LOG_MSG(ERROR, "[%s:%d]Resulting IP segment would be out of range (0-255)", __FUNCTION__, __LINE__);
            throw std::invalid_argument("Resulting IP segment would be out of range (0-255)");
        }
        ip2_parts[3] = std::to_string(last_part - 100);
    }
    std::string new_ip = ip1_parts[0] + "." + ip1_parts[1] + "." + ip1_parts[2] + "." + ip2_parts[3];
    return new_ip;
}

std::string modify_ip_as_gw(const std::string& newip, const std::string& pkgip, char delim)
{
    std::vector<std::string> newip_parts = split_single_char_delim(newip, delim);
    if (newip_parts.size() != 4) {
        LOG_MSG(ERROR, "[%s:%d]Invalid IP address format", __FUNCTION__, __LINE__);
        throw std::invalid_argument("Invalid IP address format");
    }

    std::vector<std::string> pkgip_parts = split_single_char_delim(pkgip, delim);
    if (pkgip_parts.size() != 4) {
        LOG_MSG(ERROR, "[%s:%d]Invalid IP address format", __FUNCTION__, __LINE__);
        throw std::invalid_argument("Invalid IP address format");
    }

    // pkgip_parts[3] = std::to_string(254);
    std::string default_gw = newip_parts[0] + "." + newip_parts[1] + "." + newip_parts[2] + "." + pkgip_parts[0];
    LOG_MSG(INFO, "[%s:%d]modify_ip_as_gw default_gw: %s", __FUNCTION__, __LINE__, default_gw.c_str());

    return default_gw;
}

std::pair<std::string, std::string> getNonLoopbackIPv4()
{
    std::pair<std::string, std::string> result;
#ifndef _WINDOWS
    int family, s;
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        LOG_MSG(ERROR, "[%s:%d]getifaddrs failed!", __FUNCTION__, __LINE__);
        perror("getifaddrs");
        return result; // Return an empty pair on error
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            s = getnameinfo(ifa->ifa_addr,
                            sizeof(struct sockaddr_in),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                LOG_MSG(ERROR, "[%s:%d]getnameinfo() failed: %s", __FUNCTION__, __LINE__, gai_strerror(s));
                continue;
            }
            // Set the result with the interface name and IP address
            result.first = ifa->ifa_name;
            result.second = host;
            break; // Stop after finding the first non-loopback IPv4 address
        }
    }

    freeifaddrs(ifaddr);
#endif
    return result;
}

std::string getFirstNonLoopbackIPv4()
{
    std::string ip;
#ifndef _WINDOWS
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        LOG_MSG(ERROR, "[%s:%d]getifaddrs failed!", __FUNCTION__, __LINE__);
        perror("getifaddrs");
        return ip;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            s = getnameinfo(ifa->ifa_addr,
                            sizeof(struct sockaddr_in),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                LOG_MSG(ERROR, "[%s:%d]getnameinfo() failed: %s", __FUNCTION__, __LINE__, gai_strerror(s));
                continue;
            }
            ip = host;
            break;
        }
    }

    freeifaddrs(ifaddr);
#endif
    return ip;
}

std::vector<std::string> getFirstNonLoopbackIPv4Info()
{
    std::vector<std::string> result;
#ifndef _WINDOWS
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_MSG(ERROR, "[%s:%d]getifaddrs failed!", __FUNCTION__, __LINE__);
        perror("getifaddrs");
        return result; // Return an empty pair on error
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        std::vector<std::string> tmp;
        if (ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            // Set the result with the interface name and IP address
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;
            result.push_back(ifa->ifa_name);
            result.push_back(inet_ntoa(addr->sin_addr));
            result.push_back(inet_ntoa(netmask->sin_addr));
            break;
        }
    }

    freeifaddrs(ifaddr);
#endif
    return result;
}

bool isSameIp(const std::string& ip1, const std::string& ip2)
{
    std::vector<std::string> parts1 = split_single_char_delim(ip1, '.');
    std::vector<std::string> parts2 = split_single_char_delim(ip2, '.');
    if (parts1.size() != 4 || parts2.size() != 4)
        return false;

    return parts1[0] == parts2[0] && parts1[1] == parts2[1] && parts1[2] == parts2[2] && parts1[3] == parts2[3];
}

std::string unpack_ip_from_uint32(uint32_t ip)
{
    std::vector<std::string> parts(4);
    for (int i = 0; i < 4; ++i) {
        uint8_t segment = (ip >> (i * 8)) & 0xFF;
        parts[3 - i] = std::to_string(segment);
    }
    if (parts.size() != 4)
    {
        LOG_MSG(ERROR, "[%s:%d]Invalid number of IP parts!", __FUNCTION__, __LINE__);
        throw std::invalid_argument("Invalid number of IP parts");
    }
    std::string dispatch_ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
    return dispatch_ip;
}

std::string get_gw_from_local()
{
    std::array<char, 128> buffer;
    std::string result;
    std::string line;
    std::string default_gw;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("ip route", "r"), pclose);
    if (!pipe) {
        LOG_MSG(ERROR, "[%s:%d]popen() failed!", __FUNCTION__, __LINE__);
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    std::istringstream iss(result);
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        LOG_MSG(INFO, "[%s:%d]dbg line: %s", __FUNCTION__, __LINE__, line.c_str());
        // Check if the line contains "default" and "via"
        if (line.find("default") != std::string::npos && line.find("via") != std::string::npos) {
            // Find the position of "via"
            size_t viaPos = line.find("via");
            // Skip "via " and find the space that separates the IP address from the next part
            size_t ipEndPos = line.find(' ', viaPos + 4);
            if (ipEndPos != std::string::npos) {
                // Extract the IP address
                default_gw = line.substr(viaPos + 4, ipEndPos - (viaPos + 4));
                break; // Assuming there's only one default gateway, break the loop
            }
        }
    }

    std::vector<std::string> ipparts = split_single_char_delim(default_gw, '.');
    if (ipparts[3] == "0" || ipparts[3] == "255") {
        LOG_MSG(ERROR, "[%s:%d]default gateway ip can't be: %s", __FUNCTION__, __LINE__, default_gw.c_str());
    }
    return default_gw;
}

#ifndef _WINDOWS
// receive thread function
void receive_thread()
{
    char buffer[BUFFER_SIZE] = {0};
    struct sockaddr_in serveraddr, clientaddr;
    struct timeval timeout;
    socklen_t len = sizeof(serveraddr);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    // creat udp socket
    SocketRAII sockfd_rec(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_rec < 0) {
        LOG_MSG(ERROR, "[%s:%d]socket creation failed!", __FUNCTION__, __LINE__);
        perror("socket creation failed");
        return;
    }
    int optval = 1;
    if (setsockopt(sockfd_rec, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]setsockopt SO_BROADCAST failed!", __FUNCTION__, __LINE__);
        perror("setsockopt SO_BROADCAST failed");
        return;
    }
    if (setsockopt(sockfd_rec, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]setsockopt SO_REUSEADDR failed!", __FUNCTION__, __LINE__);
        perror("setsockopt SO_REUSEADDR failed");
        return;
    }
    if (setsockopt(sockfd_rec, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]setsockopt SO_RCVTIMEO failed!", __FUNCTION__, __LINE__);
        perror("setsockopt SO_RCVTIMEO failed");
        return;
    }
    memset(&clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = INADDR_ANY;
    clientaddr.sin_port = htons(CLIENT_PORT);
    if (bind(sockfd_rec, (const struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]bind failed!", __FUNCTION__, __LINE__);
        return;
    }
    LOG_MSG(INFO, "[%s:%d]Client is listening on port %d", __FUNCTION__, __LINE__, CLIENT_PORT);
    fflush(stdout);
    // int cycle = 0; // debug for same PGB ip
    while (!stopFlag.load()) {
        ssize_t n = recvfrom(sockfd_rec, buffer, BUFFER_SIZE, 0,
                            reinterpret_cast<struct sockaddr*>(&serveraddr), &len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                LOG_MSG(ERROR, "[%s:%d]recvfrom failed", __FUNCTION__, __LINE__);
                perror("recvfrom failed");
                continue;
            }
        }
        LOG_MSG(INFO, "[%s:%d]serveraddr from: %s", __FUNCTION__, __LINE__, inet_ntoa(serveraddr.sin_addr));
        std::string pkgfromip = inet_ntoa(serveraddr.sin_addr);
        // parse message
        IPAssignmentMessage reply_msg = IPAssignmentMessage::deserialize(reinterpret_cast<const uint8_t*>(buffer));
        std::string new_ip = unpack_ip_from_uint32(reply_msg.ip_address);
        std::string new_netmask = unpack_ip_from_uint32(reply_msg.netmask);
        std::string new_gateway = unpack_ip_from_uint32(reply_msg.gw_address);
        LOG_MSG(INFO, "[%s:%d]buffer header:%x, cmd:%d, slot_type: %d, slot_id: %d, ip:%s netmask:%s gateway:%s",
                __FUNCTION__, __LINE__, reply_msg.header, reply_msg.cmd, reply_msg.slot_type, reply_msg.slot_id,
                new_ip.c_str(), new_netmask.c_str(), new_gateway.c_str());
        fflush(stdout);
        // check message header
        if (reply_msg.header == MESSAGE_HEADER_CLIENT && reply_msg.cmd == DISPATCH_IP_BY_SERVER && reply_msg.slot_id == slot_id && reply_msg.slot_type == slot_type) {
            try {
#if 1
                // 实现说明: 直接从ipreply接口获取ip地址、掩码以及网关 */
                auto eth_result = getFirstNonLoopbackIPv4Info();
                std::string eth_name = eth_result[0];
                std::string client_ip = eth_result[1];
                std::string netmask = eth_result[2];
                std::string local_default_gw = get_gw_from_local();

                // todo:
                LOG_MSG(INFO, "[%s:%d]original slottype: %d slotid: %d", __FUNCTION__, __LINE__, slot_type, slot_id);
                LOG_MSG(INFO, "[%s:%d]local eth name: %s client ip: %s gateway: %s", __FUNCTION__, __LINE__, eth_name.c_str(),
                        client_ip.c_str(), netmask.c_str(), local_default_gw.c_str());
                LOG_MSG(INFO, "[%s:%d]recvfrom message: slottype: %d slotid: %d cmd: %d ip: %s netmask: %s gateway: %s", __FUNCTION__, __LINE__,
                        reply_msg.slot_type, reply_msg.slot_id, reply_msg.cmd, new_ip.c_str(), new_netmask.c_str(), new_gateway.c_str());

                // 用于判断 消息来源于具体哪个上位机
                // update ini
                xini_config xini_ip_config;
                xini_ip_config.set_file(IP_CONFIG_FILE);
                xini_ip_config.set_data("interface.ntpserveraddr", pkgfrom_ip);

                xini_config xini_cfg;
                xini_cfg.set_file(COMMON_CONFIG_FILE);
                xini_cfg.set_data("CONFIG_COMMON.platform_addr", pkgfrom_ip);
                xini_cfg.set_data("CONFIG_COMMON.board_slot", slot_id);

                if (isSameIp(new_ip, client_ip) && isSameIp(netmask, new_netmask) && isSameIp(local_default_gw, new_gateway)) {
                    LOG_MSG(INFO, "[%s:%d]Received IP Same as local IP", __FUNCTION__, __LINE__);
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        stopFlag.store(true);
                        cv.notify_one(); // notify main thread
                    }
                    break; // only hander once reply
                }
                configureNetwork(eth_name, new_ip, new_netmask, new_gateway);
                fflush(stdout);
#endif
#if 0
                std::string new_ip = create_dispatch_ip(pkgfrom_ip, pkg_ip);
                printf("Received IP:%s new_ip:%s\n", pkg_ip.c_str(), new_ip.c_str());
#endif
#if 0
                /* 实现说明: 从服务器校准系统时间信息配置 */
                std::string command = "ntpdate " + pkgfrom_ip;
                int result = system(command.c_str());
                if (result != 0) {
                    printf("Failed to set ntpdate\n");
                } else {
                    printf("ntpdate succeed\n");
                }
#endif
                // update ini
#if 0
                // update ntpserveraddr
                xini_config xini_ip_config;
                xini_ip_config.set_file(IP_CONFIG_FILE);
                xini_ip_config.set_data("interface.ntpserveraddr", pkgfrom_ip);

                xini_config xini_cfg;
                xini_cfg.set_file(COMMON_CONFIG_FILE);
                xini_cfg.set_data("CONFIG_COMMON.platform_addr", pkgfrom_ip);
#endif

                // std::string client_ip = getFirstNonLoopbackIPv4();
                auto eth_result = getNonLoopbackIPv4();
                std::string eth_name = eth_result.first;
                std::string client_ip = eth_result.second;
                std::string local_default_gw = get_gw_from_local();
                std::string new_default_gw = modify_ip_as_gw(new_ip, pkg_ip, '.');
                printf("client_ip: %s\n", client_ip.c_str());

                // printf("//******cycle: %d ******//\n", cycle++);
                // continue; // debug for same PGB ip
                if (isSameIp(new_ip, client_ip) && isSameIp(local_default_gw, new_default_gw)) {
                    printf("Received IP Same as local IP\n");
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        stopFlag.store(true);
                        cv.notify_one(); // notify main thread
                    }
                    break; // only hander once reply
                }

                // for ip addr set
#if 1
                #if 1
                std::string netmask = "255.255.255.0";
                std::string iface = eth_name;
                configureNetwork(iface, new_ip, netmask, new_default_gw);
                #endif
                #endif
            } catch (const std::exception& e) {
                LOG_MSG(ERROR, "[%s:%d]Error: %s", __FUNCTION__, __LINE__, e.what());
                continue;
            }
        } else {
            {
                std::lock_guard<std::mutex> lock(mtx);
                stopFlag.store(true);
                cv.notify_one();
            }
            break;
        }
    }
    LOG_MSG(INFO, "[%s:%d]receive_thread is exiting.", __FUNCTION__, __LINE__);
}
#endif

void start_log()
{
    Logger::getInstance().init(LOG_FILE, DEBUG);
    return;
}

void stop_log()
{
    Logger::getInstance().shutdown();
    return;
}

int main()
{
    int broadcast = 1;
    int ret = 0;
    // 打印版本以及构建时间信息
    start_log();
    LOG_MSG(INFO, "[%s:%d]ipApply %s %s", __FUNCTION__, __LINE__, APP_VERSION, BUILD_TIME);

    // 从ip_config.ini读取使用模式
    xini_config cfg;
    int modeInt;
    if (cfg.set_file(IP_CONFIG_FILE)) {
        modeInt = cfg.get_data("interface.ipApply_use_mode", 1);
        LOG_MSG(INFO, "[%s:%d]IP config mode:%d", __FUNCTION__, __LINE__, modeInt);
        switch (modeInt) {
            case 1:
                LOG_MSG(INFO, "[%s:%d]auto get ip.", __FUNCTION__, __LINE__);
                break;
            case 2: {
                ret = ipset_from_config(IP_CONFIG_FILE);
                if (ret) {
                    LOG_MSG(ERROR, "[%s:%d]ipset_from_config failed!!!", __FUNCTION__, __LINE__);
                    return ret;
                } else {
                    LOG_MSG(INFO, "[%s:%d]Success:ipset_from_config done!!!", __FUNCTION__, __LINE__);
                }
                return ret;
            }
            case 3:
                LOG_MSG(INFO, "[%s:%d]Function not in use", __FUNCTION__, __LINE__);
                return ret;
            default:
                LOG_MSG(ERROR, "[%s:%d]Unsupported configuration mode: %s", __FUNCTION__, __LINE__, mode.c_str());
                return -1;
        }
    } else {
        LOG_MSG("[%s:%d]IP config mode:%d", __FUNCTION__, __LINE__, modeInt);
        return -1;
    }

    // config default gw
#if 0
    std::string client_ip = getFirstNonLoopbackIPv4();
    printf("client ip is %s\n", client_ip.c_str());
    fflush(stdout);
    try {
        std::string default_gw = modify_ip_as_gw(client_ip, '.');
        std::string command = "route add default gw " + default_gw;
        int result = system(command.c_str());
        if (result != 0) {
            printf("Failed to config gw %s\n", command.c_str());
        } else {
            printf("default gw config to : %s\n", command.c_str());
        }
    } catch (const std::exception& e) {
        printf("Error:%s\n", e.what());
    }
#endif

    // 创建UDP套接字
    SocketRAII sockfd(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LOG_MSG(ERROR, "[%s:%d]socket creation failed", __FUNCTION__, __LINE__);
        perror("socket creation failed");
        return -1;
    }
#ifndef _WINDOWS
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]setsockopt SO_BROADCAST failed", __FUNCTION__, __LINE__);
        perror("setsockopt SO_BROADCAST failed\n");
        return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]setsockopt SO_REUSEADDR failed", __FUNCTION__, __LINE__);
        perror("setsockopt SO_REUSEADDR failed\n");
        return -1;
    }
#endif

    slot_type = 0;
    slot_id = 0xffff; // slot_id号根据拨码开关获取
    uint16_t cmd = DISPATCH_IP_BY_SERVER;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    std::thread recv_thread(receive_thread);
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("255.255.255.255");
    serveraddr.sin_port = htons(SERVER_PORT);

    while (!stopFlag.load() && std::chrono::steady_clock::now() < deadline) {
        // 在此处获取板类型和槽位号, 是因为在启动的那一刻, 可能设备上的硬件和驱动都没有准备好
        // 所以在每次循环中都去用驱动接口读取一次板类型和槽位号
#if defined(DEV_TYP_CP_SYNC)
        int sync_slot = 0;
        ret = get_cpsync_slot(&sync_slot);
        if (ret != 0) {
            LOG_MSG(ERROR, "[%s:%d]get_cpsync_slot get slot:%x ret:%x failed, please check!!!", __FUNCTION__, __LINE__, sync_slot, ret);
            ssleep(5);
            continue;
        }
        // 因为板类型与RDBI产品冲突, CP产品SYNC板类型与0xFA00 & 0xFA00
        slot_type = (uint16_t)(sync_slot & 0xFF00) >> 8;
        slot_id = (uint16_t)sync_slot & 0xFF;
        LOG_MSG(INFO, "[%s:%d]get_cpsync_slot sync_slot:%x slottype:%x slotid:%x", __FUNCTION__, __LINE__, sync_slot, slot_type, slot_id);
#elif defined(DEV_TYP_CP_RK3588)
        // rk3588获取板类型和槽位号
        slot_type = lib_rca_get_board_type();
        LOG_MSG(INFO, "[%s:%d]lib_rca_get_rk3588 get slottype:%x", __FUNCTION__, __LINE__, slot_type);
        // pem_slotid: 0-15
        // rk3588 slotid: 0-3
        int pem_slotid = 0;
        int rca_slotid = 0;
        ret = lib_rca_get_slots(&pem_slotid, &rca_slotid, &rk3588_slotid);
        if (ret != 0) {
            LOG_MSG(ERROR, "[%s:%d]lib_rca_get_rk3588 slots get pem:%x rca:%x rk3588:%d", __FUNCTION__, __LINE__, pem_slotid, rca_slotid, rk3588_slotid);
        }
        slot_id = (uint16_t)(pem_slotid * 16) + (rca_slotid * 4) + rk3588_slotid; // 由此规则得出计算范围为: 0-255
        LOG_MSG(INFO, "[%s:%d]lib_rca_get_rk3588 slots get real slotid:%d", __FUNCTION__, __LINE__, slot_id);
#elif defined(DEV_TYP_RDBI)
        ret = DRV_GetSlotId(&slot_id);
        if (ret != DRV_OK) {
            LOG_MSG(ERROR, "[%s:%d]DRV_GetSlotId failed, please check!!!", __FUNCTION__, __LINE__);
            perror("DRV_GetSlotId failed");
            ssleep(5);
            continue;
        }
        int signed_slot_type = DRV_GetBoardType();
        if (signed_slot_type < 0) {
            LOG_MSG(ERROR, "[%s:%d]DRV_GetBoardType get slot type failed, please check!!!", __FUNCTION__, __LINE__);
            perror("DRV_GetBoardType get slot type failed");
            ssleep(5);
            continue;
        }
        slot_type = (uint16_t)signed_slot_type;
#endif

        // constructor request message
        IPAssignmentMessage request_msg(MESSAGE_HEADER_SERVER, cmd, slot_type, slot_id, 0, 0, 0);
        uint8_t request_buffer[sizeof(request_msg)];
        request_msg.serialize(request_buffer);

        // 发送广播请求
        if (sendto(sockfd, request_buffer, sizeof(request_msg), 0,
                    (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
            LOG_MSG(ERROR, "[%s:%d]sendto failed, errno: %d", __FUNCTION__, __LINE__, errno);
            perror("sendto failed\n");
            ssleep(5); // 发送失败, 应延时之后继续发送, 因为自启动时, 可能网口IP还未正确有限配置
            continue;
        }
        LOG_MSG(INFO, "[%s:%d]Sent request for IP assignment to server", __FUNCTION__, __LINE__);
        LOG_MSG(INFO, "[%s:%d]sendto msg buffer: %d slot type: %d slot id: %d", __FUNCTION__, __LINE__, cmd, slot_type, slot_id);
        fflush(stdout);

        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, std::chrono::seconds(5), []{ return stopFlag.load(); })) {
            break;
        }
    }

    stopFlag.store(true);
    recv_thread.join();

#ifndef _WINDOWS
    if (std::chrono::steady_clock::now() >= deadline) {
        LOG_MSG(WARNING, "[%s:%d]can't get IP from server, please check connect.", __FUNCTION__, __LINE__);
        // set ip from config
        ret = ipset_from_config(IP_CONFIG_FILE);
        if (ret) {
            LOG_MSG(ERROR, "[%s:%d]ipset_from_config failed!!!", __FUNCTION__, __LINE__);
        } else {
            LOG_MSG(INFO, "[%s:%d]Success:ipset_from_config done!!!", __FUNCTION__, __LINE__);
        }
    }
#endif
    stop_log();
    return ret;
}