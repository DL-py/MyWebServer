#ifndef ASSET_HPP
#define ASSET_HPP
#include <string>

class Asset
{
public:
    static char* doc_root;
    static char* default_page;
    static bool gzip_enabled;
    static int gzip_min_size;
    static int gzip_cache_size;


public:
    Asset();
    ~Asset();
    bool initAsset();
    void PrintAsset();
};

#endif