#include "user_handler.h"

#include "../auth/form_parser.h"
#include "../auth/password_hash.h"
#include "../db/db_pool.h"
#include "../global/global.hpp"

#include <mysql.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace
{
using responseCode = HttpResponseCode::HTTP_RESPONSE_CODE;

const char* const SESSION_COOKIE = "session";

struct ConnGuard {
    explicit ConnGuard(MYSQL* c) : conn(c) {}
    ~ConnGuard() { if (conn) globalDbPool->release(conn); }
    MYSQL* conn;
private:
    ConnGuard(const ConnGuard&);
    ConnGuard& operator=(const ConnGuard&);
};

struct StmtGuard {
    explicit StmtGuard(MYSQL* c) : stmt(mysql_stmt_init(c)) {}
    ~StmtGuard() { if (stmt) mysql_stmt_close(stmt); }
    MYSQL_STMT* stmt;
private:
    StmtGuard(const StmtGuard&);
    StmtGuard& operator=(const StmtGuard&);
};

std::string htmlEscape(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (char c : in)
    {
        switch (c)
        {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string trim(const std::string& in)
{
    size_t b = 0;
    while (b < in.size() && (in[b] == ' ' || in[b] == '\t' || in[b] == '\r'))
    {
        ++b;
    }
    size_t e = in.size();
    while (e > b && (in[e - 1] == ' ' || in[e - 1] == '\t' || in[e - 1] == '\r'))
    {
        --e;
    }
    return in.substr(b, e - b);
}

std::string readCookie(HttpRequest& req, const std::string& name)
{
    std::string raw = req.getHeader("Cookie");
    if (raw.empty())
    {
        raw = req.getHeader("cookie");
    }
    size_t pos = 0;
    while (pos <= raw.size())
    {
        size_t semi = raw.find(';', pos);
        if (semi == std::string::npos)
        {
            semi = raw.size();
        }
        std::string part = trim(raw.substr(pos, semi - pos));
        size_t eq = part.find('=');
        if (eq != std::string::npos && part.substr(0, eq) == name)
        {
            return part.substr(eq + 1);
        }
        if (semi == raw.size())
        {
            break;
        }
        pos = semi + 1;
    }
    return "";
}

void setError(HttpResponse& resp, responseCode code, const std::string& message)
{
    resp.setStatusCodeAndMessage((int)code);
    resp.setHeader("content-type", "text/html; charset=utf-8");
    resp.setBody(
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<title>错误</title><link rel=\"stylesheet\" href=\"/styles.css\"></head>"
        "<body><main class=\"page\"><section class=\"card\">"
        "<h1 class=\"title\">操作失败</h1><p class=\"subtitle\">" + htmlEscape(message) + "</p>"
        "<p class=\"footer-link\"><a href=\"/login.html\">返回登录</a></p>"
        "</section></main></body></html>");
}

void setRedirect(HttpResponse& resp, const std::string& location)
{
    resp.setStatusCodeAndMessage((int)responseCode::MOVED_TEMPORARILY);
    resp.setHeader("Location", location);
    resp.setBody("");
}

bool isUserNameValid(const std::string& s)
{
    if (s.size() < 3 || s.size() > 32)
    {
        return false;
    }
    for (char c : s)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_'))
        {
            return false;
        }
    }
    return true;
}

bool isPasswordValid(const std::string& s)
{
    if (s.size() < 8)
    {
        return false;
    }
    bool upper = false;
    bool lower = false;
    bool special = false;
    for (char c : s)
    {
        if (c >= 'A' && c <= 'Z') upper = true;
        else if (c >= 'a' && c <= 'z') lower = true;
        else if (!((c >= '0' && c <= '9'))) special = true;
    }
    return upper && lower && special;
}

bool inSet(const std::string& value, const char* const* items, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (value == items[i])
        {
            return true;
        }
    }
    return false;
}

bool isHobbyValid(const std::string& s)
{
    static const char* const values[] = { "运动", "唱歌", "玩游戏", "学习" };
    return inSet(s, values, sizeof(values) / sizeof(values[0]));
}

bool isSexValid(const std::string& s)
{
    static const char* const values[] = { "boy", "girl" };
    return inSet(s, values, sizeof(values) / sizeof(values[0]));
}

bool isAddressValid(const std::string& s)
{
    static const char* const values[] = { "北京", "四川", "上海", "武汉" };
    return inSet(s, values, sizeof(values) / sizeof(values[0]));
}

void bindString(MYSQL_BIND& bind, const std::string& s)
{
    std::memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(s.data());
    bind.buffer_length = s.size();
}

void bindBuffer(MYSQL_BIND& bind, void* data, unsigned long size)
{
    std::memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = data;
    bind.buffer_length = size;
}

struct UserRow {
    unsigned long userId;
    char userName[64];
    unsigned long userNameLen;
    MySqlBool userNameNull;
    char hash[128];
    unsigned long hashLen;
    MySqlBool hashNull;
    char salt[64];
    unsigned long saltLen;
    MySqlBool saltNull;
    char algo[64];
    unsigned long algoLen;
    MySqlBool algoNull;
    unsigned long iterations;
    MySqlBool iterationsNull;
    char hobby[128];
    unsigned long hobbyLen;
    MySqlBool hobbyNull;
    char sex[32];
    unsigned long sexLen;
    MySqlBool sexNull;
    char introduction[2048];
    unsigned long introductionLen;
    MySqlBool introductionNull;
    char address[128];
    unsigned long addressLen;
    MySqlBool addressNull;
};

void bindUserRowResult(MYSQL_BIND* bind, UserRow& row)
{
    std::memset(bind, 0, sizeof(MYSQL_BIND) * 10);
    std::memset(&row, 0, sizeof(row));

    bind[0].buffer_type = MYSQL_TYPE_LONGLONG; bind[0].buffer = &row.userId; bind[0].is_unsigned = 1;
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = row.userName;
        bind[1].buffer_length = sizeof(row.userName); bind[1].length = &row.userNameLen; bind[1].is_null = &row.userNameNull;
    bind[2].buffer_type = MYSQL_TYPE_STRING; bind[2].buffer = row.hash;
        bind[2].buffer_length = sizeof(row.hash); bind[2].length = &row.hashLen; bind[2].is_null = &row.hashNull;
    bind[3].buffer_type = MYSQL_TYPE_STRING; bind[3].buffer = row.salt;
        bind[3].buffer_length = sizeof(row.salt); bind[3].length = &row.saltLen; bind[3].is_null = &row.saltNull;
    bind[4].buffer_type = MYSQL_TYPE_STRING; bind[4].buffer = row.algo;
        bind[4].buffer_length = sizeof(row.algo); bind[4].length = &row.algoLen; bind[4].is_null = &row.algoNull;
    bind[5].buffer_type = MYSQL_TYPE_LONG; bind[5].buffer = &row.iterations; bind[5].is_unsigned = 1;
        bind[5].is_null = &row.iterationsNull;
    bind[6].buffer_type = MYSQL_TYPE_STRING; bind[6].buffer = row.hobby;
        bind[6].buffer_length = sizeof(row.hobby); bind[6].length = &row.hobbyLen; bind[6].is_null = &row.hobbyNull;
    bind[7].buffer_type = MYSQL_TYPE_STRING; bind[7].buffer = row.sex;
        bind[7].buffer_length = sizeof(row.sex); bind[7].length = &row.sexLen; bind[7].is_null = &row.sexNull;
    bind[8].buffer_type = MYSQL_TYPE_STRING; bind[8].buffer = row.introduction;
        bind[8].buffer_length = sizeof(row.introduction); bind[8].length = &row.introductionLen; bind[8].is_null = &row.introductionNull;
    bind[9].buffer_type = MYSQL_TYPE_STRING; bind[9].buffer = row.address;
        bind[9].buffer_length = sizeof(row.address); bind[9].length = &row.addressLen; bind[9].is_null = &row.addressNull;
}

std::string field(const char* data, unsigned long len, MySqlBool isNull)
{
    if (isNull)
    {
        return "";
    }
    return std::string(data, len);
}

bool fetchUserBy(HttpResponse& resp, const char* sql, MYSQL_BIND& param,
                 UserRow& row, bool& found)
{
    found = false;
    if (!globalDbPool)
    {
        setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
        return false;
    }
    try
    {
        ConnGuard cg(globalDbPool->acquire());
        if (!cg.conn)
        {
            setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
            return false;
        }
        StmtGuard sg(cg.conn);
        if (!sg.stmt || mysql_stmt_prepare(sg.stmt, sql, strlen(sql)) != 0 ||
            mysql_stmt_bind_param(sg.stmt, &param) != 0 ||
            mysql_stmt_execute(sg.stmt) != 0)
        {
            setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
            return false;
        }
        MYSQL_BIND result[10];
        bindUserRowResult(result, row);
        if (mysql_stmt_bind_result(sg.stmt, result) != 0 ||
            mysql_stmt_store_result(sg.stmt) != 0)
        {
            setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
            return false;
        }
        int rc = mysql_stmt_fetch(sg.stmt);
        if (rc == 0)
        {
            found = true;
        }
        else if (rc != MYSQL_NO_DATA)
        {
            setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
            return false;
        }
        return true;
    }
    catch (const std::exception&)
    {
        setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
        return false;
    }
}

} // namespace

http_user_handler::http_user_handler() {}
http_user_handler::~http_user_handler() {}

bool http_user_handler::handler_pre(HttpRequest& req, HttpResponse& resp)
{
    (void)req;
    (void)resp;
    return true;
}

bool http_user_handler::handler_post(HttpRequest& req, HttpResponse& resp)
{
    (void)req;
    (void)resp;
    return true;
}

bool http_user_handler::handler(HttpRequest& req, HttpResponse& resp)
{
    std::string url = req.getURL();
    size_t q = url.find('?');
    if (q != std::string::npos)
    {
        url.erase(q);
    }
    const std::string method = req.getMethod();

    if (url == "/api/auth/register")
    {
        if (method != "POST") { setError(resp, responseCode::METHOD_NOT_ALLOWED, "方法不允许"); return true; }
        return handleRegister(req, resp);
    }
    if (url == "/api/auth/login")
    {
        if (method != "POST") { setError(resp, responseCode::METHOD_NOT_ALLOWED, "方法不允许"); return true; }
        return handleLogin(req, resp);
    }
    if (url == "/api/auth/logout")
    {
        if (method != "POST") { setError(resp, responseCode::METHOD_NOT_ALLOWED, "方法不允许"); return true; }
        return handleLogout(req, resp);
    }
    if (url == "/api/auth/me")
    {
        if (method != "GET") { setError(resp, responseCode::METHOD_NOT_ALLOWED, "方法不允许"); return true; }
        return handleMe(req, resp);
    }
    setError(resp, responseCode::NO_RESOURCE, "路径不存在");
    return true;
}

bool http_user_handler::handleRegister(HttpRequest& req, HttpResponse& resp)
{
    std::unordered_map<std::string, std::string> form;
    if (!parseFormUrlEncoded(req.getBody(), form))
    {
        setError(resp, responseCode::BAD_REQUEST, "表单格式错误");
        return true;
    }
    const std::string user = form["user"];
    const std::string password = form["password"];
    const std::string confirm = form["confirm"];
    const std::string hobby = form["hobby"];
    const std::string sex = form["sex"];
    const std::string introduction = form.count("introduction") ? form["introduction"] : "";
    const std::string address = form["address"];

    if (!isUserNameValid(user) || !isPasswordValid(password) || confirm != password ||
        !isHobbyValid(hobby) || !isSexValid(sex) || !isAddressValid(address) ||
        introduction.size() > 1024)
    {
        setError(resp, responseCode::BAD_REQUEST, "注册信息不合法");
        return true;
    }

    PasswordHash hashed;
    try
    {
        hashed = hashPassword(password);
    }
    catch (const std::exception&)
    {
        setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
        return true;
    }

    if (!globalDbPool)
    {
        setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
        return true;
    }
    try
    {
        ConnGuard cg(globalDbPool->acquire());
        if (!cg.conn)
        {
            setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
            return true;
        }
        const char* sql =
            "INSERT INTO user_basic_info "
            "(user_name, hashed_password, salt, hash_algo, iterations, "
            "hobby, sex, introduction, address) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        StmtGuard sg(cg.conn);
        MYSQL_BIND bind[9];
        std::memset(bind, 0, sizeof(bind));
        bindString(bind[0], user);
        bindBuffer(bind[1], hashed.hash.data(), hashed.hash.size());
        bindBuffer(bind[2], hashed.salt.data(), hashed.salt.size());
        const std::string algo = hashed.algo;
        bindString(bind[3], algo);
        unsigned long iterations = hashed.iterations;
        bind[4].buffer_type = MYSQL_TYPE_LONG;
        bind[4].buffer = &iterations;
        bind[4].is_unsigned = 1;
        bindString(bind[5], hobby);
        bindString(bind[6], sex);
        bindString(bind[7], introduction);
        bindString(bind[8], address);

        if (!sg.stmt || mysql_stmt_prepare(sg.stmt, sql, strlen(sql)) != 0 ||
            mysql_stmt_bind_param(sg.stmt, bind) != 0 ||
            mysql_stmt_execute(sg.stmt) != 0)
        {
            if (sg.stmt && mysql_stmt_errno(sg.stmt) == 1062)
            {
                setError(resp, responseCode::CONFLICT, "用户名已存在");
            }
            else
            {
                setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
            }
            return true;
        }
        unsigned long long userId = mysql_stmt_insert_id(sg.stmt);
        std::string token = globalSessionStore.create((int)userId);
        resp.setHeader("Set-Cookie", "session=" + token + "; HttpOnly; SameSite=Lax; Path=/");
        setRedirect(resp, "/api/auth/me");
        return true;
    }
    catch (const std::exception&)
    {
        setError(resp, responseCode::INTERNAL_ERROR, "服务器内部错误");
        return true;
    }
}

bool http_user_handler::handleLogin(HttpRequest& req, HttpResponse& resp)
{
    std::unordered_map<std::string, std::string> form;
    if (!parseFormUrlEncoded(req.getBody(), form))
    {
        setError(resp, responseCode::BAD_REQUEST, "表单格式错误");
        return true;
    }
    const std::string user = form["user"];
    const std::string password = form["password"];
    if (user.empty() || password.empty())
    {
        setError(resp, responseCode::BAD_REQUEST, "请输入账号和密码");
        return true;
    }
    if (!isUserNameValid(user))
    {
        setError(resp, responseCode::UNAUTHORIZED, "用户不存在或密码错误");
        return true;
    }

    const char* sql =
        "SELECT user_id, user_name, hashed_password, salt, hash_algo, iterations, "
        "hobby, sex, introduction, address FROM user_basic_info WHERE user_name = ?";
    MYSQL_BIND param;
    std::memset(&param, 0, sizeof(param));
    bindString(param, user);
    UserRow row;
    bool found = false;
    if (!fetchUserBy(resp, sql, param, row, found))
    {
        return true;
    }
    if (!found)
    {
        setError(resp, responseCode::UNAUTHORIZED, "用户不存在或密码错误");
        return true;
    }

    PasswordHash stored;
    stored.salt.assign(reinterpret_cast<unsigned char*>(row.salt),
                       reinterpret_cast<unsigned char*>(row.salt) + row.saltLen);
    stored.hash.assign(reinterpret_cast<unsigned char*>(row.hash),
                       reinterpret_cast<unsigned char*>(row.hash) + row.hashLen);
    stored.iterations = (int)row.iterations;
    stored.algo = field(row.algo, row.algoLen, row.algoNull);
    if (!verifyPassword(password, stored))
    {
        setError(resp, responseCode::UNAUTHORIZED, "用户不存在或密码错误");
        return true;
    }
    std::string token = globalSessionStore.create((int)row.userId);
    resp.setHeader("Set-Cookie", "session=" + token + "; HttpOnly; SameSite=Lax; Path=/");
    setRedirect(resp, "/api/auth/me");
    return true;
}

bool http_user_handler::handleLogout(HttpRequest& req, HttpResponse& resp)
{
    std::string token = readCookie(req, SESSION_COOKIE);
    if (!token.empty())
    {
        globalSessionStore.remove(token);
    }
    resp.setHeader("Set-Cookie",
                   "session=; Max-Age=0; HttpOnly; SameSite=Lax; Path=/");
    setRedirect(resp, "/welcome.html");
    return true;
}

bool http_user_handler::handleMe(HttpRequest& req, HttpResponse& resp)
{
    std::string token = readCookie(req, SESSION_COOKIE);
    Session session;
    if (token.empty() || !globalSessionStore.get(token, session))
    {
        setError(resp, responseCode::UNAUTHORIZED, "请先登录");
        return true;
    }

    const char* sql =
        "SELECT user_id, user_name, hashed_password, salt, hash_algo, iterations, "
        "hobby, sex, introduction, address FROM user_basic_info WHERE user_id = ?";
    MYSQL_BIND param;
    std::memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    unsigned long long userId = session.userId;
    param.buffer = &userId;
    param.is_unsigned = 1;
    UserRow row;
    bool found = false;
    if (!fetchUserBy(resp, sql, param, row, found))
    {
        return true;
    }
    if (!found)
    {
        setError(resp, responseCode::UNAUTHORIZED, "请先登录");
        return true;
    }

    std::string user = htmlEscape(field(row.userName, row.userNameLen, row.userNameNull));
    std::string hobby = htmlEscape(field(row.hobby, row.hobbyLen, row.hobbyNull));
    std::string sex = htmlEscape(field(row.sex, row.sexLen, row.sexNull));
    std::string intro = htmlEscape(field(row.introduction, row.introductionLen, row.introductionNull));
    std::string address = htmlEscape(field(row.address, row.addressLen, row.addressNull));

    resp.setStatusCodeAndMessage((int)responseCode::GOOD_REQUEST);
    resp.setHeader("content-type", "text/html; charset=utf-8");
    resp.setBody(
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<title>个人资料</title><link rel=\"stylesheet\" href=\"/styles.css\"></head>"
        "<body><main class=\"page\"><section class=\"card\">"
        "<h1 class=\"title\">你好，" + user + "</h1>"
        "<p class=\"subtitle\">爱好：" + hobby + "</p>"
        "<p class=\"subtitle\">性别：" + sex + "</p>"
        "<p class=\"subtitle\">自我介绍：" + intro + "</p>"
        "<p class=\"subtitle\">地址：" + address + "</p>"
        "<form action=\"/api/auth/logout\" method=\"POST\">"
        "<button class=\"btn btn--primary\" type=\"submit\">注销</button>"
        "</form>"
        "</section></main></body></html>");
    return true;
}
