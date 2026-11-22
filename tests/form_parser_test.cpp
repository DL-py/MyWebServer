#include "../auth/form_parser.h"
#include <iostream>

int main()
{
    std::unordered_map<std::string, std::string> out;
    if (!parseFormUrlEncoded("user=alice&password=a%20b&hobby=%E8%BF%90%E5%8A%A8&empty=", out))
    {
        std::cerr << "FAIL: valid form rejected\n";
        return 1;
    }
    if (out["user"] != "alice" || out["password"] != "a b" ||
        out["hobby"] != u8"运动" ||
        out.count("empty") != 1 || !out["empty"].empty())
    {
        std::cerr << "FAIL: decoded values\n";
        return 1;
    }

    std::unordered_map<std::string, std::string> bad;
    if (parseFormUrlEncoded("user=%ZZ", bad))
    {
        std::cerr << "FAIL: malformed percent accepted\n";
        return 1;
    }

    std::unordered_map<std::string, std::string> dup;
    parseFormUrlEncoded("a=1&a=2", dup);
    if (dup["a"] != "2")
    {
        std::cerr << "FAIL: duplicate key should keep last\n";
        return 1;
    }

    std::cout << "form parser test PASS\n";
    return 0;
}
