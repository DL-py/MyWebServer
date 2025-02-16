#include "config.hpp"

MyConfig::MyConfig()
{

}

MyConfig::~MyConfig()
{

}

int MyConfig::initConfig(const std::string& filename)
{
    try
    {
        cfg.readFile(filename);
    }
    catch(const FileIOException &fioex)
    {
        std::cerr << "I/O error while reading file." << std::endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
                    << " - " << pex.getError() << std::endl;
        return(EXIT_FAILURE);
    }

    return 0;
}

const Config& MyConfig::getConfig()
{
    return cfg;
}

bool MyConfig::getQuantityValue(const char* path, int& value)
{
    if (! cfg.exists(path))
    {
        return false;
    }

    const Setting& setting = cfg.lookup(path);

    if (setting.isNumber())
    {
        value = setting;
        return true;
    }
    else if (setting.isString())
    {
        if (! getValueFromQuantityString(setting, value))
        {
            return false;
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool MyConfig::getTimeValue(const char* path, int& value)
{
    if (! cfg.exists(path))
    {
        return false;
    }

    const Setting& setting = cfg.lookup(path);

    if (setting.isNumber())
    {
        value = setting;
        return true;
    }
    else if (setting.isString())
    {
        if (! getValueFromTimeString(setting, value))
        {
            return false;
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool MyConfig::getValueFromQuantityString(const char* QString, int& value)
{
    if (strlen(QString) == 0)
    {
        return false;
    }

    int base = 0, dimension = 0;
    char type;
    /* 100K */
    if (sscanf(QString, "%d%c", &base, &type) != 2)
    {
        return false;
    }

    switch(type)
    {
        case 'K':
        {
            dimension = 1024;
            break;
        }
        case 'M':
        {
            dimension = 1024 * 1024;
            break;
        }
        case 'G':
        {
           dimension = 1024 * 1024 * 1024;
           break;
        }
        default:
        {
            return false;
        }
    }

    if (base < 0 || base > INT32_MAX / dimension)
    {
        return false;
    }

    value = base * dimension;

    return true;
}

bool MyConfig::getValueFromTimeString(const char* QString, int& value)
{
    if (strlen(QString) == 0)
    {
        return false;
    }

    int base = 0, dimension = 0;
    char type;
    /* 100K */
    if (sscanf(QString, "%d%c", &base, &type) != 2)
    {
        return false;
    }

    switch(type)
    {
        case 's':
        {
            dimension = 1;
            break;
        }
        case 'm':
        {
            dimension = 60;
            break;
        }
        case 'h':
        {
           dimension = 60 * 60;
           break;
        }
        case 'd':
        {
            dimension = 24 * 60 * 60;
            break;
        }
        default:
        {
            return false;
        }
    }

    if (base < 0 || base > INT32_MAX / dimension)
    {
        return false;
    }

    value = base * dimension;

    return true;
}


