#ifndef NETCONFIG_H
#define NETCONFIG_H

#include <map>
#include <string>

std::map<std::string, std::string> parseIniFile(const std::string& filename);
void configureNetwork(const std::string& name, const std::string& ip, const std::string& mask, const std::string& gw);
int ipset_from_config(const std::string& filename);

#endif	// NETCONFIG_H