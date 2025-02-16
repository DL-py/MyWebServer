#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <cstring>
#include <string>
#include <iostream>
#include <libconfig.h++>

using namespace libconfig;

class MyConfig
{
private:
    Config cfg;

public:
    MyConfig();
    ~MyConfig();
    int initConfig(const std::string& filename = "common.cfg");
    const Config& getConfig();
    bool getQuantityValue(const char* path, int& value);
    bool getTimeValue(const char* path, int& value);
private:
    bool getValueFromQuantityString(const char* QString, int& value);
    bool getValueFromTimeString(const char* QString, int& value);
};


#endif