---
title: "MyWebServer 总体设计说明"
description: "基于 C/C++ 实现的轻量级 HTTP 服务器架构设计文档，涵盖 Epoll 并发模型、HTTP 消息层、连接管理、路由分发、线程池等模块的详细设计。"
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
| HTTP 协议 | 支持 HTTP/1.1 GET/POST，支持 Connection: keep-alive |
| 配置管理 | 基于 libconfig 的结构化配置 |
| 路由管理 | 基于请求方法的路由分发 |
| 静态资源 | 支持 HTML/CSS/JS/图片等静态文件服务（mmap 零拷贝） |
| CGI 扩展 | 支持 Python CGI 脚本（已实现） |
| 用户管理 | MySQL 数据库 + SHA-256 加盐哈希（设计阶段） |
| 日志系统 | 多级别运行时日志，支持日志截断 |

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
| `getHeader() / setHeader()` | 读取/设置请求头字段 |
| `getBody() / setBody()` | 获取/设置请求体 |

#### 3.1.2 HttpResponse — HTTP 响应

```cpp
class HttpResponse {
    int statusCode_;                          // 状态码 (200/400/404/500)
    string statusMessage_;                    // 状态消息
    unordered_map<string, string> headers_;   // 响应头部
    string body_;                             // 响应体
};
```

**额外方法：** `setStatusCodeAndMessage()` — 通过枚举值自动设置状态码和消息。

#### 3.1.3 HttpRequestMethod — 请求方法枚举

支持的方法类型：`GET`, `POST`, `HEAD`, `PUT`, `DELETE`, `TRACE`, `OPTIONS`, `CONNECT`, `PATCH`

通过 `unordered_map` 维护字符串到枚举的映射表，提供 `HttpRequestMethodCheck()` 校验和 `HttpRequestMethodGet()` 转换。

#### 3.1.4 HttpResponseCode — 响应状态码枚举

| 状态码 | 含义 |
|--------|------|
| 200 | GOOD_REQUEST |
| 400 | BAD_REQUEST |
| 403 | FORBIDDEN_REQUEST |
| 404 | NO_RESOURCE |
| 500 | INTERNAL_ERROR |

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
| `parseStatusLine()` | 构建 `HTTP/1.1 200 OK` |
| `parseHeaders()` | 构建头部字段（Content-Type, Content-Length, Connection 等） |
| `parseContent()` | 写入响应体 |
| `getContentType()` | 根据 URL 后缀自动判断 MIME 类型 |

**输出缓冲区：** 固定 8KB (`BUFFER_SIZE = 8192`)

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

**长连接处理：** 在 `bufferWrite()` 完成后，若 HTTP 头部 `Connection: keep-alive`，则调用 `resetConn()` 重置缓冲区并重新注册 `EPOLLIN`。

#### 3.2.2 Epoll 工具函数

| 函数 | 说明 |
|------|------|
| `setnonblocking(fd)` | 设置 socket 为非阻塞 |
| `addfd(epollfd, fd, one_shot)` | 向 Epoll 添加监听（ET + EPOLLONESHOT + EPOLLRDHUP） |
| `modfd(epollfd, fd, ev)` | 修改监听事件 |
| `removefd(epollfd, fd)` | 移除监听并关闭 socket |

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
| `GET` | 创建 `http_handler_simple`（静态资源服务） |
| `POST` | 预留（CGI / 用户管理接口） |
| 其他 | 返回 400 BAD_REQUEST |

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

处理 GET 请求的静态资源访问：

```
handler_pre()  → 权限检查（文件存在性、可读性）
handler()      → mmap 文件到内存，设置响应头（Content-Type, Content-Length）
handler_post() → munmap 释放映射
```

**关键特性：**
- 使用 `mmap` 零拷贝技术
- 自动识别 MIME 类型
- 支持默认首页（`index.html`）
- 文件路径长度限制：1024 字节

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
- 文件大小监控与自动截断
- 通过宏 `LogRecord(logger, level, format, ...)` 简化调用

#### 3.5.5 Asset — 静态资源管理

```cpp
class Asset {
    static char* doc_root;       // 文档根目录
    static char* default_page;   // 默认首页
    bool initAsset();            // 从配置初始化
};
```

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
    │   │ GET  → handler    │                          │
    │   │       _simple     │                          │
    │   └────────┬─────────┘                          │
    │            ▼                                     │
    │   ┌──────────────────┐                          │
    │   │   http_handler    │                          │
    │   │   handler_pre     │ → 文件存在? 权限?         │
    │   │   handler         │ → mmap 文件, 设响应头     │
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
| `http_utils/` | HTTP 连接管理与 Epoll 工具函数 |
| `router/` | 基于请求方法的路由分发 |
| `handler/` | 业务处理器（静态资源、CGI 等） |
| `refer/` | 参考实现与待开发模块 |
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
| 用户管理 | 设计完成 | MySQL + SHA-256，支持注册/登录/注销/信息修改 |
| Gzip 压缩 | 需求确认 | 支持 gzip 传输格式，优化传输性能 |
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