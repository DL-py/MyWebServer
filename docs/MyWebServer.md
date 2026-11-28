---
title: "MyWebServer 总体设计说明"
description: "基于 C/C++ 实现的轻量级 HTTP 服务器架构设计文档，涵盖 Epoll 并发模型、HTTP 消息层、连接管理、路由分发、协商缓存与 gzip 压缩等模块的详细设计。"
date: 2025-01-01
author: "MyWebServer Team"
tags: ["C++", "HTTP Server", "Epoll", "多线程", "架构设计"]
---

# MyWebServer 设计说明书

---

## 一、项目概述

MyWebServer 是一个基于 **C/C++** 实现的轻量级 HTTP 服务器，旨在构建个人博客网站，支持用户浏览文章、发表评论等交互功能。服务器采用 **Epoll I/O 多路复用 + 线程池** 的并发模型，能够在单机环境下高效处理大量并发连接。

### 核心特性

| 特性 | 说明 |
|------|------|
| I/O 模型 | Epoll + 边沿触发 (ET) + EPOLLONESHOT + 非阻塞 I/O |
| 并发模型 | 半同步/半异步线程池（Leader/Worker 模式） |
| HTTP 协议 | HTTP/1.1 GET/POST/HEAD；协商缓存（ETag/Last-Modified → 304）；Connection: keep-alive |
| 配置管理 | 基于 libconfig 的结构化配置 |
| 路由管理 | 基于请求方法的分发 |
| 静态资源 | HTML/CSS/JS/图片等文件服务（mmap 零拷贝），目录 301、点路径 403、404 页面 |
| 缓存与压缩 | 强 ETag（内容哈希）、条件请求 304、运行时 gzip（可配置阈值与缓存） |
| CGI 扩展 | 支持 Python CGI 脚本（已实现） |
| 用户管理 | MySQL 数据库 + SHA-256 加盐哈希（设计阶段） |
| 日志系统 | 多级别运行时日志与逐请求 ACCESS 行 |

---

## 二、总体架构设计

### 2.1 架构分层图

```
┌─────────────────────────────────────────────────────────────────┐
│                         主事件循环 (main)                        │
│                Epoll 事件监听 → 连接分配 → 线程池调度              │
├─────────────────────────────────────────────────────────────────┤
│                      HTTP 连接层 (HttpConn)                      │
│         读缓冲 → 请求解析 → 路由分发 → 业务处理 → 响应构建 → 写缓冲    │
├───────────────┬───────────────┬───────────────┬─────────────────┤
│  HTTP 消息层   │   路由管理层   │  业务处理层    │   基础设施层      │
│───────────────│───────────────│───────────────│─────────────────│
│ HttpRequest   │   Router      │ http_handler   │ Config (libconfig)│
│ HttpResponse  │   (方法分发)    │ handler_simple │ Logger (运行日志)  │
│ RequestParser │               │ handler_cgi*    │ Asset (静态资源)   │
│ ResponseParser│               │ user_handler*   │ ThreadPool (线程)  │
│ RequestMethod │               │                │ Locker (同步原语)  │
│ ResponseCode  │               │                │ Global (全局实例)  │
└───────────────┴───────────────┴───────────────┴─────────────────┘
```

> *标注 \* 表示在设计阶段、尚未完全实现的模块。*

### 2.2 请求处理流程图

```
浏览器/客户端
     │
     ▼
┌──────────┐   ┌──────────────┐   ┌────────────────┐   ┌──────────────────┐
│  Epoll   │──▶│ HttpConn     │──▶│ HttpRequest    │──▶│ Router::route()  │
│ 事件循环  │   │ ::bufferRead │   │ Parser::parse  │   │ 方法+URL 分发     │
└──────────┘   └──────────────┘   └────────────────┘   └────────┬─────────┘
     ▲                                                           │
     │                                                    ┌──────▼──────┐
     │                                                    │ http_handler │
     │                                                    │ handler_pre  │
     │                                                    │ handler      │
     │                                                    │ handler_post │
     │                                                    └──────┬──────┘
     │                                                           │
┌─────┴──────┐   ┌──────────────┐   ┌────────────────┐          │
│ HttpConn   │◀──│ HttpConn     │◀──│ HttpResponse   │◀─────────┘
│ ::buffer   │   │ ::process    │   │ Parser::parse  │
│ Write      │   │ (响应构建)    │   │ (构建响应报文)   │
└────────────┘   └──────────────┘   └────────────────┘
     │
     ▼
浏览器/客户端 收到 HTTP 响应
```

### 2.3 Epoll 事件注册过程

服务器启动后的事件注册和流转：

```
1. 创建 listenfd → 注册 EPOLLIN（非 ONESHOT）
2. 新连接到达 → accept → 创建 connfd → 注册 EPOLLIN | EPOLLONESHOT
3. 可读事件 → bufferRead() 一次性读完 → 提交线程池
4. 线程处理 → process() 完成后 → modfd 注册 EPOLLOUT
5. 可写事件 → bufferWrite() 写入 socket → resetConn() 重新注册 EPOLLIN
```

**关键设计原则：**
- 单次触发模式 (EPOLLONESHOT)：每次事件到达并处理后，需要重新注册
- 一次完整读取：不出现"读一部分-处理一部分-再读一部分"的情况
- 边沿触发 (ET)：只在内核缓冲区状态变化时通知

---

## 三、模块详细设计

### 3.1 HTTP 消息层 (http_messages/)

负责定义 HTTP 协议的数据结构，是整个系统的数据核心。

#### 3.1.1 HttpRequest — HTTP 请求

```cpp
class HttpRequest {
    string method_;                           // GET/POST/...
    string url_;                              // 请求路径
    string version_;                          // HTTP/1.1
    unordered_map<string, string> headers_;   // 请求头部
    string body_;                             // 请求体
};
```

**成员方法：**
| 方法 | 说明 |
|------|------|
| `getMethod() / setMethod()` | 获取/设置请求方法 |
| `getURL() / setURL()` | 获取/设置请求 URL |
| `getVersion() / setVersion()` | 获取/设置 HTTP 版本 |
| `getHeader() / setHeader()` | 读取/设置请求头字段；字段名大小写不敏感（RFC 9110 §5.1） |
| `getBody() / setBody()` | 获取/设置请求体 |

同名头部默认首值生效；`Accept-Encoding` 按 RFC 9110 §5.3 以逗号合并（该字段允许重复出现）。

#### 3.1.2 HttpResponse — HTTP 响应

```cpp
class HttpResponse {
    int statusCode_;                          // 状态码（200/301/304/404/503/...）
    string statusMessage_;                    // 状态消息
    unordered_map<string, string> headers_;   // 响应头部
    string body_;                             // 响应体
};
```

**额外方法：** `setStatusCodeAndMessage()` — 通过枚举值自动设置状态码和消息。

`setHeader()` 采用**覆盖写**语义（同名头后写覆盖先写），HEAD 响应因此可以显式设置真实文件长度。

#### 3.1.3 HttpRequestMethod — 请求方法枚举

支持的方法类型：`GET`, `POST`, `HEAD`, `PUT`, `DELETE`, `TRACE`, `OPTIONS`, `CONNECT`, `PATCH`

通过 `unordered_map` 维护字符串到枚举的映射表，提供 `HttpRequestMethodCheck()` 校验和 `HttpRequestMethodGet()` 转换。

#### 3.1.4 HttpResponseCode — 响应状态码枚举

| 状态码 | 含义 |
|--------|------|
| 200 | GOOD_REQUEST |
| 301 | MOVED_PERMANENTLY（目录 URL 缺少尾斜杠） |
| 304 | NOT_MODIFIED（条件请求命中） |
| 400 | BAD_REQUEST |
| 403 | FORBIDDEN_REQUEST（点路径等） |
| 404 | NO_RESOURCE |
| 405 | METHOD_NOT_ALLOWED（HEAD 请求 .cgi） |
| 500 | INTERNAL_ERROR |
| 503 | SERVICE_UNAVAILABLE（数据库未启用时的 /api/auth/*） |

#### 3.1.5 HttpRequestParser — 请求解析器

采用**逐行解析**策略，遵循 HTTP 协议标准格式：

```
解析状态机：
  PARSER_STATE_REQUESTLINE → PARSER_STATE_HEADER → PARSER_STATE_CONTENT
```

**成员属性：**
| 属性 | 类型 | 说明 |
|------|------|------|
| `parserState_` | enum ParserState | 当前解析状态 |
| `parserBuffer_` | string | 待解析的缓冲区引用 |
| `parserPos_` | size_t | 当前解析位置 |
| `parserLine_` | string | 当前解析的行 |

**解析流程：**
1. `parseRequestLine()` — 解析 `METHOD URL VERSION`
2. `parseHeaders()` — 逐行解析 Header: Value
3. `parseContent()` — 根据 Content-Length 读取请求体
4. `getOneLine()` — 辅助方法，按 `\r\n` 分行
5. `urlDecode()` — URL 解码

#### 3.1.6 HttpResponseParser — 响应构建器

将 HttpResponse 对象序列化为符合 HTTP 协议的字节流：

```
构建状态机：
  PARSER_STATE_STATUSLINE → PARSER_STATE_HEADER → PARSER_STATE_CONTENT → PARSE_STATE_FINISH
```

**核心方法：**
| 方法 | 说明 |
|------|------|
| `httpResponseParser()` | 主接口，完成完整响应构建 |
| `parseStatusLine()` | 构建 `HTTP/1.1 200 OK`（状态消息按值保存后再写入，避免悬垂临时对象） |
| `parseHeaders()` | 构建头部字段；未设置时补 `Date`（IMF-fixdate GMT）；`Content-Length` 仅在处理器未设置时按响应体大小补齐，304 不写该头（RFC 9110 §8.6） |
| `parseContent()` | 写入响应体 |

**输出缓冲区：** 固定 8KB (`BUFFER_SIZE = 8192`)

**MIME 判定：** 由静态资源处理器的 `contentTypeForPath()` 按**真实文件路径**的扩展名决定（不再按 URL 后缀）。

**`Connection` 判定：** 按逗号拆分、忽略 OWS、大小写不敏感地匹配 `keep-alive` / `close`；同时出现两者时 `close` 优先。该解析由 `http_protocol.hpp` 的 `headerHasToken()` 共享给响应构建与连接复用判定。

---

### 3.2 HTTP 连接层 (http_utils/)

#### 3.2.1 HttpConn — HTTP 连接管理

`HttpConn` 是整个系统的核心控制器，负责完整的 HTTP 请求-响应生命周期。

```cpp
class HttpConn {
    static int epollfd;        // 全局 Epoll 文件描述符
    static int userCount;      // 全局用户连接计数

    string readBuffer;         // HTTP 请求读缓冲区
    string writeBuffer;        // HTTP 响应写缓冲区
    int writePos_;             // 写位置（分块写入）
    int sockfd;                // 客户端 socket
    sockaddr_in address;       // 客户端地址
    bool linger;               // 是否长连接
};
```

**缓冲区限制：**
| 常量 | 值 | 说明 |
|------|-----|------|
| `HTTP_REQUEST_SIZE` | 10 MB | 请求上限，超限拒绝访问 |
| `READ_BUFFER_SIZE` | 2 KB | 单次 recv 尺寸 |
| `HTTP_RESPONSE_SIZE` | 64 MB | 响应上限 |
| `WRITE_BUFFER_SIZE` | 8 KB | 单次 send 尺寸 |

**核心方法流程：**

```
process() {
    1. HttpRequestParser 解析 readBuffer → HttpRequest + HttpResponse
    2. Router::route() 获取 handler
    3. handler->handler_pre()  权限/资源检查
    4. handler->handler()      业务逻辑
    5. handler->handler_post() 后处理
    6. HttpResponseParser 序列化 → writeBuffer
    7. modfd() 注册 EPOLLOUT 事件
}
```

**长连接处理：** 在 `bufferWrite()` 完成后，若响应头 `Connection` 含 `keep-alive` token，则调用 `resetConn()` 重置缓冲区并重新注册 `EPOLLIN`。

**访问日志：** 每个请求在响应构建完成后写一行 `ACCESS <方法> <URL> <状态码> <响应体字节> <耗时>ms`，URL 截断到 512 字节并剔除控制字符；由 `log.access_log` 开关控制。日志在持有互斥锁时写入并立即 `fflush`。

**优雅退出：** `main` 捕获 `SIGTERM` / `SIGINT`，主循环以 `sig_atomic_t` 标志退出；先关闭监听 fd，再保持事件循环最多 5 秒排空在途响应（空闲 keep-alive 约 300ms 提前退出），随后 join worker 线程、关闭剩余连接，最后记录 `server stopped` 并释放资源。

#### 3.2.2 Epoll 工具函数

| 函数 | 说明 |
|------|------|
| `setnonblocking(fd)` | 设置 socket 为非阻塞 |
| `addfd(epollfd, fd, one_shot)` | 向 Epoll 添加监听（ET + EPOLLONESHOT + EPOLLRDHUP） |
| `modfd(epollfd, fd, ev)` | 修改监听事件 |
| `removefd(epollfd, fd)` | 移除监听并关闭 socket |

#### 3.2.3 ETagCache — 实体哈希索引

```cpp
class ETagCache {
    static ETagCache& instance();
    std::string etagFor(const std::string& path, const struct stat& st);
};
```

- 以 `std::map<path, Entry{size, stamp, etag}>` 记录「文件大小 + 纳秒 mtime → ETag」，`std::mutex` 保护
- `stamp = st_mtim.tv_sec * 1e9 + tv_nsec`；`size` 一起参与比较，覆盖同秒内改写但 mtime 未变的场景
- 哈希为内容 FNV-1a 64 位，输出带双引号的强校验器（如 `"c2974a9f489cc4c1"`）
- 命中且指纹一致时直接复用，不必重新读文件
- 索引上限 4096 条；超限时按插入序号淘汰最小者，同路径覆盖会刷新序号

#### 3.2.4 gzip — 压缩与有界缓存

```cpp
bool gzipCompress(const char* data, std::size_t len, std::string& out);
bool acceptsGzipEncoding(const std::string& header);

class GzipCache {
    static GzipCache& instance();
    void setBudget(std::size_t bytes);
    bool get(const std::string& path, long long size, long long stamp, std::string& out);
    void put(const std::string& path, long long size, long long stamp, const std::string& body);
    bool isCompressionNotBeneficial(const std::string& path, long long size, long long stamp);
    void markCompressionNotBeneficial(const std::string& path, long long size, long long stamp);
};
```

- 压缩参数：`deflateInit2(level 6, windowBits 15+16)`，即 gzip 包装；输出缓冲区按需倍增
- `acceptsGzipEncoding` 是纯函数，大小写不敏感地解析逗号列表、`q=0`、`*`、`x-gzip`，便于单元测试
- 缓存键为「路径 + 实体大小 + 纳秒指纹」，替换同键条目时正确维护总字节数
- 预算默认 32 MB，由 `asset.gzip_cache_size` 注入；超出预算按插入顺序 FIFO 淘汰，单条超过预算的条目直接拒收
- 负缓存标记（上限 4096 条）记录「压缩不划算」的实体，避免每次请求重复 deflate

---

### 3.3 路由管理层 (router/)

```cpp
class Router {
    static http_handler* route(HttpRequest&, HttpResponse&);
    static void handlerDestory(http_handler*);
};
```

**当前路由规则（基于请求方法）：**

| 方法 | 处理逻辑 |
|------|---------|
| `GET` | `.cgi` → `http_handler_cgi`；其余 → `http_handler_simple` |
| `HEAD` | 同 GET；`HEAD` 到 `.cgi` → `405` + `Allow: GET, POST` |
| `POST` | `.cgi` → `http_handler_cgi`；其余 → `400` |
| 其他 | 返回 400 BAD_REQUEST |

`/api/auth/*` 在数据库未启用时直接返回 `503`，不进入 handler。

**设计初衷：** 基于请求方法和 URL 的映射表进行路由，支持模块化扩展。

---

### 3.4 业务处理层 (handler/)

#### 3.4.1 http_handler — 处理基类（接口）

```cpp
class http_handler {
    virtual bool handler_pre(HttpRequest&, HttpResponse&) = 0;   // 前置检查
    virtual bool handler(HttpRequest&, HttpResponse&) = 0;       // 核心逻辑
    virtual bool handler_post(HttpRequest&, HttpResponse&) = 0;  // 后置处理
};
```

采用**模板方法模式**，约定三段式处理流程：前置检查 → 核心处理 → 后置处理。

#### 3.4.2 http_handler_simple — 静态资源处理

处理 GET / HEAD 静态资源访问，并负责协商缓存、gzip 与路径安全：

```
handler_pre()  → 路径解析与安全校验、缓存头、条件请求（304）、gzip 协商
handler()      → mmap（identity）或取压缩缓存体（gzip），设置响应体
handler_post() → munmap 释放映射
```

**路径与响应行为：**
- 忽略 URL 查询串；编码路径先解码再解析
- 目录 URL 缺少尾斜杠 → `301` 并保留查询串
- 点开头的路径段（如 `/.git/config`）→ `403`
- 缺失路径优先返回 `doc_root/404.html`，无该文件则 404 空正文
- `handler_pre` 只 `open` 一次（`O_RDONLY | O_CLOEXEC`）并 `fstat`，后续 mmap / gzip 读取复用同一 fd，避免「先 stat 后 open」的 TOCTOU；304 / HEAD / 301 等未进入 `handler()` 的路径由析构关闭 fd
- HEAD 返回与 GET 相同的头与真实长度，不发正文；HEAD 到 `.cgi` → `405` + `Allow: GET, POST`

**MIME 类型：** HTML/CSS/JS/MJS/JSON/MAP/XML/SVG/PNG/JPEG/WebP/GIF/ICO/TXT/WOFF/WOFF2/TTF/OTF/EOT/PDF/WASM/WebManifest，未知后缀回退 `application/octet-stream`。

**协商缓存：**
- `ETag`：内容 FNV-1a 64 位哈希（强校验器），由 `ETagCache` 按「路径 + 文件大小 + 纳秒 mtime」索引，进程内加锁复用；部署后 mtime 变化但内容不变时 ETag 不变
- `Last-Modified`：RFC 1123 GMT
- `Cache-Control`：HTML 为 `no-cache`，其余静态资源为 `public, max-age=600`
- 条件请求：`If-None-Match` 优先（支持 `*`、逗号列表、`W/` 前缀），否则用 `If-Modified-Since`；命中返回 `304`（带 ETag/Cache-Control/Vary，无正文、无 Content-Length）

**运行时 gzip：**
- 开关与阈值来自配置（`asset.gzip` / `gzip_min_size` / `gzip_cache_size`）；可压缩类型为 text/*、JS、JSON、XML、SVG、manifest
- 按 `Accept-Encoding` 协商（大小写不敏感，支持逗号列表、`q=0`、`*`、`x-gzip`）
- 压缩结果由 `GzipCache` 缓存（键为路径 + 大小 + 纳秒 mtime，总预算 FIFO 淘汰）；压缩不划算的实体记负缓存标记，后续请求直接走 identity
- 响应带 `Vary: Accept-Encoding`；gzip 表示使用独立强 ETag（`"<hash>-gzip"`），与 identity 表示互不混用
- `Content-Length` 使用压缩后长度，保持 keep-alive 精确分帧；HEAD 与 GET 共用同一压缩结果

#### 3.4.3 http_handler_cgi — CGI 脚本处理（已实现）

在线程池工作线程中通过 `fork + execve` 执行 `doc_root` 下的 Python CGI 脚本：

```
实现流程：
  1. Router 按 URL 后缀 .cgi 选择 CGI 处理器（GET/POST）
  2. realpath 规范化路径，拒绝 doc_root 之外的脚本
  3. 检查普通文件与可执行位，失败返回 404/403/400
  4. 两根管道 + poll() 并发传输请求体与脚本 stdout，避免大 body 死锁
  5. 通过 envp 传入 CONTENT_LENGTH、REQUEST_METHOD、Is_Repeat=0、
     IsCorrect=0、LANG=C.UTF-8、PYTHONIOENCODING=utf-8
  6. 剥离可选的 CGI 响应头，其余作为响应体，默认 Content-Type
     text/html; charset=utf-8
  7. 超时 5000ms、输出上限 1MB、子进程非零退出均返回 500
```

`Is_Repeat` 和 `IsCorrect` 是现有脚本依赖的用户管理占位变量；用户管理模块尚未实现，因此固定为 `0`。

#### 3.4.4 用户管理处理（设计阶段）

基于 MySQL 的用户系统：

**用户基础信息表 `user_basic_info`：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `user_id` | INT UNSIGNED AUTO_INCREMENT PK | 用户 ID |
| `user_name` | VARCHAR(32) UNIQUE NOT NULL | 用户昵称 |
| `hashed_password` | VARBINARY(32) NOT NULL | SHA-256 密码哈希 |
| `salt` | VARBINARY(16) NOT NULL | 随机盐值 |
| `hash_algo` | VARCHAR(32) NOT NULL | 哈希算法标识 |
| `gender` | BOOLEAN | 性别 |
| `profile` | VARCHAR(1024) | 个人简介 |
| `created_time` | TIMESTAMP | 注册时间 |

**安全设计：**
- SHA-256 + 随机盐值哈希存储
- 用户名唯一索引
- 密码复杂度校验（≥8 位，含大小写和特殊字符）
- 前端/后端双重用户名查重

---

### 3.5 基础设施层

#### 3.5.1 ThreadPool — 线程池

```
半同步/半异步模式：
  - Leader 线程（主线程）：Epoll 事件循环，接收连接
  - Worker 线程（线程池）：执行 HTTP 请求处理
```

**核心设计：**

```cpp
template<typename T>
class threadpool {
    int m_thread_number;        // 工作线程数（默认 8）
    int m_max_requests;         // 最大请求队列长度（默认 10000）
    pthread_t* m_threads;       // 线程数组
    list<T*> m_workqueue;       // 任务队列
    locker m_queuelocker;       // 队列互斥锁
    sem m_queuestat;            // 队列信号量
};
```

**工作流程：**
1. 主线程 `pool->append(conn)` 将任务放入队列
2. `sem.post()` 通知工作线程
3. 工作线程 `sem.wait()` 获取信号
4. 加锁取任务 → 执行 `conn->process()`
5. 释放锁，继续等待

#### 3.5.2 Locker — 同步原语

| 类 | 封装 | 用途 |
|----|------|------|
| `sem` | POSIX semaphore | 线程池任务通知 |
| `locker` | pthread mutex | 队列互斥访问 |
| `cond` | pthread condition | 条件变量（预留） |

#### 3.5.3 Config — 配置管理

基于 `libconfig` 库的结构化配置系统：

```cpp
class MyConfig {
    Config cfg;  // libconfig 配置对象
    int initConfig(string filename = "common.cfg");
    bool getQuantityValue(path, value);  // 支持带单位的值（KB/MB）
    bool getTimeValue(path, value);      // 支持时间格式的值
};
```

配置项包括：IP 地址、端口号、日志级别、资源路径等。

#### 3.5.4 Logger — 运行日志

```cpp
enum class LogLevel {
    LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR,
    LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG
};
```

**特性：**
- 基于日志级别过滤
- 自动记录文件名、函数名、行号
- 日志以写模式打开（重启即截断）；`log.max_log_size` / `log.period_timeout` 目前仅解析，尚未参与轮转
- 每行在持有互斥锁时写入并 `fflush`，进程退出前不会丢日志
- `log.access_log`（默认 `true`）控制逐请求 `ACCESS` 行
- 通过宏 `LogRecord(logger, level, format, ...)` 简化调用

#### 3.5.5 Asset — 静态资源管理

```cpp
class Asset {
    static char* doc_root;        // 文档根目录
    static char* default_page;    // 默认首页
    static bool gzip_enabled;     // 运行时 gzip 总开关
    static int gzip_min_size;     // 低于此值不压缩
    static int gzip_cache_size;   // 压缩缓存总预算
    bool initAsset();             // 从配置初始化
};
```

`asset` 配置段示例：

```conf
asset =
{
  doc_root = "/home/dl/DL-py.github.io";
  default_page = "index.html";
  gzip = true;              /* 总开关 */
  gzip_min_size = "1K";     /* quantity 需带 K/M/G 后缀 */
  gzip_cache_size = "32M";  /* 0 表示禁用压缩缓存 */
}
```

非法类型或负值会让 `initAsset()` 失败；无法解析的 quantity 会打印告警并沿用默认值。

#### 3.5.6 Global — 全局实例

单例模式管理全局对象：

```cpp
extern MyConfig globalCFG;    // 全局配置
extern Logger logger;         // 全局日志
extern Asset globalAsset;     // 全局资源
```

---

## 四、数据流图

### 4.1 完整请求生命周期

```
                    ┌──────────────┐
                    │  浏览器/客户端  │
                    └──────┬───────┘
                           │ HTTP Request (TCP)
    ┌──────────────────────▼──────────────────────────┐
    │                  main() 事件循环                  │
    │   epoll_wait() → 事件分发                        │
    │   ├─ listenfd 可读 → accept → initConn          │
    │   ├─ connfd EPOLLIN → bufferRead → pool->append │
    │   └─ connfd EPOLLOUT → bufferWrite → resetConn  │
    └──────────────────────┬──────────────────────────┘
                           │ pool->append(conn)
    ┌──────────────────────▼──────────────────────────┐
    │              线程池 Worker 线程                   │
    │   sem.wait() → conn->process()                  │
    └──────────────────────┬──────────────────────────┘
                           │
    ┌──────────────────────▼──────────────────────────┐
    │              HttpConn::process()                 │
    │                                                  │
    │   readBuffer (string)                           │
    │       │                                          │
    │       ▼                                          │
    │   ┌──────────────────┐                          │
    │   │ HttpRequestParser │                          │
    │   │ parseRequestLine  │ → method_, url_, version_│
    │   │ parseHeaders      │ → headers_              │
    │   │ parseContent      │ → body_                 │
    │   └────────┬─────────┘                          │
    │            ▼                                     │
    │   ┌──────────────────┐                          │
    │   │   Router::route   │                          │
    │   │ GET/HEAD → handler│                          │
    │   │       _simple     │                          │
    │   └────────┬─────────┘                          │
    │            ▼                                     │
    │   ┌──────────────────┐                          │
    │   │   http_handler    │                          │
    │   │   handler_pre     │ → 路径/权限、ETag、304、   │
    │   │                   │   gzip 协商               │
    │   │   handler         │ → mmap 或压缩缓存体        │
    │   │   handler_post    │ → munmap                 │
    │   └────────┬─────────┘                          │
    │            ▼                                     │
    │   ┌──────────────────┐                          │
    │   │HttpResponseParser │                          │
    │   │ parseStatusLine   │ → HTTP/1.1 200 OK        │
    │   │ parseHeaders      │ → Content-Type, ...      │
    │   │ parseContent      │ → 响应体                  │
    │   └────────┬─────────┘                          │
    │            ▼                                     │
    │   writeBuffer (string)                          │
    │   modfd(epollfd, sockfd, EPOLLOUT)               │
    └──────────────────────┬──────────────────────────┘
                           │ EPOLLOUT 事件触发
    ┌──────────────────────▼──────────────────────────┐
    │           HttpConn::bufferWrite()                │
    │   send() 分段写入 socket → 回应浏览器             │
    │   keep-alive? → resetConn → 注册 EPOLLIN         │
    │   否则 → closeConn                               │
    └─────────────────────────────────────────────────┘
```

---

## 五、目录结构

项目按功能模块组织，主要目录职责如下：

| 目录/文件 | 职责 |
|-----------|------|
| `main.cpp` | 程序入口，主事件循环 |
| `config/` | 基于 libconfig 的配置管理 |
| `global/` | 全局单例实例 |
| `logs/` | 多级别运行日志 |
| `asset/` | 静态资源管理（文档根目录、默认页） |
| `http_messages/` | HTTP 消息数据结构（请求/响应/状态码/解析器） |
| `http_utils/` | HTTP 连接、Epoll 工具、ETag 索引（`etag_cache`）、gzip 压缩与缓存（`gzip`） |
| `router/` | 基于请求方法的分发 |
| `handler/` | 业务处理器（静态资源、CGI 等） |
| `tests/` | 集成测试（`hexo_hosting.sh`）与 gzip 单元测试 |
| `docs/` | 项目文档 |

---

## 六、技术栈

| 技术 | 用途 |
|------|------|
| C/C++ | 主编程语言 |
| CMake | 构建系统 |
| Epoll | Linux I/O 多路复用 |
| POSIX Threads | 多线程并发 |
| mmap | 零拷贝文件传输 |
| zlib | 运行时 gzip 压缩 |
| libconfig | 结构化配置解析 |
| MySQL | 用户数据持久化（设计阶段） |
| Python | CGI 脚本语言（已实现） |
| SHA-256 | 密码哈希算法 |
| Postman | API 测试与压力测试 |

---

## 七、并发模型

```
                          ┌─────────────────┐
                          │   Leader 线程    │
                          │  (主事件循环)     │
                          └────────┬────────┘
                                   │
                    epoll_wait() 事件分发
                                   │
              ┌────────────────────┼────────────────────┐
              │                    │                    │
         ┌────▼────┐         ┌────▼────┐         ┌────▼────┐
         │ accept  │         │ buffer  │         │ buffer  │
         │ 新连接   │         │ Read    │         │ Write   │
         └────┬────┘         └────┬────┘         └────▲────┘
              │                   │                    │
              │          pool->append()               │
              │                   │                    │
              │        ┌──────────▼──────────┐        │
              │        │     任务队列          │        │
              │        │  list<HttpConn*>     │        │
              │        └──────────┬──────────┘        │
              │                   │                    │
              │        ┌──────────▼──────────┐        │
              │        │    线程池 (8线程)     │        │
              │        │  sem.wait() 等待任务  │        │
              │        │  conn->process()     ├────────┘
              │        │  处理完成后注册写事件   │
              │        └──────────────────────┘
              │
              └──────▶ HttpConn[] 连接池 (MAX_FD = 65536)
```

---

## 八、已规划的扩展模块

| 模块 | 状态 | 说明 |
|------|------|------|
| CGI 支持 | 已实现 | Python 脚本处理，通过管道传输 HTTP 请求体 |
| 静态站点托管 | 已实现 | 目录 301、点路径 403、404 页面、编码路径、无数据库启动 |
| 协商缓存与 HEAD | 已实现 | 强 ETag（内容哈希）、Last-Modified、304、Cache-Control、HEAD |
| Gzip 压缩 | 已实现 | 运行时 gzip，`Accept-Encoding` 协商、压缩缓存、`Vary`、变体 ETag |
| 用户管理 | 设计完成 | MySQL + SHA-256，支持注册/登录/注销/信息修改 |
| Range / 206 | 需求确认 | 大文件分段下载 |
| TLS（HTTPS） | 需求确认 | 证书与 TLS 终止 |
| 异常处理 | 需求确认 | 统一异常处理逻辑（资源不存在、无权限等） |
| 压力测试 | 方案确定 | 基于 Postman 的压力测试与性能分析 |

---

## 九、关键设计决策

1. **EPOLLONESHOT 模式**：确保每个 socket 连接在任意时刻仅被一个线程处理，避免竞态条件。

2. **边沿触发 (ET)**：配合非阻塞 I/O，避免惊群效应和重复通知，提升性能。

3. **请求缓冲区上限**：10MB 硬限制，防止内存耗尽攻击；同时使用 `std::string` 动态伸缩，兼顾灵活性。

4. **分段写入**：8KB 分块写入，避免大响应（最大 64MB）一次性发送阻塞 socket。

5. **mmap 零拷贝**：静态文件直接内存映射，减少内核态-用户态数据拷贝。

6. **SHA-256 + 盐值**：密码安全存储，每个用户独立的 16 字节随机盐值。

7. **模板方法模式**：`handler_pre → handler → handler_post` 三段式接口，统一业务处理流程。

8. **内容哈希强 ETag**：FNV-1a 64 位哈希，按「路径 + 文件大小 + 纳秒 mtime」索引复用；`hexo deploy` 刷新 mtime 但内容不变时 ETag 不变，重复访问仍可命中 304。

9. **编码变体独立校验器**：gzip 与 identity 表示使用不同强 ETag（gzip 加 `-gzip` 后缀），并统一发 `Vary: Accept-Encoding`，避免共享缓存把两种表示混用。

10. **keep-alive 为显式 opt-in**：仅当请求 `Connection` 含 `keep-alive` token 才复用连接，HTTP/1.1 默认持久连接未实现（RFC 9112 §9.3 偏差）。代价是浏览器等默认不发该头的 HTTP/1.1 客户端会为每个资源新建连接；本阶段按决策保留该偏差。

11. **有界压缩缓存**：压缩结果按总预算 FIFO 淘汰，键含文件大小与纳秒 mtime；压缩不划算的实体记录负缓存标记，后续重验证直接走 identity 并可正常返回 304。