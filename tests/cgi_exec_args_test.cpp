#include "../handler/cgi_exec_args.h"

#include <cstring>
#include <iostream>
#include <string>

int main()
{
    CgiExecArgs args = buildCgiExecArgs("/srv/www/register.cgi", "POST", "42");

    if (args.argv.size() != 3 || args.argv[2] != NULL)
    {
        std::cerr << "FAIL: argv layout\n";
        return 1;
    }
    if (std::strcmp(args.argv[0], "/srv/www/register.cgi") != 0)
    {
        std::cerr << "FAIL: argv[0]\n";
        return 1;
    }
    if (std::strcmp(args.argv[1], "42") != 0)
    {
        std::cerr << "FAIL: argv[1]\n";
        return 1;
    }

    bool hasLength = false;
    bool hasMethod = false;
    bool hasRepeat = false;
    bool hasCorrect = false;
    bool terminated = false;

    for (size_t i = 0; i < args.envp.size(); ++i)
    {
        if (args.envp[i] == NULL)
        {
            terminated = (i + 1 == args.envp.size());
            break;
        }
        const std::string entry(args.envp[i]);
        hasLength = hasLength || entry == "CONTENT_LENGTH=42";
        hasMethod = hasMethod || entry == "REQUEST_METHOD=POST";
        hasRepeat = hasRepeat || entry == "Is_Repeat=0";
        hasCorrect = hasCorrect || entry == "IsCorrect=0";
    }

    if (!terminated || !hasLength || !hasMethod || !hasRepeat || !hasCorrect)
    {
        std::cerr << "FAIL: envp contents\n";
        return 1;
    }

    std::cout << "CGI exec args test PASS\n";
    return 0;
}
