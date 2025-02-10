#ifndef CONFIG_HPP
#define CONFIG_HPP
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
};


#endif