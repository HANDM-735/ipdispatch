#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <string>

#define MESSAGE_HEADER_SERVER 0xFEEF
#define MESSAGE_HEADER_CLIENT 0xEFFE

// listen port
#define SERVER_PORT 11020
#define CLIENT_PORT 11019

// buffer size
#define BUFFER_SIZE 1024

enum ipcmd {
    DISPATCH_IP_BY_SERVER = 0,
    GET_SYNC_IP_BY_SERVER = 1,
    GET_FAN_MONITOR_IP_BY_SERVER = 2,
    GET_HOT_SIMULATE_BOARD_IP_BY_SERVER = 3,
    IP_CMD_MAX,
};

struct SocketRAII {
    int fd;
    SocketRAII(int domain, int type, int protocol) : fd(socket(domain, type, protocol)) {}
    ~SocketRAII() { if (fd != -1) close(fd); }
    operator int() const { return fd; }
};

struct IPAssignmentMessage {
    uint16_t header;
    uint16_t cmd;
    uint16_t slot_type;
    uint16_t slot_id;
    uint32_t ip_address;
    uint32_t netmask;
    uint32_t gw_address;

    // constructor function
    IPAssignmentMessage(uint16_t h, uint16_t c, uint16_t st, uint16_t si, uint32_t ip, uint32_t nm, uint32_t gw)
        : header(h), cmd(c), slot_type(st), slot_id(si), ip_address(ip), netmask(nm), gw_address(gw) {}

    // serialization
    void serialize(uint8_t* buffer) const {
        memcpy(buffer, &header, sizeof(header));
        memcpy(buffer + sizeof(header), &cmd, sizeof(cmd));
        memcpy(buffer + sizeof(header) + sizeof(cmd), &slot_type, sizeof(slot_type));
        memcpy(buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type), &slot_id, sizeof(slot_id));
        memcpy(buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type) + sizeof(slot_id), &ip_address, sizeof(ip_address));
        memcpy(buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type) + sizeof(slot_id) + sizeof(ip_address), &netmask, sizeof(netmask));
        memcpy(buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type) + sizeof(slot_id) + sizeof(ip_address) + sizeof(netmask), &gw_address, sizeof(gw_address));
    }

    // deserialization
    static IPAssignmentMessage deserialize(const uint8_t* buffer) {
        uint16_t header, cmd, slot_type, slot_id;
        uint32_t ip_address, netmask, gw_address;
        memcpy(&header, buffer, sizeof(header));
        memcpy(&cmd, buffer + sizeof(header), sizeof(cmd));
        memcpy(&slot_type, buffer + sizeof(header) + sizeof(cmd), sizeof(slot_type));
        memcpy(&slot_id, buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type), sizeof(slot_id));
        memcpy(&ip_address, buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type) + sizeof(slot_id), sizeof(ip_address));
        memcpy(&netmask, buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type) + sizeof(slot_id) + sizeof(ip_address), sizeof(netmask));
        memcpy(&gw_address, buffer + sizeof(header) + sizeof(cmd) + sizeof(slot_type) + sizeof(slot_id) + sizeof(ip_address) + sizeof(netmask), sizeof(gw_address));
        return IPAssignmentMessage(header, cmd, slot_type, slot_id, ip_address, netmask, gw_address);
    }
};

#endif // MESSAGE_H