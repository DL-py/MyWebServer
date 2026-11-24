#include "asset.hpp"
#include "../global/global.hpp"

char* Asset::doc_root = (char* )"./html";
char* Asset::default_page = (char* )"welcome.html";
bool Asset::gzip_enabled = true;
int Asset::gzip_min_size = 1024;
int Asset::gzip_cache_size = 32 * 1024 * 1024;


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

    /* get gzip settings from configuration. */
    if (cfg.exists("asset.gzip"))
    {
        try
        {
            const Setting& setting = cfg.lookup("asset.gzip");
            if (setting.getType() == Setting::TypeBoolean)
            {
                gzip_enabled = (bool)setting;
            }
            else if (setting.getType() == Setting::TypeInt)
            {
                gzip_enabled = ((int)setting != 0);
            }
            else
            {
                std::cerr << "invalid configuration type: asset.gzip" << std::endl;
                return false;
            }
        }
        catch(const SettingTypeException& tex)
        {
            std::cerr<< "invalid configuration type: "<< tex.getPath() << std::endl;
        }
    }

    int quantity = 0;
    if (cfg.exists("asset.gzip_min_size"))
    {
        if (!globalCFG.getQuantityValue("asset.gzip_min_size", quantity))
        {
            std::cerr << "invalid value: asset.gzip_min_size (use e.g. \"1K\")" << std::endl;
        }
        else if (quantity < 0)
        {
            std::cerr << "invalid value: asset.gzip_min_size" << std::endl;
            return false;
        }
        else
        {
            gzip_min_size = quantity;
        }
    }
    if (cfg.exists("asset.gzip_cache_size"))
    {
        if (!globalCFG.getQuantityValue("asset.gzip_cache_size", quantity))
        {
            std::cerr << "invalid value: asset.gzip_cache_size (use e.g. \"32M\")" << std::endl;
        }
        else if (quantity < 0)
        {
            std::cerr << "invalid value: asset.gzip_cache_size" << std::endl;
            return false;
        }
        else
        {
            /* zero disables the compressed-body cache. */
            gzip_cache_size = quantity;
        }
    }

    return true;
}

void Asset::PrintAsset()
{
    std::cout << std::left << std::setw(16) << "Asset info: "<<std::endl;
    std::cout << "document root path: " << doc_root << std::endl;
    std::cout << "default document page: " << default_page << std::endl;
    std::cout << "gzip: " << (gzip_enabled ? "enabled" : "disabled") << std::endl;
    std::cout << "gzip min size: " << gzip_min_size << std::endl;
    std::cout << "gzip cache size: " << gzip_cache_size << std::endl;
}
