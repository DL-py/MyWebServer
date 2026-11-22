#include "form_parser.h"

namespace
{

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

bool decodeComponent(const std::string& in, std::string& out)
{
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        char c = in[i];
        if (c == '+')
        {
            out.push_back(' ');
        }
        else if (c == '%')
        {
            if (i + 2 >= in.size())
            {
                return false;
            }
            int high = hexValue(in[i + 1]);
            int low = hexValue(in[i + 2]);
            if (high < 0 || low < 0)
            {
                return false;
            }
            out.push_back(static_cast<char>((high << 4) | low));
            i += 2;
        }
        else
        {
            out.push_back(c);
        }
    }
    return true;
}

} // namespace

bool parseFormUrlEncoded(const std::string& body,
                         std::unordered_map<std::string, std::string>& out)
{
    std::size_t pos = 0;
    while (pos <= body.size())
    {
        std::size_t amp = body.find('&', pos);
        if (amp == std::string::npos)
        {
            amp = body.size();
        }
        if (amp > pos)
        {
            std::string segment = body.substr(pos, amp - pos);
            std::size_t eq = segment.find('=');
            std::string rawKey = (eq == std::string::npos) ? segment : segment.substr(0, eq);
            std::string rawValue = (eq == std::string::npos) ? std::string() : segment.substr(eq + 1);

            std::string key;
            std::string value;
            if (!decodeComponent(rawKey, key) || !decodeComponent(rawValue, value))
            {
                return false;
            }
            out[key] = value;
        }
        if (amp == body.size())
        {
            break;
        }
        pos = amp + 1;
    }
    return true;
}
