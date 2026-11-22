#include "handler_cgi.h"
#include "cgi_exec_args.h"

#include "../global/global.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <vector>

namespace
{
const int CGI_TIMEOUT_MS = 5000;
const size_t CGI_MAX_OUTPUT = 1024 * 1024;
const size_t IO_CHUNK = 4096;

void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

std::string trim(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() &&
           (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r'))
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
    {
        --end;
    }

    return value.substr(begin, end - begin);
}
}  // namespace

http_handler_cgi::http_handler_cgi()
{
}

http_handler_cgi::~http_handler_cgi()
{
}

bool http_handler_cgi::buildScriptPath(HttpRequest& req, HttpResponse& resp)
{
    using responseCode = HttpResponseCode::HTTP_RESPONSE_CODE;

    std::string url = req.getURL();
    size_t queryPos = url.find('?');
    if (queryPos != std::string::npos)
    {
        url.erase(queryPos);
    }

    if (url.size() < 5 || url.compare(url.size() - 4, 4, ".cgi") != 0)
    {
        resp.setStatusCodeAndMessage((int)responseCode::BAD_REQUEST);
        return false;
    }

    char* docRoot = realpath(globalAsset.doc_root, NULL);
    if (docRoot == NULL)
    {
        resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        return false;
    }
    m_docRoot = docRoot;
    free(docRoot);

    std::string candidate = std::string(globalAsset.doc_root) + url;
    char* resolved = realpath(candidate.c_str(), NULL);
    if (resolved == NULL)
    {
        resp.setStatusCodeAndMessage((int)responseCode::NO_RESOURCE);
        return false;
    }
    m_scriptPath = resolved;
    free(resolved);

    const std::string prefix = m_docRoot + "/";
    if (m_scriptPath.compare(0, prefix.size(), prefix) != 0)
    {
        resp.setStatusCodeAndMessage((int)responseCode::FORBIDDEN_REQUEST);
        return false;
    }

    struct stat fileStat;
    if (stat(m_scriptPath.c_str(), &fileStat) < 0)
    {
        resp.setStatusCodeAndMessage((int)responseCode::NO_RESOURCE);
        return false;
    }

    if (!S_ISREG(fileStat.st_mode))
    {
        resp.setStatusCodeAndMessage((int)responseCode::BAD_REQUEST);
        return false;
    }

    if (!(fileStat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
    {
        resp.setStatusCodeAndMessage((int)responseCode::FORBIDDEN_REQUEST);
        return false;
    }

    return true;
}

bool http_handler_cgi::handler_pre(HttpRequest& req, HttpResponse& resp)
{
    return buildScriptPath(req, resp);
}

bool http_handler_cgi::execute(HttpRequest& req, HttpResponse& resp, std::string& output)
{
    using responseCode = HttpResponseCode::HTTP_RESPONSE_CODE;

    const std::string body = req.getBody();
    const std::string method = req.getMethod();
    const std::string contentLength = std::to_string(body.size());

    int pipeParentToChild[2];
    int pipeChildToParent[2];

    if (pipe(pipeParentToChild) < 0)
    {
        resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        return false;
    }

    if (pipe(pipeChildToParent) < 0)
    {
        close(pipeParentToChild[0]);
        close(pipeParentToChild[1]);
        resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        return false;
    }

    setNonBlocking(pipeParentToChild[1]);
    setNonBlocking(pipeChildToParent[0]);

    // All heap allocation happens before fork(). The child only performs
    // async-signal-safe calls before execve().
    CgiExecArgs execArgs = buildCgiExecArgs(m_scriptPath, method, contentLength);

    pid_t childPid = fork();
    if (childPid < 0)
    {
        close(pipeParentToChild[0]);
        close(pipeParentToChild[1]);
        close(pipeChildToParent[0]);
        close(pipeChildToParent[1]);
        resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        return false;
    }

    if (childPid == 0)
    {
        close(pipeParentToChild[1]);
        close(pipeChildToParent[0]);

        dup2(pipeParentToChild[0], STDIN_FILENO);
        dup2(pipeChildToParent[1], STDOUT_FILENO);

        close(pipeParentToChild[0]);
        close(pipeChildToParent[1]);

        execve(m_scriptPath.c_str(), &execArgs.argv[0], &execArgs.envp[0]);
        _exit(127);
    }

    close(pipeParentToChild[0]);
    close(pipeChildToParent[1]);

    size_t written = 0;
    bool bodyDone = body.empty();
    if (bodyDone)
    {
        close(pipeParentToChild[1]);
    }

    bool outputDone = false;
    bool timedOut = false;
    bool tooLarge = false;
    std::string result;

    while (!outputDone)
    {
        struct pollfd fds[2];
        int bodyIndex = -1;
        int outputIndex = -1;
        int nfds = 0;

        if (!bodyDone)
        {
            fds[nfds].fd = pipeParentToChild[1];
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;
            bodyIndex = nfds++;
        }

        fds[nfds].fd = pipeChildToParent[0];
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        outputIndex = nfds++;

        int ready = poll(fds, nfds, CGI_TIMEOUT_MS);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            timedOut = true;
            break;
        }
        if (ready == 0)
        {
            timedOut = true;
            break;
        }

        if (bodyIndex >= 0 && (fds[bodyIndex].revents & (POLLOUT | POLLERR | POLLHUP)))
        {
            size_t remaining = body.size() - written;
            size_t chunk = remaining < IO_CHUNK ? remaining : IO_CHUNK;
            ssize_t bytesWritten = write(pipeParentToChild[1], body.data() + written, chunk);
            if (bytesWritten > 0)
            {
                written += static_cast<size_t>(bytesWritten);
            }
            else if (bytesWritten < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                bodyDone = true;
                close(pipeParentToChild[1]);
            }

            if (written >= body.size() && !bodyDone)
            {
                bodyDone = true;
                close(pipeParentToChild[1]);
            }
        }

        if (fds[outputIndex].revents & (POLLIN | POLLERR | POLLHUP))
        {
            char buffer[IO_CHUNK];
            ssize_t bytesRead = read(pipeChildToParent[0], buffer, sizeof(buffer));
            if (bytesRead > 0)
            {
                if (result.size() + static_cast<size_t>(bytesRead) > CGI_MAX_OUTPUT)
                {
                    tooLarge = true;
                    break;
                }
                result.append(buffer, static_cast<size_t>(bytesRead));
            }
            else if (bytesRead == 0)
            {
                outputDone = true;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                outputDone = true;
            }
        }
    }

    if (!bodyDone)
    {
        close(pipeParentToChild[1]);
    }
    close(pipeChildToParent[0]);

    if (timedOut || tooLarge)
    {
        kill(childPid, SIGKILL);
    }

    int status = 0;
    while (waitpid(childPid, &status, 0) < 0 && errno == EINTR)
    {
    }

    if (timedOut || tooLarge)
    {
        resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        return false;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        return false;
    }

    output = result;
    return true;
}

void http_handler_cgi::parseCgiOutput(const std::string& output, HttpResponse& resp)
{
    size_t headerEnd = output.find("\r\n\r\n");
    size_t separatorLength = 4;
    if (headerEnd == std::string::npos)
    {
        headerEnd = output.find("\n\n");
        separatorLength = 2;
    }

    bool hasHeaders = false;
    if (headerEnd != std::string::npos)
    {
        const std::string headerBlock = output.substr(0, headerEnd);
        if (headerBlock.find(':') != std::string::npos && headerBlock.find('<') == std::string::npos)
        {
            hasHeaders = true;
        }
    }

    size_t bodyStart = 0;
    if (hasHeaders)
    {
        const std::string headerBlock = output.substr(0, headerEnd);
        size_t lineStart = 0;
        while (lineStart <= headerBlock.size())
        {
            size_t lineEnd = headerBlock.find('\n', lineStart);
            if (lineEnd == std::string::npos)
            {
                lineEnd = headerBlock.size();
            }

            std::string line = headerBlock.substr(lineStart, lineEnd - lineStart);
            size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                std::string name = trim(line.substr(0, colon));
                std::string value = trim(line.substr(colon + 1));
                if (strcasecmp(name.c_str(), "Content-Type") == 0 && !value.empty())
                {
                    resp.setHeader("Content-Type", value);
                }
            }

            if (lineEnd == headerBlock.size())
            {
                break;
            }
            lineStart = lineEnd + 1;
        }
        bodyStart = headerEnd + separatorLength;
    }

    resp.setBody(output.substr(bodyStart));
    if (resp.getHeader("Content-Type").empty())
    {
        resp.setHeader("Content-Type", "text/html; charset=utf-8");
    }
}

bool http_handler_cgi::handler(HttpRequest& req, HttpResponse& resp)
{
    using responseCode = HttpResponseCode::HTTP_RESPONSE_CODE;

    std::string output;
    if (!execute(req, resp, output))
    {
        if (resp.getStatusCode() != (int)responseCode::INTERNAL_ERROR)
        {
            resp.setStatusCodeAndMessage((int)responseCode::INTERNAL_ERROR);
        }
        return false;
    }

    parseCgiOutput(output, resp);
    resp.setStatusCodeAndMessage((int)responseCode::GOOD_REQUEST);
    return true;
}

bool http_handler_cgi::handler_post(HttpRequest& req, HttpResponse& resp)
{
    (void)req;
    (void)resp;
    return true;
}
