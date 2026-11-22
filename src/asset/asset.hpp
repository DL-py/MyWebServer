#ifndef ASSET_HPP
#define ASSET_HPP
#include <string>

class Asset
{
public:
    static char* doc_root;
    static char* default_page;


public:
    Asset();
    ~Asset();
    bool initAsset();
    void PrintAsset();
};

#endif