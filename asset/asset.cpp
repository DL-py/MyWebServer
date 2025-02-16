#include "asset.hpp"
#include "../global/global.hpp"

char* Asset::doc_root = (char* )"./html";
char* Asset::default_page = (char* )"welcome.html";


Asset::Asset()
{

}

Asset::~Asset()
{

}

bool Asset::initAsset()
{
    const Config& cfg = globalCFG.getConfig();

    /* get root ducument path from configuration. */
    if (cfg.exists("asset.doc_root"))
    {
        try
        {
            doc_root = const_cast<char*>((const char* )cfg.lookup("asset.doc_root"));
        }
        catch(const SettingTypeException& tex)
        {
            std::cerr<< "invalid configuration type: "<< tex.getPath() << std::endl;
        }   
    }

    /* get default page from configuration. */
    if (cfg.exists("asset.default_page"))
    {
        try
        {
            default_page = const_cast<char*>((const char* )cfg.lookup("asset.default_page"));
        }
        catch(const SettingTypeException& tex)
        {
            std::cerr<< "invalid configuration type: "<< tex.getPath() << std::endl;
        }           
    }

    return true;
}

void Asset::PrintAsset()
{
    std::cout << std::left << std::setw(16) << "Asset info: "<<std::endl;
    std::cout << "document root path: " << doc_root << std::endl;
    std::cout << "default document page: " << default_page << std::endl;
}
