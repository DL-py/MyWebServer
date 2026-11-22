# 用户管理模块设计（方向 A：MySQL + C++ 原生 handler）

- 日期：2026-11-22
- 状态：已批准（2026-11-22）
- 关联：`specs/MyWebServer.md`、`refer/user_handle.c`、`refer/ConnPool.*`

## 1. 背景与目标

现状（已核实）：

- `web/login.html`、`web/register.html` 只是静态表单；`login.html` 的 `action` 还错指到 `register.cgi`。
- `web/register.cgi` 只要收到请求体就打印“注册成功”，不落库。
- `web/login.cgi` 依赖 `IsCorrect`，而 `handler/cgi_exec_args.cpp` 固定写入 `IsCorrect=0`，所以永远显示失败。
- 没有持久化、没有 Session/Cookie。
- `refer/` 已有 MySQL 参考实现（`ConnPool`、`user_handle.c`），但存明文密码、表名与 spec 不一致。

目标：

- 真正的注册、登录、注销、查看个人资料（本期不做修改）。
- 用户数据持久化到 MySQL；密码加盐哈希存储。
- 登录状态用 Cookie + 服务端 Session。

成功标准：

- 浏览器可完成“注册 → 登录 → 访问 `/api/auth/me` → 注销”。
- 服务重启后用户数据不丢。
- 非法输入、重复用户名、错误密码都有明确提示。

## 2. 非目标与反目标

非目标（本期不做）：

- OAuth/第三方登录、邮箱验证、找回密码。
- 修改个人资料（本期只做查看）。
- HTTPS 终止、分布式 Session、多机部署。
- 完整 CSRF token、登录限流。

反目标（出现即失败）：

- 明文存密码。
- 把密码或 Session token 写进日志。
- 字符串拼接 SQL。
- 数据库口令硬编码进源码。

## 3. 约束

- C++11、CMake 2.8；Epoll + 线程池，每请求一个 worker 线程。
- 沿用 `http_handler` 三段式接口（`handler_pre`/`handler`/`handler_post`）与 `Router` 方法分发。
- 沿用 libconfig 配置（`etc/common.cfg.example`）。
- 本机已装 OpenSSL 3.0.2；**未装 MySQL 客户端库与服务端**。安装 `default-libmysqlclient-dev` + `mariadb-server` 与编码同步进行（需要 sudo/网络）。
- 仓库约定：master 不自动提交；测试产生的文件/库跑完清理。

## 4. 方案对比（已选 A）

| 方案 | 做法 | 优点 | 缺点 |
|------|------|------|------|
| A（选中） | MySQL + C++ 原生 `user_handler` | 贴合 spec/refer；无 fork/exec；能练连接池、哈希、Session | 工作量大，依赖 MySQL |
| B | MySQL + Python CGI | C++ 改动最小 | 每请求 fork+exec，性能差，与 C++ 项目目标不符 |
| C | SQLite + C++/CGI | 零运维、可移植 | 偏离 spec 的 MySQL |
| D | 文件/JSON | 最简单 | 并发与一致性差，仅适合演示 |

选 A 的理由：项目本身是 C++ 服务器学习项目，`refer/` 已给出 MySQL 参考，spec 也定了 MySQL；原生 handler 能顺带实现 Session 与连接池。

## 5. 总体结构

新增模块：

- `db/`：`DbPool`（MySQL 连接池）、`DbConfig`。
- `auth/`：`password_hash`（PBKDF2-HMAC-SHA256）、`form_parser`（`application/x-www-form-urlencoded`）、`session_store`（内存 Session）。
- `handler/user_handler.{h,cpp}`：注册/登录/注销/查看资料。
- `sql/schema.sql`：建库建表。
- 改动：`router/router.cpp`、`handler/CMakeLists.txt`、根 `CMakeLists.txt`、`etc/common.cfg.example`、`web/*.html`、`http_messages/http_response_code.h`。

请求流：

```
浏览器 → HttpConn::process → Router 识别 /api/auth/*
      → user_handler → DbPool 借连接 → 预处理语句
      → 写 Session / Set-Cookie → HTML 或 302
```

## 6. 数据库设计

统一表名沿用 spec 的 `user_basic_info`；字段沿用现有表单与 `refer/` 的命名（`hobby/sex/introduction/address`），并补上 spec 的哈希字段。`sex`、`introduction` 分别对应 spec 里的 `gender`、`profile`。

```sql
CREATE DATABASE IF NOT EXISTS mywebserver
  DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE user_basic_info (
  user_id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  user_name       VARCHAR(32)  NOT NULL,
  hashed_password VARBINARY(32) NOT NULL,
  salt            VARBINARY(16) NOT NULL,
  hash_algo       VARCHAR(32)  NOT NULL,
  iterations      INT UNSIGNED NOT NULL,
  hobby           VARCHAR(32)  NULL,
  sex             VARCHAR(8)   NULL,
  introduction    VARCHAR(1024) NULL,
  address         VARCHAR(64)  NULL,
  created_time    TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uk_user_name (user_name)
) ENGINE=InnoDB;
```

- `confirm` 只做前后端校验，不入库。
- `hobby/sex/address` 以白名单值入库（见现有下拉框选项）。

用户名大小写：采用 `utf8mb4_unicode_ci`，即 `Alice` 与 `alice` 视为同一账号；保留用户输入时的原始大小写。这是本设计的决定，若需区分大小写改用 `utf8mb4_bin`（上线后改需迁移）。

Session 默认放内存：

- `unordered_map<token, {user_id, expires_at}>` + `mutex`。
- 进程重启后失效，可接受；如需持久化再加 `user_session(token VARBINARY(32) PK, user_id, expires_at)` 表。

## 7. 密码与安全

- 哈希：PBKDF2-HMAC-SHA256，16 字节随机盐（`RAND_bytes`），100000 次迭代，32 字节输出。
  - `hash_algo='pbkdf2-sha256'`，`iterations` 入库，便于以后升级。
  - 说明：spec 写“SHA-256 + 盐”；裸 SHA-256 太快、抗暴力破解弱，故用 PBKDF2（仍属 SHA-256 家族）。若要严格裸 SHA-256，只需改 `password_hash` 一处。
- SQL：全部使用预处理语句（`mysql_stmt_*`），不拼接、不依赖转义。
- 日志：不打印密码、盐、Session token。
- 输入校验：用户名 3–32 位，仅字母/数字/下划线；密码 ≥8 位且含大小写与特殊字符（spec）；两次密码一致；`hobby/sex/address` 仅接受白名单值，`introduction` 限长 1024。
- Session token：`RAND_bytes(32)` 转 hex；Cookie 设 `HttpOnly; SameSite=Lax; Path=/`。未启用 HTTPS，暂不设 `Secure`（记为已知限制）。
- 登录失败统一提示“用户不存在或密码错误”，避免枚举用户。

## 8. 接口设计

| 方法 | 路径 | 表单/说明 | 成功 | 失败 |
|------|------|-----------|------|------|
| POST | `/api/auth/register` | `user,password,confirm,hobby,sex,introduction,address` | 302 → `/api/auth/me` | 400 校验失败 / 409 用户名重复 / 500 DB 错误，均返回错误 HTML |
| POST | `/api/auth/login` | `user,password` | 302 + `Set-Cookie` → `/api/auth/me` | 400 校验失败 / 401 凭证错误 / 500 DB 错误 |
| POST | `/api/auth/logout` | 读 Cookie | 302 → `welcome.html` | 302 |
| GET | `/api/auth/me` | 读 Cookie | 200 + 完整资料 HTML（含全部字段与注销表单） | 401 |

前端改动：

- `login.html` 的 `action` 改为 `/api/auth/login`。
- `register.html` 的 `action` 改为 `/api/auth/register`，增加 `confirm` 输入，`user` 的 `maxlength` 改为 32；保留 `hobby/sex/introduction/address`。
- 保留现有 `styles.css` 与页面结构。
- `/api/auth/me` 直接返回完整 HTML 页面，引用绝对路径 `/styles.css`，展示全部字段并内含 `POST /api/auth/logout` 的表单；不新增静态页。
- 登录/注册成功统一跳 `/api/auth/me`，它同时充当“受保护页面”。

## 9. 关键实现点

- `Router`：在 `.cgi` 判断之前，先按 `/api/auth/` 前缀匹配到 `user_handler`；方法不符返回 405。
- 状态码枚举补 `302/400/401/405/409/500`；错误页统一用 `setStatusCodeAndMessage(code, msg)`，避免 `assert`。
- 错误码约定：参数/格式错误 400，用户名重复 409，未登录或凭证错误 401，DB/内部错误 500。
- `HttpResponse::setHeader` 仅在 key 不存在时写入；`Set-Cookie` 作为新 key 可写入。当前结构不支持多个同名响应头，本设计只需一个 Cookie。
- 请求头查找大小写敏感：读 Cookie 时同时尝试 `Cookie` 与 `cookie`，或让 `getHeader` 做大小写不敏感匹配。
- 表单解析：按 `&`、`=` 拆分后 URL 解码（复用 `http_request_parser.cpp` 中 `urlDecode` 的逻辑）；`Content-Length` 已由解析器校验。
- 连接池：启动时按 `db.pool_size` 建连；worker 借用/归还；连接失效时重连；`mysql_library_init/end` 在 `main` 中配对调用。
- 建库/建表：由管理员一次性执行 `sql/schema.sql`；应用启动时检查表存在，不存在则报错退出，不做自动迁移。
- 测试库：集成测试用独立的 `mywebserver_test`，用有 DDL 权限的测试账号创建/删除，跑完清理。
- 依赖安装：`default-libmysqlclient-dev` + `mariadb-server`（或 `mysql-server`）；CMake 用 `find_library(MYSQLCLIENT mysqlclient)`，链接 `mysqlclient crypto`。

## 10. 配置

`etc/common.cfg.example` 新增：

```text
db =
{
  host = "127.0.0.1";
  port = 3306;
  user = "myweb";
  password = "change-me";
  name = "mywebserver";
  pool_size = 8;
};
```

- 真实口令不提交；本地用 `etc/common.cfg` 覆盖（已被 `.gitignore` 忽略则沿用现状）。
- 启动时读取失败要报错退出，不静默降级。

## 11. 测试

- 单元：密码哈希（同盐同结果、异盐异结果）、表单解析、Session 过期。
- 集成：`tests/user_integration.sh` 用 curl 走注册/登录/me/注销，并校验 `hobby/sex/introduction/address` 已入库；使用独立 `mywebserver_test` 库，由测试账号创建/删除，跑完清理临时文件（AGENTS.md）。
- 手工：浏览器完整走一遍，确认 302 与 Cookie。

## 12. 迁移与兼容

- 保留 `.cgi` 脚本但前端不再指向；确认无引用后再删。
- `Is_Repeat`/`IsCorrect` 环境变量废弃，可保留或从 `cgi_exec_args.cpp` 移除。

## 13. 风险与开放问题

- 本机缺 MySQL 依赖，安装与编码并行；若 sudo/网络受阻会阻塞编译与端到端测试。
- 纯 HTTP：Session Cookie 可被窃听，仅适合本地/学习用途。
- 内存 Session 重启丢失；需要持久化再切 `user_session` 表。
- 未做 CSRF token 与限流；`SameSite=Lax` 提供基础防护。
