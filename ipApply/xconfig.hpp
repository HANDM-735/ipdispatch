#ifndef XCONFIG_H
#define XCONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

class xini_config // INI配置类
{
public:
    xini_config() {}
public:
    bool set_file(std::string file_path)
    {
        m_file_path = file_path;
        try
        {
            m_tree_ini.clear();
            boost::property_tree::ini_parser::read_ini(m_file_path, m_tree_ini);
            return true;
        }
        catch (...) // 读取配置文件错误! 重新生成
        {
            printf("< cfg > get config file(%s) error!\n", m_file_path.c_str());
        }
        return false;
    }

    std::string get_data(std::string key, std::string def_value="") // "SYSTEM.gateway_id"
    {
        try
        {
            std::string value = m_tree_ini.get<std::string>(key);
            return value;
        }
        catch (...) {
            printf("< cfg > read config item(%s) error!\n", key.c_str());
        }
        return def_value;
    }

    int get_data(std::string key, int def_value=0)
    {
        try
        {
            std::string value = m_tree_ini.get<std::string>(key);
            return atoi(value.c_str());
        }
        catch (...) {
            printf("< cfg > read config item(%s) error!\n", key.c_str());
        }
        return def_value;
    }

    int set_data(std::string key, std::string value, bool save=true)
    {
        try
        {
            m_tree_ini.put<std::string>(key, value);
            if(save) boost::property_tree::ini_parser::write_ini(m_file_path, m_tree_ini);
        }
        catch (...) {
            printf("< cfg > write config item(%s) error!\n", key.c_str());
            return -1;
        }
        return 0;
    }

    int set_data(std::string key, int value, bool save=true)
    {
        try
        {
            m_tree_ini.put<int>(key, value);
            if(save) boost::property_tree::ini_parser::write_ini(m_file_path, m_tree_ini);
        }
        catch (...) {
            printf("< cfg > write config item(%s) error!\n", key.c_str());
            return -1;
        }
        return 0;
    }

    int save_data() // 保存数据到磁盘文件
    {
        try
        {
            boost::property_tree::ini_parser::write_ini(m_file_path, m_tree_ini);
        }
        catch (...) {
            printf("< cfg > write config file(%s) error!\n", m_file_path.c_str());
            return -1;
        }
        return 0;
    }

protected:
    std::string m_file_path;
    boost::property_tree::ptree m_tree_ini;
};

#endif // XCONFIG_H