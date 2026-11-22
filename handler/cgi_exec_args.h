#ifndef MYWEBSERVER_CGI_EXEC_ARGS_H
#define MYWEBSERVER_CGI_EXEC_ARGS_H

#include <string>
#include <vector>

// Builds the argv/envp arrays for execve before fork() is called.
// Each string owns its own heap buffer so the char* pointers stay valid
// when this struct is moved across the fork boundary.
struct CgiExecArgs
{
    std::vector<std::vector<char> > storage;
    std::vector<char*> envp;
    std::vector<char*> argv;
};

CgiExecArgs buildCgiExecArgs(const std::string& script,
                             const std::string& method,
                             const std::string& contentLength);

#endif
