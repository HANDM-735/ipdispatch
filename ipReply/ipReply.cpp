#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <ifaddrs.h>
#include <netdb.h>
#include <message.h>
#include <net/if.h>
#include <sstream>
#include <vector>
#include <iomanip>
#include <stdexcept>
#include <sys/types.h>
#include <memory>
#include <array>
#include <chrono>
#include <ctime>
#include <ipReply.hpp>

/*
当前ipReply、ipApply全部采用发送有限广播（255.255.255.255），消息只会在同一广播域（即同一个局域网）内，
ipReply监听所有接口（0.0.0.0: 11020）
问题1、服务器或者单板接收不到或发送不出去消息问题
解决方案: 服务器配置路由表，跟网关配置在哪个网口无关
此条命令用于解决接收到的有限广播消息经过eth1网口进来
sudo ip route add 255.255.255.255 dev eth1  # eth1是内网接口名
# 或者
sudo route add -host 255.255.255.255 dev eth1

此条命令用于解决发送的有线广播消息经过eth1网口出去
# 确保内网网段通过内网接口
sudo ip route add 192.168.10.0/24 dev eth1

或者代码明确绑定到内网接口
sock.bind(('192.168.10.10', 12345))
*/

const char* APP_VERSION = "version_module:V1.0.8";
const char* BUILD_TIME = "build_time:" __DATE__ " " __TIME__ "";

DevIpAddrList IpAddrList;
std::string ipaddrMapFile;

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

std::string get_current_time(int mode)
{
    std::time_t t = std::time(nullptr);
    std::tm* local = std::localtime(&t);
    char buf[TIME_BUFF_LEN];
    if (mode == 1) {
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local);
    } else {
        std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", local);
    }
    return std::string(buf);
}

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

std::string get_gw_from_local()
{
    std::array<char, 128> buffer;
    std::string result;
    std::string line;
    std::string default_gw;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("ip route", "r"), pclose);
    if (!pipe) {
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

    return default_gw;
}

std::string getFirstNonLoopbackIPv4()
{
    std::string ip;
#ifndef _WINDOWS
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
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
#endif /* _WINDOWS */
    return ip;
}

// 依赖root权限
bool ip_is_pass_with_ping(const std::string& ip)
{
    IPDetector detector(IPDetector::DetectionMethod::AUTO); // 使用ARP和ICMP两种方法探测
    bool active = detector.isIPActive(ip, 100); // 100ms超时
    LOG_MSG(INFO, "[%s:%d]ping ip: %s %s", __FUNCTION__, __LINE__, ip.c_str(), (active ? " is OK" : " is NOT OK"));
    return active;
}

void write_date_to_ipaddrmap_file(uint16_t slot_type, uint16_t slot_id, uint16_t offset, const std::string& ip)
{
    std::ofstream map_file(ipaddrMapFile, std::ios_base::app);
    if (!map_file.is_open()) {
        LOG_MSG(ERROR, "[%s:%d]Failed to open file for writing filehead: %s", __FUNCTION__, __LINE__, ipaddrMapFile.c_str());
        return;
    }
    map_file << get_current_time(1).c_str() << std::string(6, ' ') << slot_type << std::string(12, ' ') << slot_id << std::string(9, ' ') << offset << std::string(8, ' ') << ip.c_str() << std::endl;
    map_file.close();
    return;
}

int start_init_ipaddr_map()
{
    // 1、添加本端ip地址
    std::string host_ip = getFirstNonLoopbackIPv4();
    if (host_ip.empty()) {
        LOG_MSG(ERROR, "[%s:%d]Host IP address was NULL", __FUNCTION__, __LINE__);
        return -1;
    }
    ipaddrMapFile = IPADR_MAP_FILE + get_current_time(2) + ".log";
    std::ofstream map_file(ipaddrMapFile);
    if (!map_file.is_open()) {
        LOG_MSG(ERROR, "[%s:%d]Failed to open file for writing filehead: %s", __FUNCTION__, __LINE__, ipaddrMapFile.c_str());
        return -1;
    }
    map_file << std::string(7, ' ') << "DATE" << std::string(10, ' ') << "SLOT_TYPE" << std::string(7, ' ') << "SLOT_ID" << std::string(7, ' ') << "OFFSET" << std::string(7, ' ') << "IPADDR" << std::endl;
    map_file.close();
    IpAddrList.addNode(get_current_time(1), 0xff, 0xff, host_ip);
    write_date_to_ipaddrmap_file(0xff, 0xff, 0, host_ip);

    // 2、添加客户端网关ip地址
    std::string gateway = get_gw_from_local();
    if (gateway.empty()) {
        LOG_MSG(ERROR, "[%s:%d]Gateway IP address was NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    std::vector<std::string> ipparts = split_single_char_delim(host_ip, '.');
    if (ipparts.size() != 4) {
        LOG_MSG(ERROR, "[%s:%d]Host IP address format was Invalid", __FUNCTION__, __LINE__);
        return -1;
    }

    // 3、添加网络标识地址和广播标识地址
    std::string net_ip = ipparts[0] + "." + ipparts[1] + "." + ipparts[2] + "." + "0";
    std::string broadcast_ip = ipparts[0] + "." + ipparts[1] + "." + ipparts[2] + "." + "255";
    IpAddrList.addNode(get_current_time(1), 0xff, 0xff, gateway);
    IpAddrList.addNode(get_current_time(1), 0xff, 0xff, net_ip);
    IpAddrList.addNode(get_current_time(1), 0xff, 0xff, broadcast_ip);
    write_date_to_ipaddrmap_file(0xff, 0xff, 0, gateway);
    write_date_to_ipaddrmap_file(0xff, 0xff, 0, net_ip);
    write_date_to_ipaddrmap_file(0xff, 0xff, 0, broadcast_ip);

#ifdef DEBUG_PRINT
    IpAddrList.printDevIpAddrInfo();
#endif

    return 0;
}

void running_add_ipaddr_map_new(std::string& ipaddr, const std::string& client_ip, uint16_t slot_type, uint16_t slot_id, char delim)
{
    // 分配的ip与客户端ip地址相同时
    if (ipaddr == client_ip) {
        dev_ipaddr_info *tmp_node = IpAddrList.findNodeBySlot(slot_type, slot_id);
        if (tmp_node == nullptr) { // 内存中不存在，则保存
            IpAddrList.addNode(get_current_time(1), slot_type, slot_id, ipaddr);
        } else { // 存在则更新信息
            tmp_node->ipaddr = ipaddr;
            tmp_node->time = get_current_time(1);
        }
        return;
    }

    // 需要解决同单板下发多次消息，ipReply接收收到多次相同消息。对于slot_type & slot_id是唯一的。
    dev_ipaddr_info *node = IpAddrList.findNodeBySlot(slot_type, slot_id);
    if (ip_is_pass_with_ping(ipaddr)) { // 分配的IP地址能够ping通的场景下
        if (node != nullptr && node->ipaddr == ipaddr) {
            // 分配的IP地址与内存中slottype slotid对应的IP地址相同，则不往下分配
            LOG_MSG(ERROR, "[%s:%d]slot_type:%d slot_id:%d ip:%s already exists!", __FUNCTION__, __LINE__, slot_type, slot_id, ipaddr.c_str());
            ipaddr.clear();
            return;
        }
        std::vector<std::string> ipparts = split_single_char_delim(ipaddr, delim);
        if (ipparts.size() != 4) {
            throw std::invalid_argument("Invalid IP address format");
        }
        ipaddr.clear();
        for (int i = IPADR_MIN_VALUE; i < IPADR_MAX_VALUE; i++) {
            ipaddr = ipparts[0] + "." + ipparts[1] + "." + ipparts[2] + "." + std::to_string(i);
            // 若已存在，且ping不通，则复用
            node = IpAddrList.findNodeByIpaddr(ipaddr);
            if (node != nullptr && !ip_is_pass_with_ping(ipaddr)) {
                if (node->slot_type != 0xff && node->slot_id != 0xff) {
                    node->slot_type = slot_type;
                    node->slot_id = slot_id;
                    node->time = get_current_time(1);
                }
                break;
            }
        }
        // 若不存在，且ping不同，则直接使用
        if (node == nullptr && !ip_is_pass_with_ping(ipaddr)) {
            IpAddrList.addNode(get_current_time(1), slot_type, slot_id, ipaddr);
            break;
        }
    } else { // 分配的IP地址不能够ping通的场景下
        if (node != nullptr) {
            // 内存中有信息，且分配的IP地址无法ping通，则占用此IP，并更新内存信息
            node->time = get_current_time(1);
            node->ipaddr = ipaddr;
        } else {
            // 内存中没有信息，且分配的IP地址无法ping通，则直接使用此IP
            IpAddrList.addNode(get_current_time(1), slot_type, slot_id, ipaddr);
        }
    }
    return;
}

uint32_t ip_string_to_uint32(const std::string& ip, char delim)
{
    std::vector<std::string> parts = split_single_char_delim(ip, delim);
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t segment = std::stoi(parts[3 - i]);
        result |= segment << (i * 8);
    }
    return result;
}

std::string unpack_ip_from_uint32(uint32_t ip)
{
    std::vector<std::string> parts(4);
    for (int i = 0; i < 4; ++i) {
        uint8_t segment = (ip >> (i * 8)) & 0xFF;
        parts[3 - i] = std::to_string(segment);
    }
    if (parts.size() != 4) {
        throw std::invalid_argument("Invalid number of IP parts");
    }
    std::string dispatch_ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
    return dispatch_ip;
}

uint32_t modify_ip_as_dispatch(const std::string& ip, int slot, char delim)
{
    std::vector<std::string> parts = split_single_char_delim(ip, delim);
    if (parts.size() != 4) {
        throw std::invalid_argument("Invalid IP address format");
    }

    std::string gw = get_gw_from_local();
    std::vector<std::string> gw_parts = split_single_char_delim(gw, delim);
    if (gw_parts.size() != 4) {
        throw std::invalid_argument("Invalid IP address format");
    }

    if (gw_parts[3] != "0" || gw_parts[3] != "255") {
        parts[0] = gw_parts[3];
    }

    int new_last_segment = slot + OLD_FOURTH_ADDR_OFFSET;
    if (new_last_segment < 0 || new_last_segment > 255) {
        throw std::out_of_range("New last segment is out of range [0, 255]");
    }

    parts[3] = std::to_string(new_last_segment);
    std::string new_ip_printf = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
    LOG_MSG(INFO, "[%s:%d]new_ip_printf: %s", __FUNCTION__, __LINE__, new_ip_printf.c_str());

    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        uint32_t segment = std::stoi(parts[3 - i]);
        result |= segment << (i * 8);
    }

    return result;
}

uint32_t find_sync_ip_as_dispatch(uint16_t slot_type, uint16_t slot_id, char delim)
{
    struct topo_map {
        uint16_t sync_slot_type;
        uint16_t sync_slot_id;
        uint16_t board_type;
        uint16_t slot_id_start;
        uint16_t slot_id_end;
    };
    topo_map topo_arr[] = {
        // 此处代表含义: sync板type:1 id:0 对应pgb板type:2 id: {0-63}
        // 通过查找对应的pgb板type，进一步查找id是否在pgbid范围，从而确定sync板对应的ip id
        { CP_SYNC_BOARD_TYPE, 0, CP_RK3588_BOARD_TYPE, 0, 64 },
        { CP_SYNC_BOARD_TYPE, 1, CP_RK3588_BOARD_TYPE, 64, 128 },
        { CP_SYNC_BOARD_TYPE, 2, CP_RK3588_BOARD_TYPE, 128, 192 },
        { CP_SYNC_BOARD_TYPE, 3, CP_RK3588_BOARD_TYPE, 192, 256 },
        { FT_SYNC_BOARD_TYPE, 0, FT_PGB_BOARD_TYPE, 0, 4 },
        { FT_SYNC_BOARD_TYPE, 1, FT_PGB_BOARD_TYPE, 0, 8 },
    };

    uint16_t sync_slot_type = 0xffff;
    uint16_t sync_slot_id = 0xffff;
    for (uint32_t i = 0; i < sizeof(topo_arr) / sizeof(topo_arr[0]); i++) {
        if (slot_type == topo_arr[i].board_type) {
            if (slot_id >= topo_arr[i].slot_id_start && slot_id < topo_arr[i].slot_id_end) {
                sync_slot_type = topo_arr[i].sync_slot_type;
                sync_slot_id = topo_arr[i].sync_slot_id;
                break;
            }
        }
    }

    if (sync_slot_type == 0xffff || sync_slot_id == 0xffff) {
        throw std::invalid_argument("Can't find valid sync board type or id!");
    }

    dev_ipaddr_info *ipaddr_node = IpAddrList.findNodeBySlot(sync_slot_type, sync_slot_id);
    if (ipaddr_node == nullptr) {
        throw std::invalid_argument("Can't find sync ip!");
    }

    std::vector<std::string> ip_parts = split_single_char_delim(ipaddr_node->ipaddr, delim);
    if (ip_parts.size() != 4) {
        throw std::invalid_argument("Invalid IP address format");
    }

    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        uint32_t segment = std::stoi(ip_parts[3 - i]);
        result |= segment << (i * 8);
    }

    return result;
}

static uint16_t ipoffset;
std::string ipreply_set_dispatch_ip_new(const std::string& local_ip, uint32_t netmask, uint16_t slot_type, uint16_t slot_id)
{
    struct ip_allocation_area {
        uint16_t slot_type;
        uint16_t offset;
        uint16_t max;
    } ip_area[] = {
        { CP_SYNC_BOARD_TYPE, CP_SYNC_IP_OFFSET, CP_SYNC_MAX },
        { CP_RK3588_BOARD_TYPE, CP_RK3588_IP_OFFSET, CP_RK3588_MAX },
        { FT_SYNC_BOARD_TYPE, FT_SYNC_IP_OFFSET, FT_SYNC_MAX },
        { FT_PGB_BOARD_TYPE, FT_PGB_IP_OFFSET, FT_PGB_MAX },
        { RDBI_PGB_BOARD_TYPE, RDBI_PGB_IP_OFFSET, RDBI_PGB_MAX },
        { RDBI_MONITOR_BOARD_TYPE, RDBI_MONITOR_IP_OFFSET, RDBI_MONITOR_MAX },
        { RDBI_HOTSIMULATE_BOARD_TYPE, RDBI_HOTSIMULATE_OFFSET, ROBI_HOTSIMULATE_MAX },
    };

    uint16_t offset = 0xffff;
    uint16_t max = 0;
    for (uint32_t i = 0; i < sizeof(ip_area) / sizeof(ip_area[0]); i++) {
        if (slot_type == ip_area[i].slot_type) {
            offset = ip_area[i].offset;
            max = ip_area[i].max;
            break;
        }
    }

    if (offset == 0xffff || max == 0) {
        LOG_MSG(ERROR, "[%s:%d]slottype:%d slotid:%d Can't find the slottype!", __FUNCTION__, __LINE__, slot_type, slot_id);
        return "";
    }

    if (slot_id >= max) {
        LOG_MSG(ERROR, "[%s:%d]slottype:%d slotid:%d offset:%d max:%d set dispatch ip failed!", __FUNCTION__, __LINE__, slot_type, slot_id, offset, max);
        return "";
    }

    ipoffset = offset;
    // 当前只有CP产品rk3588槽位数量超过254,需要考虑进位
    uint32_t carry_bit = 0;
    uint32_t logic_slotid = slot_id;
    if (slot_type == CP_RK3588_BOARD_TYPE) {
        if (slot_id >= CP_RK3588_SLOTID_THRESHOLD) {
            carry_bit = 1;
            logic_slotid = slot_id - CP_RK3588_SLOTID_THRESHOLD;
        }
    } else if (slot_type == RDBI_PGB_BOARD_TYPE) {
        logic_slotid = slot_type + slot_id; // 因历史上采用100 + slot_type + slot_id的分配规则，要求保持一致
    }
    LOG_MSG(INFO, "[%s:%d]carry bit:%d logic slot id: %d", __FUNCTION__, __LINE__, carry_bit, logic_slotid);

    uint32_t fourth_addr = offset + logic_slotid;
    if (fourth_addr <= 0 || fourth_addr > UINT8_MAX) {
        LOG_MSG(ERROR, "[%s:%d]offset:%d logic slotid:%d fourth addr:%d set dispatch ip failed!", __FUNCTION__, __LINE__, offset, logic_slotid, fourth_addr);
        return "";
    }

    uint32_t ip_value = ip_string_to_uint32(local_ip, '.');
    std::string net_ip = unpack_ip_from_uint32((ip_value & netmask) | (carry_bit << 8));
    std::vector<std::string> parts = split_single_char_delim(net_ip, '.');
    std::string new_ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + std::string::to_string(fourth_addr);
    LOG_MSG(INFO, "[%s:%d]get new ip:%s", __FUNCTION__, __LINE__, new_ip.c_str());
    return new_ip;
}

std::string ipreply_set_dispatch_ip(const std::string& local_ip, uint32_t netmask, uint16_t slot_type, uint16_t slot_id)
{
    uint32_t ip_value = ip_string_to_uint32(local_ip, '.');
    std::string net_ip = unpack_ip_from_uint32(ip_value & netmask);
    std::vector<std::string> parts = split_single_char_delim(net_ip, '.');
    int fourth_addr = slot_id + slot_type + NWE_FOURTH_ADDR_OFFSET;
    ipoffset = NWE_FOURTH_ADDR_OFFSET;
    if (fourth_addr <= 0 || fourth_addr >= 255) {
        fourth_addr = 1;
    }
    std::string new_ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + std::to_string(fourth_addr);
    return new_ip;
}

uint32_t get_dispatch_ip_by_slot(const std::string& local_ip, const std::string& client_ip, uint32_t netmask, uint16_t slot_type, uint16_t slot_id)
{
    std::string new_ip = ipreply_set_dispatch_ip_new(local_ip, netmask, slot_type, slot_id);
    if (new_ip.empty()) {
        throw std::invalid_argument("Invalid IP address format was Null");
    }
    running_add_ipaddr_map(new_ip, client_ip, slot_type, slot_id, '.');
    if (new_ip.empty()) {
        throw std::invalid_argument("Invalid IP address format was NULL");
    }
    LOG_MSG(INFO, "[%s:%d]slot_type %d slot_id %d offset %d IP %s", __FUNCTION__, __LINE__, slot_type, slot_id, ipoffset, new_ip.c_str());
    write_date_to_ipaddrmap_file(slot_type, slot_id, ipoffset, new_ip);
    return ip_string_to_uint32(new_ip, '.');
}

std::vector<uint32_t> find_hot_simulation_ip_as_dispatch(uint16_t slot_type, uint16_t slot_id)
{
#define BOARD_NUM_MAX       (256) // 当前同类型单板数量最大为256
    std::vector<uint32_t> iplist;
    for (uint16_t i = 0; i < BOARD_NUM_MAX; i++) {
        dev_ipaddr_info *node = IpAddrList.findNodeBySlot(slot_type, i);
        if (node == NULL) {
            continue;
        }
        iplist.push_back(ip_string_to_uint32(node->ipaddr, '.'));
    }
    return iplist;
}

uint32_t find_single_board_ip_as_dispatch(uint16_t slot_type, uint16_t slot_id)
{
    dev_ipaddr_info *node = IpAddrList.findNodeBySlot(slot_type, slot_id);
    if (node == nullptr) {
        LOG_MSG(ERROR, "[%s:%d]Can't find slottype %d slotid %d ip!", __FUNCTION__, __LINE__, slot_type, slot_id);
        throw std::invalid_argument("Can't find IP");
    }
    return ip_string_to_uint32(node->ipaddr, '.');
}

int construct_reply_msg(const std::string& client_ip, char *buffer, size_t recv_len, uint8_t **reply_buffer, size_t *reply_len, uint16_t *header)
{
    uint16_t recv_header = 0;
    uint16_t recv_slotid = 0;
    uint32_t recv_ip = 0;
    uint16_t dispatch_ip = 0;
    uint16_t send_header = MESSAGE_HEADER_CLIENT;

    if(recv_len == 8) { // 老版本ipApply报文格式构成uint16_t header, uint16_t slot_id, uint32_t ipaddr;所以消息字节数为：2 + 2 + 4 = 8
        // parse recv msg
        memcpy(&recv_header, buffer, sizeof(recv_header));
        memcpy(&recv_slotid, buffer + sizeof(recv_header), sizeof(recv_slotid));
        memcpy(&recv_ip, buffer + sizeof(recv_header) + sizeof(recv_slotid), sizeof(recv_ip));

        LOG_MSG(WARNING, "[%s:%d]OLD VERSION ipApply! recvfm buffer header:%x, slot_id:%d", __FUNCTION__, __LINE__, recv_header, recv_slotid);
        fflush(stdout);

        // 老版本ipApply只有一个功能自动获取分配ip功能
        if (recv_header == MESSAGE_HEADER_SERVER) {
            device_ip = "1.1.1.1";
            try {
                dispatch_ip = modify_ip_as_dispatch(device_ip, recv_slotid, '.');
            } catch (const std::exception& e) {
                LOG_MSG(ERROR, "[%s:%d]Error: %s", __FUNCTION__, __LINE__, e.what());
                return -1;
            }
        }

        // construct send msg
        memcpy(*reply_buffer, &send_header, sizeof(send_header));
        memcpy(*reply_buffer + sizeof(send_header), &recv_slotid, sizeof(recv_slotid));
        memcpy(*reply_buffer + sizeof(send_header) + sizeof(recv_slotid), &dispatch_ip, sizeof(dispatch_ip));
        *reply_len = sizeof(send_header) + sizeof(recv_slotid) + sizeof(dispatch_ip);
        LOG_MSG(INFO, "[%s:%d]sendto buffer len:%lu slot id: %d Original IP: %x, dispatch ip: %d", __FUNCTION__, __LINE__, *reply_len, recv_slotid, device_ip.c_str(), dispatch_ip);
    } else {
        // 新版本
        // parse message
        IPAssignmentMessage received_msg = IPAssignmentMessage::deserialize(reinterpret_cast<const uint8_t*>(buffer));
        LOG_MSG(INFO, "[%s:%d]NEW VERSION ipApply! recvfm buffer header: 0x%x, cmd: 0x%x, slot_type: %d, slot_id: %d, ip: %d, netmask: %d gateway: %d", __FUNCTION__, __LINE__, received_msg.header, received_msg.cmd, received_msg.slot_type, received_msg.slot_id, received_msg.ip_address, received_msg.netmask, received_msg.gw_address);
        fflush(stdout);

        recv_header = received_msg.header;
        recv_slottype = received_msg.slot_type;
        recv_slotid = received_msg.slot_id;

        if (recv_header == MESSAGE_HEADER_SERVER) {
            uint32_t dispatch_ip = 0;
            uint32_t dispatch_gateway = 0;
            uint16_t cmd = received_msg.cmd;
            std::vector<uint32_t> device_ip;
            uint16_t ip_cnt = 0;

            // 构造/获取ip信息
            try {
                if (cmd == DISPATCH_IP_BY_SERVER) {
                    // 当前/L最大支持分配254个IP地址，若后续需要扩展大可分配IP地址数量，可以修改为/23-16
                    // 约束条件:
                    // 1. 局域网，服务器局域网对应的网卡需手动或者自动设置ip、netmask、route等
                    // 2. 修改 get_dispatch_ip_by_slot 接口实现，网络统一广播地址之间的地址即可分配IP地址数量
                    std::string local_ip = getFirstNonLoopbackIPv4();
                    if (recv_slottype == CP_SYNC_BOARD_TYPE || recv_slottype == CP_RK3588_BOARD_TYPE) {
                        dispatch_netmask = ip_string_to_uint32(NETWORK_NETMAKE_24); // 可优化为通过配置文件获取
                    } else {
                        dispatch_netmask = ip_string_to_uint32(NETWORK_NETMAKE_24, '.');
                    }
                    dispatch_ip = get_dispatch_ip_by_slot(local_ip, client_ip, dispatch_netmask, recv_slottype, recv_slotid);
                    dispatch_gateway = dispatch_ip;
                } else if (cmd == GET_SYNC_IP_BY_SERVER) {
                    dispatch_ip = find_sync_ip_as_dispatch(recv_slottype, recv_slotid, '.');
                    dispatch_gateway = dispatch_ip;
                } else if (cmd == GET_FAN_MONITOR_IP_BY_SERVER) {
                    dispatch_ip = find_single_board_ip_as_dispatch(recv_slottype, recv_slotid);
                    dispatch_gateway = dispatch_ip;
                } else if (cmd == GET_HOT_SIMULATE_BAORD_IP_BY_SERVER) {
                    device_ip = find_hot_simulation_ip_as_dispatch(recv_slottype, recv_slotid);
                    if (device_ip.empty()) {
                        LOG_MSG(ERROR, "[%s:%d]cmd %x slot type %d slot id %d get hot simulation board ip is empty!", __FUNCTION__, __LINE__, cmd, recv_slottype, recv_slotid);
                        return -1;
                    }
                    ip_cnt = device_ip.size();
                }
            } catch (const std::exception& e) {
                LOG_MSG(ERROR, "[%s:%d]Error: %s", __FUNCTION__, __LINE__, e.what());
                return -1;
            }

            // 构造发送消息
            if (cmd == DISPATCH_IP_BY_SERVER || cmd == GET_SYNC_IP_BY_SERVER || cmd == GET_FAN_MONITOR_IP_BY_SERVER) {
                // constructor reply message
                IPAssignmentMessage reply_msg(send_header, cmd, recv_slottype, recv_slotid, dispatch_ip, dispatch_netmask, dispatch_gateway);
                reply_msg.serialize(*reply_buffer);
                *reply_len = sizeof(reply_msg);
                std::string buffer_ip = unpack_ip_from_uint32(dispatch_ip);
                std::string buffer_netmask = unpack_ip_from_uint32(dispatch_netmask);
                std::string buffer_gateway = unpack_ip_from_uint32(dispatch_gateway);
                LOG_MSG(INFO, "[%s:%d]sendto buffer len:%lu cmd: %d slot type: %d slot id: %d ip: %s netmask: %s gateway: %s", __FUNCTION__, __LINE__, *reply_len, cmd, recv_slottype, recv_slotid, buffer_ip.c_str(), buffer_netmask.c_str(), buffer_gateway.c_str());
            } else if (cmd == GET_HOT_SIMULATE_BAORD_IP_BY_SERVER) {
                // constructor reply message
                memcpy(*reply_buffer, &send_header, sizeof(send_header));
                memcpy(*reply_buffer + sizeof(send_header), &cmd, sizeof(cmd));
                memcpy(*reply_buffer + sizeof(send_header) + sizeof(cmd), &recv_slottype, sizeof(recv_slottype));
                memcpy(*reply_buffer + sizeof(send_header) + sizeof(cmd) + sizeof(recv_slottype), &recv_slotid, sizeof(recv_slotid));
                memcpy(*reply_buffer + sizeof(send_header) + sizeof(cmd) + sizeof(recv_slottype) + sizeof(recv_slotid), &ip_cnt, sizeof(ip_cnt));
                LOG_MSG(INFO, "[%s:%d]sendto buffer len:%lu cmd: %d slot type: %d slot id: %d ipcnt: %d", __FUNCTION__, __LINE__, *reply_len, cmd, recv_slottype, recv_slotid, ip_cnt);
                for (uint32_t i = 0; i < ip_cnt; i++) {
                    std::string tmpip = unpack_ip_from_uint32(device_ip[i]);
                    LOG_MSG(INFO, "[%s:%d]index: %d: device_ip: %d IP: %s", __FUNCTION__, __LINE__, i, device_ip[i], tmpip.c_str());
                    memcpy(*reply_buffer + sizeof(send_header) + sizeof(cmd) + sizeof(recv_slottype) + sizeof(recv_slotid) + sizeof(ip_cnt) + sizeof(uint32_t) * i, &device_ip[i], sizeof(uint32_t));
                }
                *reply_len = sizeof(send_header) + sizeof(cmd) + sizeof(recv_slottype) + sizeof(recv_slotid) + sizeof(ip_cnt) + sizeof(uint32_t) * ip_cnt;
            }
        }
    }

    *header = recv_header;
    return 0;
}

int main()
{
    struct sockaddr_in serveraddr, clientaddr;
    char buffer[BUFFER_SIZE] = {0};
    socklen_t len = sizeof(clientaddr);

    start_log();

    // 打印版本以及构建时间信息
    LOG_MSG(INFO, "[%s:%d]ipReply %s %s", __FUNCTION__, __LINE__, APP_VERSION, BUILD_TIME);
    int ret = start_init_ipaddr_map();
    if (ret) {
        return ret;
    }

    // create udp socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LOG_MSG(ERROR, "[%s:%d]socket creation failed!", __FUNCTION__, __LINE__);
        return -1;
    }

    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt SO_BROADCAST failed");
        return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        LOG_MSG(ERROR, "[%s:%d]bind failed!", __FUNCTION__, __LINE__);
        return -1;
    }

    LOG_MSG(INFO, "[%s:%d]Server is listening on port %d", __FUNCTION__, __LINE__, SERVER_PORT);
    fflush(stdout);

    // config boardcastaddr
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcastAddr.sin_port = htons(CLIENT_PORT);

    while (true) {
        // receive message
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr *)&clientaddr, &len);
        if (n < 0) {
            LOG_MSG(ERROR, "[%s:%d]recvfrom failed!", __FUNCTION__, __LINE__);
            continue;
        }

        LOG_MSG(INFO, "[%s:%d]recv msg len: %lu clientaddr from: %s", __FUNCTION__, __LINE__, n, inet_ntoa(clientaddr.sin_addr));
        std::string client_ip = inet_ntoa(clientaddr.sin_addr);

        uint8_t reply_buffer[MSG_MAX_LEN];
        uint8_t* ptr_arr = reply_buffer;
        size_t reply_len = 0;
        uint16_t recv_header = 0;

        ret = construct_reply_msg(client_ip, buffer, n, &ptr_arr, &reply_len, &recv_header);
        if (ret != 0) {
            continue;
        }

        if (recv_header == MESSAGE_HEADER_SERVER) {
            // send reply
            LOG_MSG(INFO, "[%s:%d]sendto start!", __FUNCTION__, __LINE__);
            fflush(stdout);
            if (sendto(sockfd, reply_buffer, reply_len, 0,
                       (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr)) < 0) {
                perror("sendto failed");
                continue;
            }
            LOG_MSG(INFO, "[%s:%d]sendto end! data length: %lu:%lu", __FUNCTION__, __LINE__, reply_len, sizeof(reply_buffer));
            

            #ifdef DEBUG_PRINT
                        {
                            IpAddrList.printDevIpAddrListInfo();
                        }
            #endif
            fflush(stdout);
        }
    }

    stop_log();
    return 0;
}