# MyWebServer

基于 C++11 的轻量级 HTTP 服务器（Epoll + 线程池），支持静态页面、CGI 和用户管理（注册 / 登录 / 资料 / 注销）。

## 目录结构

- `src/`：全部 C++ 源码（`main.cpp`、`locker.h`、`threadpool.h` 及 10 个模块）与配置模板 `common.cfg.example`
- `web/`：前端页面、样式、favicon 与 CGI 脚本
- `shells/`：服务启动脚本（`start-server.sh`）
- `sql/`：建库建表脚本
- `docs/`：项目文档（`MyWebServer.md`）
- `reference/`：旧参考实现，不参与构建
- `superpowers/`：中间设计与计划（git 忽略）
- `build/`：构建目录（git 忽略）
- `devel/`：可执行文件、库与本地运行配置（`devel/common.cfg`，git 忽略）

## 构建

```bash
cmake --build build -j4
```

新环境先配置（本机无系统 libconfig++，使用 `devel/lib/vendor/`）：

```bash
cmake -S . -B build \
  -DCMAKE_CXX_FLAGS=-I$PWD/devel/lib/vendor/include \
  -DCMAKE_EXE_LINKER_FLAGS=-L$PWD/devel/lib/vendor
```

## 运行

```bash
./shells/start-server.sh
```

首次运行时，脚本会从 `src/common.cfg.example` 生成 `devel/common.cfg`。

- 默认地址：http://127.0.0.1:6380
- 注册：http://127.0.0.1:6380/register.html
- 登录：http://127.0.0.1:6380/login.html
- 停止：`fuser -k 6380/tcp`

## 数据库

用户数据存于 MySQL。首次初始化需管理员权限（脚本会创建库、表、用户与授权）：

```bash
sudo mysql < sql/schema.sql
```

日常查看使用应用账号（仅读写 `mywebserver`）：

```bash
mysql -u myweb -pchange-me mywebserver
```

`devel/common.cfg` 的 `db` 段配置连接信息（模板见 `src/common.cfg.example`）。
