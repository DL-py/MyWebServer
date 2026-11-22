#include "cgi_exec_args.h"

namespace
{
size_t appendOwned(CgiExecArgs& args, const std::string& value)
{
    args.storage.push_back(std::vector<char>(value.begin(), value.end()));
    args.storage.back().push_back('\0');
    return args.storage.size() - 1;
}
}  // namespace

CgiExecArgs buildCgiExecArgs(const std::string& script,
                             const std::string& method,
                             const std::string& contentLength)
{
    CgiExecArgs args;

    appendOwned(args, "CONTENT_LENGTH=" + contentLength);
    appendOwned(args, "REQUEST_METHOD=" + method);
    appendOwned(args, "Is_Repeat=0");
    appendOwned(args, "IsCorrect=0");
    appendOwned(args, "LANG=C.UTF-8");
    appendOwned(args, "PYTHONIOENCODING=utf-8");
    const size_t envCount = args.storage.size();

    const size_t scriptIndex = appendOwned(args, script);
    const size_t lengthIndex = appendOwned(args, contentLength);

    for (size_t i = 0; i < envCount; ++i)
    {
        args.envp.push_back(args.storage[i].data());
    }
    args.envp.push_back(NULL);

    args.argv.push_back(args.storage[scriptIndex].data());
    args.argv.push_back(args.storage[lengthIndex].data());
    args.argv.push_back(NULL);

    return args;
}
