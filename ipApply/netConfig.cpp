#include "netconfig.h"

#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#ifndef _WINDOWS
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#endif

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

static std::string getFirstNonLoopbackIPv4() {
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
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
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

std::map<std::string, std::string> parseIniFile(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    while (getline(file, line)) {
        if (line.empty() || line[0] == '[') continue;
        std::remove_if(line.begin(), line.end(), ::isspace, line.end());
        std::vector<std::string> result_parts = split_single_char_delim(line, '=');
        if (result_parts.size() != 2) {
            throw std::invalid_argument("Invalid format in IP config file");
        } else {
            config[result_parts[0]] = result_parts[1];
        }
    }

    return config;
}

void configureNetwork(const std::string& name, const std::string& ip, const std::string& mask, const std::string& gw) {
    std::stringstream cmd;
    cmd << "ifconfig " << name << " " << ip << " netmask " << mask << " up";
    printf("Executing command: %s\n", cmd.str().c_str());
    system(cmd.str().c_str());

    std::stringstream cmd1;
    cmd1 << "route add default gw " << gw << " dev " << name;
    printf("Executing command: %s\n", cmd1.str().c_str());
    system(cmd1.str().c_str());
}

int ipset_from_config(const std::string& filename) {
    try {
        auto config = parseIniFile(filename);
        auto name = config["name"];
        auto ip = config["ip"];
        auto mask = config["netmask"];
        auto gw = config["gateway"];
        auto ntpserveraddr = config["ntpserveraddr"];

        if (name.empty() || ip.empty() || mask.empty() || gw.empty()) {
            printf("Error: Missing configuration values.\n");
            return -1;
        }
        printf("name:%s ip:%s mask:%s gw:%s\n", name.c_str(), ip.c_str(), mask.c_str(), gw.c_str());

        //if (!ntpserveraddr.empty()) {
        //    std::string command = "ntpdate " + ntpserveraddr;
        //    int result = system(command.c_str());
        //    if (result != 0) {
        //        printf("Failed to set ntpdate\n");
        //    } else {
        //        printf("ntpdate succeeded\n");
        //    }
        //}

        std::string client_ip = getFirstNonLoopbackIPv4();
        if (ip != "0.0.0.0" && ip != client_ip) {
            configureNetwork(name, ip, mask, gw);
        } else {
            printf("No need config!\n");
        }
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return -1;
    }
    return 0;
}