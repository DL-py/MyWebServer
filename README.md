# MyWebServer

基于 C++11 的轻量级 HTTP 服务器（Epoll + 线程池），支持静态站点托管、CGI 与基于 MySQL 的用户管理。

## 特性

- Epoll 边沿触发 + `EPOLLONESHOT` + 非阻塞 I/O，半同步/半异步线程池
- HTTP/1.1 GET/POST/HEAD；`Date`、条件请求（ETag/Last-Modified → 304）、HEAD
- 静态资源：目录 301、点路径 403、`404.html` 回退、运行时 gzip 与压缩缓存
- `Connection: keep-alive` 需客户端显式请求，否则响应后关闭连接
- CGI（Python）与 `/api/auth/*` 用户注册/登录/资料/注销
- 运行日志 + 逐请求 `ACCESS` 日志；`SIGTERM` / `SIGINT` 优雅退出
- 未内置 HTTPS：需要 TLS 时建议由 nginx 等反向代理终止

## 目录结构

| 路径 | 说明 |
|------|------|
| `src/` | C++ 源码与配置模板 `common.cfg.example` |
| `web/` | 前端页面、样式与 CGI 脚本 |
| `shells/` | 启动脚本 `start-server.sh` |
| `sql/` | 数据库建表与授权脚本 |
| `docs/` | 设计文档 |
| `tests/` | 本地测试脚本（不纳入版本库） |
| `build/` | 构建产物（忽略） |
| `devel/` | 可执行文件、共享库与本地配置（忽略） |

## 环境依赖

Ubuntu：

```bash
sudo apt-get install -y build-essential cmake libconfig++-dev libconfig-dev libmysqlclient-dev libssl-dev zlib1g-dev
```

## 构建

```bash
cmake -S . -B build
cmake --build build -j4
```

## 运行

```bash
./shells/start-server.sh
```

首次运行会从 `src/common.cfg.example` 生成 `devel/common.cfg`。

- 默认地址：<http://127.0.0.1:6380>
- 运行日志：`devel/rtmsg.log`
- 进程名为 `leader`

后台运行：

```bash
./shells/start-server.sh >/tmp/mywebserver.log 2>&1 &
```

优雅停止（等待在途响应并 flush 日志）：

```bash
kill -TERM "$(pgrep -x leader | head -1)"
```

仅在进程无响应时使用强杀：`fuser -k 6380/tcp`。

## 访问

启动后用浏览器打开：

- 首页：<http://127.0.0.1:6380/>
- 注册：<http://127.0.0.1:6380/register.html>
- 登录：<http://127.0.0.1:6380/login.html>

端口以 `devel/common.cfg` 的 `conn.port` 为准；注册与登录需要 `db.enabled = true` 且已初始化数据库。

## 配置

配置文件为 `devel/common.cfg`，模板见 `src/common.cfg.example`：

| 段 | 关键项 | 说明 |
|----|--------|------|
| `conn` | `ip`、`port` | 监听地址与端口，默认 `0.0.0.0:6380` |
| `log` | `log_file_name`、`log_level`、`access_log` | 日志路径、级别、逐请求 ACCESS 开关 |
| `asset` | `doc_root`、`default_page`、`gzip`、`gzip_min_size`、`gzip_cache_size` | 站点根目录与压缩策略 |
| `db` | `enabled`、`host`、`port`、`user`、`password`、`name`、`pool_size` | MySQL 连接；关闭时 `/api/auth/*` 返回 503 |

说明：

- `max_log_size` / `period_timeout` 目前仅解析，尚未参与日志轮转
- ACCESS 行格式：`ACCESS <方法> <URL> <状态码> <响应体字节> <耗时>ms`

## 数据库

用户数据存于 MySQL。首次初始化需管理员权限（创建库、表、用户与授权）：

```bash
sudo mysql < sql/schema.sql
```

日常查看使用应用账号：

```bash
mysql -u myweb -pchange-me mywebserver
```

## 文档

- [设计说明书](docs/MyWebServer.md)：架构、模块、协议行为与关键决策
- [验证与测试手册](docs/VERIFICATION.md)：开发者冒烟、协议行为、缓存压缩、日志退出与单元测试

