#include "http_protocol.hpp"

#include <cstring>

#include <strings.h>

namespace
{
bool parseWithFormat(const std::string& value, const char* format, struct tm& tmv)
{
    memset(&tmv, 0, sizeof(tmv));
    const char* end = strptime(value.c_str(), format, &tmv);
    if (end == nullptr)
    {
        return false;
    }
    while (*end == ' ' || *end == '\t')
    {
        ++end;
    }
    return *end == '\0';
}
}  // namespace

std::string httpDate(std::time_t when)
{
    struct tm tmv;
    if (gmtime_r(&when, &tmv) == nullptr)
    {
        return "";
    }
    char buffer[64];
    if (strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tmv) == 0)
    {
        return "";
    }
    return buffer;
}

bool parseHttpDate(const std::string& value, std::time_t& out)
{
    static const char* const formats[] = {
        "%a, %d %b %Y %H:%M:%S GMT", /* IMF-fixdate */
        "%A, %d-%b-%y %H:%M:%S GMT", /* RFC 850 */
        "%a %b %e %H:%M:%S %Y"       /* asctime */
    };
    struct tm nowTm;
    const std::time_t now = std::time(nullptr);
    if (gmtime_r(&now, &nowTm) == nullptr)
    {
        memset(&nowTm, 0, sizeof(nowTm));
    }

    for (std::size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
    {
        struct tm tmv;
        if (!parseWithFormat(value, formats[i], tmv))
        {
            continue;
        }
        /* RFC 9110 5.6.7: two-digit years more than 50 years in the future
           belong to the previous century. Only RFC 850 has a two-digit year. */
        if (i == 1 && tmv.tm_year > nowTm.tm_year + 50)
        {
            tmv.tm_year -= 100;
        }
        out = timegm(&tmv);
        return true;
    }
    return false;
}

bool headerHasToken(const std::string& value, const std::string& token)
{
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t comma = value.find(',', start);
        std::string item = value.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start);
        while (!item.empty() && (item[0] == ' ' || item[0] == '\t'))
        {
            item.erase(item.begin());
        }
        while (!item.empty() &&
               (item[item.size() - 1] == ' ' || item[item.size() - 1] == '\t'))
        {
            item.erase(item.size() - 1);
        }
        if (strcasecmp(item.c_str(), token.c_str()) == 0)
        {
            return true;
        }
        if (comma == std::string::npos)
        {
            break;
        }
        start = comma + 1;
    }
    return false;
}
