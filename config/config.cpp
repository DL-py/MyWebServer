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


