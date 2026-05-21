#ifndef _IPREPLY_HPP
#define _IPREPLY_HPP

#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include <chrono>
#include <string>
#include <system_error>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include "log_format.hpp"

#define MSG_MAX_LEN         (1024)
#define TIME_BUFF_LEN       (80)
#define NWE_FOURTH_ADDR_OFFSET  100
#define OLD_FOURTH_ADDR_OFFSET  0
#define IPADR_MAX_VALUE     (255)
#define IPADR_MAP_FILE      ("/opt/log/ipaddr_map")
#define SYNC_IP_TOPOLOGY_FILE ("./syncip_topology")
#define LAN_IP_CONFIG       ("./lan_ipconfig.ini")
#define LOG_FILE            ("/opt/log/ipReply")

#define NETWORK_NETMAKE_24  ("255.255.255.0")
#define NETWORK_NETMAKE_23  ("255.255.254.0")

// #define DEBUG_PRINT

/* CP产品当前涉及IP的设备范围：服务器、万用表、SYNC板、TH板、MF板、PGB板、PEM-RK3588板
   RDBI产品当前涉及IP的设备范围：服务器、PGB板、风扇监控板、SBT板、热模拟板
   FT产品当前涉及IP的设备范围：服务器、SYNC板、PGB板、TH板、MF板
*/

// 已FT/RDBI项目经理确认单板范围以及槽位号范围，slotid起始值为1。确认CP产品槽位号起始值为0
#define UINT8_MAX           (255)
#define ALL_PRODUCT_IP_BASE (55)        // xx.xx.xx.0/24 为网络标识
#define ALL_PRODUCT_IP_RSV  (20)        // xx.xx.xx.1-20 为保留ip地址，用于服务器、TH板、MF板、万用表、路由等设备分配固定IP，范围为1-20

/* CP产品所有板类型 */
#define CP_SYNC_BOARD_TYPE      (0xFA06)  // 实际板类型是6，加了0xFA00偏移
#define CP_RK3588_BOARD_TYPE    (0xFA07)  // 实际板类型是7，加了0xFA00偏移

/* CP产品所有板类型IP地址范围划分 */
#define CP_RK3588_SLOTID_THRESHOLD (150)
#define CP_SYNC_MAX             (4)       // CP当前支持4块SYNC板
#define CP_RK3588_MAX           (256)     // CP当前最大支持4*1*16=256块RK3588，使用B类IP地址段/23，x0.100-249共150个地址，x1.100-205个地址
#define CP_SYNC_IP_OFFSET       (21)      // 预留出21-30 IP地址段，sloid从0开始，范围0-3
#define CP_RK3588_IP_OFFSET     (100)     // 预留出100-249和100-205 IP地址段，sloid从0开始，范围0-255

/* FT产品所有板类型 */
#define FT_SYNC_BOARD_TYPE      (33)
#define FT_PGB_BOARD_TYPE       (34)

/* FT产品所有板类型IP地址范围划分 */
#define FT_IP_OFFSET            (10)
#define FT_SYNC_MAX             (2)  // FT当前支持2块SYNC板
#define FT_PGB_MAX              (4)  // FT当前支持4块PGB板
#define FT_SYNC_IP_OFFSET       (ALL_PRODUCT_IP_BASE + ALL_PRODUCT_IP_RSV) // 预留出21-30 IP地址段，基于sloid为1分析
#define FT_PGB_IP_OFFSET        (FT_SYNC_IP_OFFSET + FT_IP_OFFSET)          // 预留出31-40 IP地址段，基于sloid为1分析

/* RDBI产品所有板类型 */
#define RDBI_PGB_BOARD_TYPE       (3)
#define RDBI_MONITOR_BOARD_TYPE   (4)
#define RDBI_HOTSIMULATE_BOARD_TYPE (6)

/* RDBI产品所有板类型IP地址范围划分，RDBI左右炉体是隔离开的，单独半边炉体为一个独立系统 */
/* 只有PGB板存在旧版本ipReply规则分配，其他单板均适配新版本 */
// xx.xx.xx.xx (50-100)用于旧版本ipApply申请的ip分配，xx.xx.xx.xx(1-49)用于其他固定IP配置
#define RDBI_PGB_MAX              (30)    // RDBI当前支持30块PGB板
#define RDBI_MONITOR_MAX          (10)    // RDBI当前支持10块风扇监控板
#define RDBI_HOTSIMULATE_MAX      (30)    // RDBI当前支持30块热模拟板
#define RDBI_PGB_IP_OFFSET        (100)   // 104-131IP地址段用于PGB板，sloid从1开始，因要求与历史版本保持一致，所以要求加上板类型
#define RDBI_HOTSIMULATE_IP_OFFSET (150)  // 151-180IP地址段用于热模拟板，sloid从1开始
#define RDBI_MONITOR_IP_OFFSET    (200)   // 200-210IP地址段用于监控板，sloid从0开始

#define ARRAY_SIZE(arg)      (sizeof(arg) / sizeof(arg[0]))

struct dev_ipaddr_info {
    std::string time;
    uint16_t slot_type;
    uint16_t slot_id;
    std::string ipaddr;
    std::unique_ptr<dev_ipaddr_info> next; // 自动管理内存

    // 构造函数
    dev_ipaddr_info(std::string t, uint16_t st, uint16_t si, std::string ip)
        : time(std::move(t)), slot_type(st), slot_id(si), ipaddr(std::move(ip)), next(nullptr) {}

    void print() const {
        LOG_MSG(DEBUG, "slot_type %d slot_id %d ipaddr %s", slot_type, slot_id, ipaddr.c_str());
        if (next) next->print();
    }
};

class DevIpAddrList {
private:
    std::unique_ptr<dev_ipaddr_info> head; // 链表头节点

public:
    DevIpAddrList() = default;
    ~DevIpAddrList() = default; // 自动释放：无需手动析构，指针会自动处理

    // 添加节点到链表尾部
    void addNode(const std::string& time, uint16_t slot_type, uint16_t slot_id, const std::string& ipaddr) {
        auto newNode = std::make_unique<dev_ipaddr_info>(time, slot_type, slot_id, ipaddr);
        if (!head) {
            head = std::move(newNode);
        } else {
            auto current = head.get();
            while (current->next) {
                current = current->next.get();
            }
            current->next = std::move(newNode);
        }
    }

    // 通过slot_type和slot_id获取节点
    dev_ipaddr_info* findNodeBySlot(uint16_t slot_type, uint16_t slot_id) {
        auto* current = head.get();
        while (current) {
            if (current->slot_type == slot_type && current->slot_id == slot_id) {
                return current;
            }
            current = current->next.get();
        }
        return nullptr;
    }

    dev_ipaddr_info* findNodeByIpaddr(const std::string& ipaddr) {
        auto* current = head.get();
        while (current) {
            if (current->ipaddr == ipaddr) {
                return current;
            }
            current = current->next.get();
        }
        return nullptr;
    }

    void printDevIpAddrListInfo() {
        if (head) head->print();
        else LOG_MSG(WARNING, "List is empty!");
    }
};

class IPDetector {
public:
    enum class DetectionMethod {
        AUTO,        // 自动选择最佳方法
        ARP_ONLY,    // 仅使用ARP
        ICMP_ONLY    // 仅使用ICMP
    };

    IPDetector(DetectionMethod method = DetectionMethod::AUTO,
               const std::string& interface = "")
        : method_(method), interface_(interface) {
        // 尝试初始化ARP检测
        if (method == DetectionMethod::AUTO || method == DetectionMethod::ARP_ONLY) {
            try {
                initARP();
                arp_initialized_ = true;
            } catch (const std::system_error& e) {
                if (method == DetectionMethod::ARP_ONLY) {
                    throw std::runtime_error("ARP initialization failed: " +
                                             std::string(e.what()));
                }
                arp_initialized_ = false;
            }
        }

        // 尝试初始化ICMP检测
        if (method == DetectionMethod::AUTO || method == DetectionMethod::ICMP_ONLY) {
            try {
                initICMP();
                icmp_initialized_ = true;
            } catch (const std::system_error& e) {
                if (method == DetectionMethod::ICMP_ONLY) {
                    throw std::runtime_error("ICMP initialization failed: " +
                                             std::string(e.what()));
                }
                icmp_initialized_ = false;
            }
        }

        if (!arp_initialized_ && !icmp_initialized_) {
            throw std::runtime_error("Neither ARP nor ICMP detection could be initialized");
        }
    }

    ~IPDetector() {
        if (arp_sockfd_ >= 0) close(arp_sockfd_);
        if (icmp_sockfd_ >= 0) close(icmp_sockfd_);
    }

    bool isIPActive(const std::string& ip, int timeout_ms = 100) {
        if (method_ == DetectionMethod::AUTO) {
            // 优先使用ARP检测
            if (arp_initialized_) {
                bool arp_result = checkWithARP(ip, timeout_ms);
                if (arp_result) return true;

                // ARP无响应，尝试ICMP
                if (icmp_initialized_) {
                    return checkWithICMP(ip, timeout_ms);
                }
                return false;
            }
            // 只有ICMP可用
            return checkWithICMP(ip, timeout_ms);
        } else if (method_ == DetectionMethod::ARP_ONLY && arp_initialized_) {
            return checkWithARP(ip, timeout_ms);
        } else if (method_ == DetectionMethod::ICMP_ONLY && icmp_initialized_) {
            return checkWithICMP(ip, timeout_ms);
        }
        return false;
    }

private:
    void initARP() {
        // 创建原始套接字
        arp_sockfd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
        if (arp_sockfd_ < 0) {
            throw std::system_error(errno, std::system_category(), "ARP socket creation failed");
        }

        // 获取接口索引和MAC地址
        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        std::strncpy(ifr.ifr_name, interface_.empty() ? getDefaultInterface().c_str() : interface_.c_str(), IFNAMSIZ);

        if (ioctl(arp_sockfd_, SIOCGIFINDEX, &ifr) == -1) {
            close(arp_sockfd_);
            arp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to get interface index");
        }
        ifindex_ = ifr.ifr_ifindex;

        if (ioctl(arp_sockfd_, SIOCGIFHWADDR, &ifr) == -1) {
            close(arp_sockfd_);
            arp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to get MAC address");
        }
        std::memcpy(src_mac_, ifr.ifr_hwaddr.sa_data, 6);

        // 设置非阻塞
        int flags = fcntl(arp_sockfd_, F_GETFL, 0);
        if (flags == -1) {
            close(arp_sockfd_);
            arp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to get socket flags");
        }
        if (fcntl(arp_sockfd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(arp_sockfd_);
            arp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to set non-blocking mode");
        }
    }

    void initICMP() {
        // 创建原始套接字
        icmp_sockfd_ = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (icmp_sockfd_ < 0) {
            throw std::system_error(errno, std::system_category(), "ICMP socket creation failed");
        }

        // 设置超时
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        if (setsockopt(icmp_sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            close(icmp_sockfd_);
            icmp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to set socket timeout");
        }

        // 设置非阻塞
        int flags = fcntl(icmp_sockfd_, F_GETFL, 0);
        if (flags == -1) {
            close(icmp_sockfd_);
            icmp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to get socket flags");
        }
        if (fcntl(icmp_sockfd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(icmp_sockfd_);
            icmp_sockfd_ = -1;
            throw std::system_error(errno, std::system_category(), "Failed to set non-blocking mode");
        }
    }

    bool checkWithARP(const std::string& ip, int timeout_ms) {
        // 构建ARP请求包
        uint8_t packet[60] = {0};

        // 以太网头
        struct ethhdr* eth = reinterpret_cast<struct ethhdr*>(packet);
        std::memset(eth->h_dest, 0xff, 6); // 广播地址
        std::memcpy(eth->h_source, src_mac_, 6);
        eth->h_proto = htons(ETH_P_ARP);

        // ARP头
        struct arphdr* arp = reinterpret_cast<struct arphdr*>(packet + sizeof(struct ethhdr));
        arp->ar_hrd = htons(ARPHRD_ETHER);
        arp->ar_pro = htons(ETH_P_IP);
        arp->ar_hln = 6;
        arp->ar_pln = 4;
        arp->ar_op = htons(ARPOP_REQUEST);

        // 发送方MAC和IP
        std::memcpy(packet + sizeof(struct ethhdr) + sizeof(struct arphdr), src_mac_, 6);
        struct in_addr src_ip;
        inet_pton(AF_INET, "0.0.0.0", &src_ip); // 使用0.0.0.0因为我们不知道自己的IP
        std::memcpy(packet + sizeof(struct ethhdr) + sizeof(struct arphdr) + 6, &src_ip, 4);

        // 目标MAC(全0)和IP
        std::memset(packet + sizeof(struct ethhdr) + sizeof(struct arphdr) + 10, 0, 6);
        struct in_addr dst_ip;
        inet_pton(AF_INET, ip.c_str(), &dst_ip);
        std::memcpy(packet + sizeof(struct ethhdr) + sizeof(struct arphdr) + 16, &dst_ip, 4);

        // 发送ARP请求
        struct sockaddr_ll sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sll_family = AF_PACKET;
        sa.sll_protocol = htons(ETH_P_ARP);
        sa.sll_ifindex = ifindex_;
        sa.sll_halen = ETH_ALEN;
        std::memset(sa.sll_addr, 0xff, ETH_ALEN); // 广播地址

        if (sendto(arp_sockfd_, packet, sizeof(packet), 0,
                   reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
            return false;
        }

        // 使用poll等待响应
        struct pollfd fds;
        fds.fd = arp_sockfd_;
        fds.events = POLLIN;

        auto start = std::chrono::steady_clock::now();
        int remaining_time = timeout_ms;

        while (remaining_time > 0) {
            int ret = poll(&fds, 1, remaining_time);
            if (ret > 0 && (fds.revents & POLLIN)) {
                uint8_t buffer[60];
                ssize_t len = recv(arp_sockfd_, buffer, sizeof(buffer), 0);
                if (len > 0) {
                    // 检查是否是ARP响应
                    struct ethhdr* eth_resp = reinterpret_cast<struct ethhdr*>(buffer);
                    struct arphdr* arp_resp = reinterpret_cast<struct arphdr*>(
                        buffer + sizeof(struct ethhdr));

                    if (ntohs(eth_resp->h_proto) == ETH_P_ARP &&
                        ntohs(arp_resp->ar_op) == ARPOP_REPLY) {
                        // 检查响应是否针对我们的请求
                        struct in_addr resp_ip;
                        std::memcpy(&resp_ip,
                                    buffer + sizeof(struct ethhdr) + sizeof(struct arphdr) + 6,
                                    4);
                        if (resp_ip.s_addr == dst_ip.s_addr) {
                            return true;
                        }
                    }
                }
            } else if (ret == 0) {
                break; // 超时
            }

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            remaining_time = timeout_ms - elapsed.count();
        }

        return false;
    }

    bool checkWithICMP(const std::string& ip, int timeout_ms) {
        // 构建ICMP包
        uint8_t packet[sizeof(struct icmphdr) + 32];
        std::memset(packet, 0, sizeof(packet));

        struct icmphdr* icmp = reinterpret_cast<struct icmphdr*>(packet);
        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->un.echo.id = getpid() & 0xFFFF;
        icmp->un.echo.sequence = seq_++;
        icmp->checksum = checksum(reinterpret_cast<uint16_t*>(icmp), sizeof(packet));

        // 设置目标地址
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        // 发送ICMP请求
        if (sendto(icmp_sockfd_, packet, sizeof(packet), 0,
                   reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <= 0) {
            return false;
        }

        // 使用poll等待响应
        struct pollfd fds;
        fds.fd = icmp_sockfd_;
        fds.events = POLLIN;

        auto start = std::chrono::steady_clock::now();
        int remaining_time = timeout_ms;

        while (remaining_time > 0) {
            int ret = poll(&fds, 1, remaining_time);
            if (ret > 0 && (fds.revents & POLLIN)) {
                uint8_t buffer[1024];
                struct sockaddr_in from;
                socklen_t fromlen = sizeof(from);
                ssize_t len = recvfrom(icmp_sockfd_, buffer, sizeof(buffer), 0,
                                       reinterpret_cast<struct sockaddr*>(&from), &fromlen);
                if (len < 0) {
                    continue;
                }

                // 解析IP头
                struct iphdr* iph = reinterpret_cast<struct iphdr*>(buffer);
                if (iph->protocol != IPPROTO_ICMP) continue;

                // 解析ICMP头
                struct icmphdr* icmph = reinterpret_cast<struct icmphdr*>(
                    buffer + (iph->ihl << 2));

                // 检查是否是回应应答
                if (icmph->type == ICMP_ECHOREPLY &&
                    icmph->un.echo.id == (getpid() & 0xFFFF)) {
                    return true;
                }
            } else if (ret == 0) {
                break; // 超时
            }

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            remaining_time = timeout_ms - elapsed.count();
        }

        return false;
    }

    uint16_t checksum(uint16_t* buf, int len) {
        uint32_t sum = 0;
        while (len > 1) {
            sum += *buf++;
            len -= 2;
        }
        if (len == 1) {
            sum += *(uint8_t*)buf;
        }
        sum = (sum >> 16) + (sum & 0xFFFF);
        sum += (sum >> 16);
        return ~sum;
    }

    std::string getDefaultInterface() {
        // 简化实现 - 实际项目中可能需要更复杂的逻辑来获取默认接口
        // return "eth0"; // 或 "en0" 在MacOS上
        std::string result = "eth0";

        int family, s;
        struct ifaddrs *ifaddr, *ifa;
        char host[NI_MAXHOST];

        if (getifaddrs(&ifaddr) == -1) {
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
                    std::cerr << "getnameinfo() failed: " << gai_strerror(s) << std::endl;
                    continue;
                }
                // Set the result with the interface name and IP address
                result = ifa->ifa_name;
                break; // Stop after finding the first non-loopback IPv4 address
            }
        }
        freeifaddrs(ifaddr);
        return result;
    }

    DetectionMethod method_;
    std::string interface_;

    // ARP相关
    int arp_sockfd_ = -1;
    int ifindex_ = 0;
    uint8_t src_mac_[6] = {0};
    bool arp_initialized_ = false;

    // ICMP相关
    int icmp_sockfd_ = -1;
    uint16_t seq_ = 0;
    bool icmp_initialized_ = false;
};

#endif /* _IPREPLY_HPP */