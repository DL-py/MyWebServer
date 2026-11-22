# 用户管理模块实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 MyWebServer 增加真正的注册、登录、注销、查看资料能力，用户数据落 MySQL，密码加盐哈希，登录态用 Cookie + 内存 Session。

**Architecture:** 新增 `db/`（MySQL 连接池）、`auth/`（表单解析、PBKDF2 哈希、Session）两个模块，新增 `handler/user_handler` 承载 `/api/auth/*`，`Router` 在 `.cgi` 判断前把该前缀路由到它。`main` 启动时初始化连接池，worker 线程通过 `DbPool` 借还连接。

**Tech Stack:** C++11、CMake 2.8、Epoll + 线程池、MariaDB/MySQL C API、OpenSSL 3（PBKDF2/RAND）、libconfig。

**Spec:** `specs/2026-11-22-user-management-design.md`

## Global Constraints

- C++11；CMake 最低 2.8；沿用现有 `http_handler` 三段式接口与 `Router` 方法分发。
- 数据库：MariaDB/MySQL；表 `user_basic_info`；字符集 `utf8mb4 COLLATE utf8mb4_unicode_ci`。
- 密码：PBKDF2-HMAC-SHA256，16 字节盐，100000 次迭代，32 字节输出，`hash_algo='pbkdf2-sha256'`。
- 所有 SQL 必须用预处理语句（`mysql_stmt_*`），禁止字符串拼接。
- 端点：`POST /api/auth/register`、`POST /api/auth/login`、`POST /api/auth/logout`、`GET /api/auth/me`。
- 状态码：302 成功跳转；400 参数/格式错误；401 未登录/凭证错误；405 方法不允许；409 用户名重复；500 DB/内部错误。
- Cookie：`session=<hex>; HttpOnly; SameSite=Lax; Path=/`，不设 `Secure`（当前是 HTTP）。
- 注册字段：`user,password,confirm,hobby,sex,introduction,address`；`confirm` 只校验不入库。
- 本期不做修改资料。
- 测试产生的临时文件、日志、测试库必须清理；master 不自动提交，只在功能分支/worktree 提交。

## Review Focus

1. **畸形 `%XX` 表单**：`user=%ZZ` 应返回 400 而不是崩溃或空值入库；测试在 Task 1 单元测试与 Task 8 集成测试。
2. **资料页 XSS**：`introduction=<b>world</b>` 在 `/api/auth/me` 必须输出 `&lt;b&gt;world&lt;/b&gt;`，不能原样输出标签；测试在 Task 8。
3. **用户名重复（大小写不敏感）**：先注册 `alice` 再注册 `Alice` 必须 409，不能 500 或建出两个账号；测试在 Task 8。
4. **无效/过期/缺失 Cookie**：`/api/auth/me` 必须 401；测试在 Task 3 单元测试与 Task 8。
5. **超长或缺失字段**：`introduction` 超过 1024 字节或缺少必填字段必须 400，不能 500 或截断入库；测试在 Task 8。

---

### Task 0: 环境与数据库初始化

**Files:**
- Create: `sql/schema.sql`
- Create: `sql/schema_test.sql`

**Interfaces:**
- Consumes: 无。
- Produces: 数据库 `mywebserver`、`mywebserver_test`；账号 `myweb`/`change-me`（仅本地开发占位，生产必须改）；两库各有 `user_basic_info` 表。

- [ ] **Step 1: 安装依赖（与编码并行；需要 sudo 与网络）**

```bash
sudo apt-get update
sudo apt-get install -y default-libmysqlclient-dev mariadb-server
sudo service mariadb start
```

- [ ] **Step 2: 写 `sql/schema.sql`**

包含：建 `mywebserver` 库、建 `user_basic_info` 表、建 `myweb` 账号并授权。

```sql
CREATE DATABASE IF NOT EXISTS mywebserver
  DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS mywebserver.user_basic_info (
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

CREATE USER IF NOT EXISTS 'myweb'@'localhost' IDENTIFIED BY 'change-me';
CREATE USER IF NOT EXISTS 'myweb'@'127.0.0.1' IDENTIFIED BY 'change-me';
GRANT SELECT, INSERT, UPDATE, DELETE ON mywebserver.* TO 'myweb'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON mywebserver.* TO 'myweb'@'127.0.0.1';
GRANT ALL PRIVILEGES ON mywebserver_test.* TO 'myweb'@'localhost';
GRANT ALL PRIVILEGES ON mywebserver_test.* TO 'myweb'@'127.0.0.1';
FLUSH PRIVILEGES;
```

- [ ] **Step 3: 写 `sql/schema_test.sql`**

同样的表结构，但库名是 `mywebserver_test`，不建账号。

```sql
CREATE DATABASE IF NOT EXISTS mywebserver_test
  DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS mywebserver_test.user_basic_info (
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

- [ ] **Step 4: 初始化并验证**

```bash
sudo mysql < sql/schema.sql
sudo mysql < sql/schema_test.sql
mysql -u myweb -pchange-me -e "SHOW TABLES" mywebserver
mysql -u myweb -pchange-me -e "SHOW TABLES" mywebserver_test
mysql -h 127.0.0.1 -u myweb -pchange-me -e "SHOW TABLES" mywebserver
```

Expected: 三条命令都输出 `user_basic_info`（第三条验证 TCP 账号 `'myweb'@'127.0.0.1'`）。

- [ ] **Step 5: 准备分支并提交**

当前 worktree 是 detached HEAD，先建功能分支：

```bash
git switch -c feat/user-management
git add sql/schema.sql sql/schema_test.sql \
        specs/2026-11-22-user-management-design.md \
        specs/plans/2026-11-22-user-management.md
git commit -m "chore: add user database schema and design docs"
```

---

### Task 1: 表单 URL 编码解析

**Files:**
- Create: `auth/form_parser.h`
- Create: `auth/form_parser.cpp`
- Create: `auth/CMakeLists.txt`
- Create: `tests/form_parser_test.cpp`
- Modify: `CMakeLists.txt`（根，新增 `add_subdirectory(auth)` 与 `target_link_libraries(myhttp auth)`）

**Interfaces:**
- Produces:
  - `bool parseFormUrlEncoded(const std::string& body, std::unordered_map<std::string, std::string>& out);`
  - 返回 `false` 表示存在畸形 `%XX`；重复 key 以最后一次为准；空值保留（`key=` → `""`）。

- [ ] **Step 1: 写失败测试 `tests/form_parser_test.cpp`**

```cpp
#include "../auth/form_parser.h"
#include <iostream>

int main()
{
    std::unordered_map<std::string, std::string> out;
    if (!parseFormUrlEncoded("user=alice&password=a%20b&hobby=%E8%BF%90%E5%8A%A8&empty=", out))
    {
        std::cerr << "FAIL: valid form rejected\n";
        return 1;
    }
    if (out["user"] != "alice" || out["password"] != "a b" ||
        out["hobby"] != u8"运动" ||
        out.count("empty") != 1 || !out["empty"].empty())
    {
        std::cerr << "FAIL: decoded values\n";
        return 1;
    }

    std::unordered_map<std::string, std::string> bad;
    if (parseFormUrlEncoded("user=%ZZ", bad))
    {
        std::cerr << "FAIL: malformed percent accepted\n";
        return 1;
    }

    std::unordered_map<std::string, std::string> dup;
    parseFormUrlEncoded("a=1&a=2", dup);
    if (dup["a"] != "2")
    {
        std::cerr << "FAIL: duplicate key should keep last\n";
        return 1;
    }

    std::cout << "form parser test PASS\n";
    return 0;
}
```

- [ ] **Step 2: 运行，确认失败**

```bash
g++ -std=c++11 -I. tests/form_parser_test.cpp auth/form_parser.cpp -o /tmp/form_parser_test
```

Expected: 编译失败，`auth/form_parser.h` 或 `.cpp` 不存在。

- [ ] **Step 3: 实现 `auth/form_parser.h/.cpp` 与 `auth/CMakeLists.txt`**

`auth/form_parser.h`：

```cpp
#ifndef AUTH_FORM_PARSER_H
#define AUTH_FORM_PARSER_H
#include <string>
#include <unordered_map>

bool parseFormUrlEncoded(const std::string& body,
                         std::unordered_map<std::string, std::string>& out);
#endif
```

实现要点：按 `&` 分段，按第一个 `=` 分 key/value；`+` 转空格；`%XX` 两位十六进制，非法则返回 `false`；空段跳过。

`auth/CMakeLists.txt` 与现有模块同构：

```cmake
cmake_minimum_required (VERSION 2.8)
get_filename_component(CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib/${CUR_DIR})
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")
add_library(auth SHARED form_parser.cpp)
```

根 `CMakeLists.txt` 增加 `add_subdirectory(auth)`，并给 `myhttp` 加 `target_link_libraries(myhttp auth)`。

- [ ] **Step 4: 运行，确认通过**

```bash
g++ -std=c++11 -I. tests/form_parser_test.cpp auth/form_parser.cpp -o /tmp/form_parser_test && /tmp/form_parser_test
```

Expected: `form parser test PASS`。

- [ ] **Step 5: 提交**

```bash
git add auth/form_parser.h auth/form_parser.cpp auth/CMakeLists.txt tests/form_parser_test.cpp CMakeLists.txt
git commit -m "feat: add form urlencoded parser"
```

---

### Task 2: 密码哈希

**Files:**
- Create: `auth/password_hash.h`
- Create: `auth/password_hash.cpp`
- Modify: `auth/CMakeLists.txt`（加入 `password_hash.cpp`，链接 `crypto`）
- Create: `tests/password_hash_test.cpp`

**Interfaces:**
- Produces:
  - `struct PasswordHash { std::vector<unsigned char> salt; std::vector<unsigned char> hash; int iterations; std::string algo; };`
  - `PasswordHash hashPasswordWithSalt(const std::string& password, const std::vector<unsigned char>& salt, int iterations);`
  - `PasswordHash hashPassword(const std::string& password);`
  - `bool verifyPassword(const std::string& password, const PasswordHash& stored);`
  - 默认值：盐 16 字节、哈希 32 字节、迭代 100000、算法 `pbkdf2-sha256`。

- [ ] **Step 1: 写失败测试 `tests/password_hash_test.cpp`**

```cpp
#include "../auth/password_hash.h"
#include <iostream>

int main()
{
    std::vector<unsigned char> salt(16, 0x42);
    PasswordHash a = hashPasswordWithSalt("Passw0rd!", salt, 1000);
    PasswordHash b = hashPasswordWithSalt("Passw0rd!", salt, 1000);
    if (a.hash != b.hash || a.hash.size() != 32 || a.algo != "pbkdf2-sha256")
    {
        std::cerr << "FAIL: deterministic hash\n";
        return 1;
    }
    if (!verifyPassword("Passw0rd!", a) || verifyPassword("wrong", a))
    {
        std::cerr << "FAIL: verify\n";
        return 1;
    }
    PasswordHash badAlgo = a;
    badAlgo.algo = "sha256";
    PasswordHash badIter = a;
    badIter.iterations = 0;
    if (verifyPassword("Passw0rd!", badAlgo) || verifyPassword("Passw0rd!", badIter))
    {
        std::cerr << "FAIL: unsupported stored parameters accepted\n";
        return 1;
    }

    PasswordHash c = hashPassword("Passw0rd!");
    if (c.salt.size() != 16 || c.hash.size() != 32 || c.iterations != 100000)
    {
        std::cerr << "FAIL: default sizes\n";
        return 1;
    }
    if (!verifyPassword("Passw0rd!", c))
    {
        std::cerr << "FAIL: verify random salt\n";
        return 1;
    }
    if (hashPassword("Passw0rd!").hash == hashPassword("Passw0rd!").hash)
    {
        std::cerr << "FAIL: random salts should differ\n";
        return 1;
    }

    std::cout << "password hash test PASS\n";
    return 0;
}
```

- [ ] **Step 2: 运行，确认失败**

```bash
g++ -std=c++11 -I. tests/password_hash_test.cpp auth/password_hash.cpp -lcrypto -o /tmp/password_hash_test
```

Expected: 编译失败，文件不存在。

- [ ] **Step 3: 实现 `auth/password_hash.h/.cpp`**

用 OpenSSL：`RAND_bytes` 生成盐（失败抛 `std::runtime_error`）；`PKCS5_PBKDF2_HMAC(password.data(), password.size(), salt.data(), salt.size(), iterations, EVP_sha256(), 32, out.data())`（失败同样抛异常）；`verifyPassword` 先检查 `stored.algo == "pbkdf2-sha256"`、`stored.iterations > 0`、`stored.salt.size() == 16`、`stored.hash.size() == 32`，不满足直接返回 `false`；满足则用存储的盐和迭代重算，再用 `CRYPTO_memcmp` 比较。

`auth/CMakeLists.txt` 改为：

```cmake
add_library(auth SHARED form_parser.cpp password_hash.cpp)
target_link_libraries(auth crypto)
```

- [ ] **Step 4: 运行，确认通过**

```bash
g++ -std=c++11 -I. tests/password_hash_test.cpp auth/password_hash.cpp -lcrypto -o /tmp/password_hash_test && /tmp/password_hash_test
```

Expected: `password hash test PASS`。

- [ ] **Step 5: 提交**

```bash
git add auth/password_hash.h auth/password_hash.cpp auth/CMakeLists.txt tests/password_hash_test.cpp
git commit -m "feat: add pbkdf2 password hashing"
```

---

### Task 3: Session 存储

**Files:**
- Create: `auth/session_store.h`
- Create: `auth/session_store.cpp`
- Modify: `auth/CMakeLists.txt`（加入 `session_store.cpp`，链接 `crypto` 与 `pthread`）
- Create: `tests/session_store_test.cpp`

**Interfaces:**
- Produces:
  - `struct Session { int userId; std::time_t expiresAt; };`
  - `class SessionStore { explicit SessionStore(int defaultTtlSeconds = 86400); std::string create(int userId, int ttlSeconds = -1); bool get(const std::string& token, Session& out) const; void remove(const std::string& token); void purgeExpired(); };`
  - `create` 返回 32 字节随机数的 64 位十六进制字符串；`ttlSeconds == -1` 用默认 TTL；`get` 对过期 token 返回 `false`。

- [ ] **Step 1: 写失败测试 `tests/session_store_test.cpp`**

```cpp
#include "../auth/session_store.h"
#include <iostream>

int main()
{
    SessionStore store(60);
    std::string t = store.create(42);
    if (t.size() != 64)
    {
        std::cerr << "FAIL: token length\n";
        return 1;
    }
    Session s;
    if (!store.get(t, s) || s.userId != 42)
    {
        std::cerr << "FAIL: get\n";
        return 1;
    }
    store.remove(t);
    if (store.get(t, s))
    {
        std::cerr << "FAIL: removed token still valid\n";
        return 1;
    }

    std::string expired = store.create(7, 0);
    if (store.get(expired, s))
    {
        std::cerr << "FAIL: expired token accepted\n";
        return 1;
    }
    if (store.create(1) == store.create(2))
    {
        std::cerr << "FAIL: tokens should differ\n";
        return 1;
    }

    std::cout << "session store test PASS\n";
    return 0;
}
```

- [ ] **Step 2: 运行，确认失败**

```bash
g++ -std=c++11 -I. tests/session_store_test.cpp auth/session_store.cpp -lcrypto -lpthread -o /tmp/session_store_test
```

Expected: 编译失败，文件不存在。

- [ ] **Step 3: 实现 `auth/session_store.h/.cpp`**

用 `RAND_bytes` 生成 32 字节 token 并转十六进制（失败抛 `std::runtime_error`）；`std::mutex`（`mutable`）保护 `std::unordered_map<std::string, Session>`；`create` 计算 `expiresAt = time(nullptr) + ttl`（`ttlSeconds == -1` 用默认值）后插入并返回 token；`get` 先查表，再判断 `expiresAt > std::time(nullptr)`，过期则返回 `false`（可顺手 `remove`）；`purgeExpired` 遍历删除过期项。

`auth/CMakeLists.txt` 改为：

```cmake
add_library(auth SHARED form_parser.cpp password_hash.cpp session_store.cpp)
target_link_libraries(auth crypto pthread)
```

- [ ] **Step 4: 运行，确认通过**

```bash
g++ -std=c++11 -I. tests/session_store_test.cpp auth/session_store.cpp -lcrypto -lpthread -o /tmp/session_store_test && /tmp/session_store_test
```

Expected: `session store test PASS`。

- [ ] **Step 5: 提交**

```bash
git add auth/session_store.h auth/session_store.cpp auth/CMakeLists.txt tests/session_store_test.cpp
git commit -m "feat: add in-memory session store"
```

---

### Task 4: MySQL 连接池

**Files:**
- Create: `db/db_pool.h`
- Create: `db/db_pool.cpp`
- Create: `db/CMakeLists.txt`
- Create: `tests/db_pool_test.cpp`
- Modify: `CMakeLists.txt`（根，新增 `add_subdirectory(db)`、`find_path`/`find_library`、链接 `db` 与 `${MYSQLCLIENT_LIB}`；`auth`/`db` 必须排在 `global`/`handler` 之前）

**Interfaces:**
- Produces:
  - `struct DbConfig { std::string host; int port; std::string user; std::string password; std::string name; int poolSize; };`
  - `class DbPool { DbPool(int size, DbCreateFn create, DbDestroyFn destroy, bool pingOnAcquire = true); ~DbPool(); MYSQL* acquire(); void release(MYSQL* conn); int capacity() const; };`
  - `using DbCreateFn = std::function<MYSQL*()>; using DbDestroyFn = std::function<void(MYSQL*)>;`
  - `DbPool* makeMysqlPool(const DbConfig& config);`（失败抛 `std::runtime_error`）
  - `acquire()` 内部调用 `mysql_thread_init()`，保证 worker 线程可用 C API。

- [ ] **Step 1: 写失败测试 `tests/db_pool_test.cpp`**

```cpp
#include "../db/db_pool.h"
#include <iostream>

int main()
{
    static int fake1 = 1;
    static int fake2 = 2;
    MYSQL* p1 = reinterpret_cast<MYSQL*>(&fake1);
    MYSQL* p2 = reinterpret_cast<MYSQL*>(&fake2);
    int created = 0;

    DbPool pool(2,
        [&]() -> MYSQL* { return (created++ == 0) ? p1 : p2; },
        [](MYSQL*) {},
        false);  // 测试用假指针，跳过 mysql_ping

    MYSQL* a = pool.acquire();
    MYSQL* b = pool.acquire();
    if (!a || !b || a == b || pool.capacity() != 2)
    {
        std::cerr << "FAIL: acquire\n";
        return 1;
    }
    pool.release(a);
    MYSQL* c = pool.acquire();
    if (c != a)
    {
        std::cerr << "FAIL: release/acquire order\n";
        return 1;
    }
    pool.release(b);
    pool.release(c);

    std::cout << "db pool test PASS\n";
    return 0;
}
```

- [ ] **Step 2: 运行，确认失败**

```bash
g++ -std=c++11 -I. tests/db_pool_test.cpp db/db_pool.cpp -lmysqlclient -lpthread -o /tmp/db_pool_test
```

Expected: 编译失败，文件不存在。

- [ ] **Step 3: 实现 `db/db_pool.h/.cpp` 与 `db/CMakeLists.txt`**

`db/db_pool.h`：

```cpp
#ifndef DB_DB_POOL_H
#define DB_DB_POOL_H
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <mysql.h>

struct DbConfig {
    std::string host;
    int port;
    std::string user;
    std::string password;
    std::string name;
    int poolSize;
};

using DbCreateFn = std::function<MYSQL*()>;
using DbDestroyFn = std::function<void(MYSQL*)>;

class DbPool {
public:
    DbPool(int size, DbCreateFn create, DbDestroyFn destroy, bool pingOnAcquire = true);
    ~DbPool();
    MYSQL* acquire();
    void release(MYSQL* conn);
    int capacity() const;
private:
    std::list<MYSQL*> available_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    DbCreateFn create_;
    DbDestroyFn destroy_;
    bool pingOnAcquire_;
    int capacity_;
    bool closed_;
};

DbPool* makeMysqlPool(const DbConfig& config);
#endif
```

实现要点：构造时循环 `size` 次调用 `create()` 放入 `available_`；任一 `create()` 抛异常时先 `destroy_` 已建连接再重抛。`acquire()` 先 `mysql_thread_init()`，再用 `std::unique_lock` 等待 `!available_.empty() || closed_`，取出队首；`pingOnAcquire_` 为真时对取出的连接执行 `mysql_ping()`，非 0 时 `destroy_` 掉并用 `create_` 重连（重连失败抛 `std::runtime_error`，由 handler 转 500）；`release()` 加锁放回并 `notify_one`；析构对剩余连接调用 `destroy_`。`makeMysqlPool` 的 create lambda 用 `mysql_init` + `mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4")` + `mysql_real_connect(host,user,password,name,port,NULL,CLIENT_FOUND_ROWS)`，任一步失败则 `mysql_close` 并抛 `std::runtime_error`；destroy 用 `mysql_close`。`DbConfig.poolSize <= 0` 视为配置错误并抛异常。

`MYSQL_BIND::is_null` 的类型随客户端库而变（MariaDB 为 `my_bool`，MySQL 8 为 `bool`）；在 `db/db_pool.h` 用 `MYSQL_VERSION_ID >= 80000 ? bool : my_bool` 定义别名，后续 handler 统一使用该别名。

`db/CMakeLists.txt`：

```cmake
cmake_minimum_required (VERSION 2.8)
get_filename_component(CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib/${CUR_DIR})
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")
add_library(db SHARED db_pool.cpp)
target_link_libraries(db ${MYSQLCLIENT_LIB} pthread)
```

根 `CMakeLists.txt` 在 `add_subdirectory` 前加：

```cmake
find_path(MYSQLCLIENT_INCLUDE NAMES mysql.h PATH_SUFFIXES mysql mariadb)
find_library(MYSQLCLIENT_LIB NAMES mysqlclient mariadb)
if (NOT MYSQLCLIENT_INCLUDE OR NOT MYSQLCLIENT_LIB)
  message(FATAL_ERROR "MySQL client library not found; install default-libmysqlclient-dev")
endif()
include_directories(${MYSQLCLIENT_INCLUDE})
```

并加 `add_subdirectory(db)`（放在 `add_subdirectory(global)` 之前）、`target_link_libraries(myhttp db ${MYSQLCLIENT_LIB})`。

安装依赖后先重新配置一次，让 `find_path`/`find_library` 生效：

```bash
cmake -S . -B build \
  -DCMAKE_CXX_FLAGS=-I$PWD/devel/lib/vendor/include \
  -DCMAKE_EXE_LINKER_FLAGS=-L$PWD/devel/lib/vendor
```

- [ ] **Step 4: 运行，确认通过**

```bash
g++ -std=c++11 -I. -I/usr/include/mysql -I/usr/include/mariadb tests/db_pool_test.cpp db/db_pool.cpp -lmysqlclient -lpthread -o /tmp/db_pool_test && /tmp/db_pool_test
```

Expected: `db pool test PASS`。

- [ ] **Step 5: 提交**

```bash
git add db/db_pool.h db/db_pool.cpp db/CMakeLists.txt tests/db_pool_test.cpp CMakeLists.txt
git commit -m "feat: add mysql connection pool"
```

---

### Task 5: HTTP 状态码

**Files:**
- Modify: `http_messages/http_response_code.h`
- Modify: `http_messages/http_response_code.cpp`
- Create: `tests/status_code_test.cpp`

**Interfaces:**
- Produces: 枚举新增 `MOVED_TEMPORARILY = 302`、`UNAUTHORIZED = 401`、`METHOD_NOT_ALLOWED = 405`、`CONFLICT = 409`；`setStatusCodeAndMessage(code)` 对这四个值不再触发 `assert`。

- [ ] **Step 1: 写失败测试 `tests/status_code_test.cpp`**

```cpp
#include "../http_messages/http_response.h"
#include <iostream>

int main()
{
    struct Case { int code; const char* text; };
    Case cases[] = {
        {302, "Found"}, {401, "Unauthorized"},
        {405, "Method Not Allowed"}, {409, "Conflict"},
    };
    for (const Case& c : cases)
    {
        HttpResponse resp;
        resp.setStatusCodeAndMessage(c.code);
        if (resp.getStatusCode() != c.code || resp.getStatusMessage() != c.text)
        {
            std::cerr << "FAIL: status " << c.code << "\n";
            return 1;
        }
    }
    std::cout << "status code test PASS\n";
    return 0;
}
```

- [ ] **Step 2: 运行，确认失败**

```bash
g++ -std=c++11 -I. tests/status_code_test.cpp http_messages/http_response.cpp http_messages/http_response_code.cpp -o /tmp/status_code_test && /tmp/status_code_test
```

Expected: 进程因 `assert` 终止（map 中没有 302 等）。

- [ ] **Step 3: 修改枚举与 map**

`http_response_code.h` 的 `enum HTTP_RESPONSE_CODE` 加入四个值；`http_response_code.cpp` 的 `httpResponseCodeInfo` 加入对应中文/英文文本（用上面测试里的文本）。

- [ ] **Step 4: 运行，确认通过**

```bash
g++ -std=c++11 -I. tests/status_code_test.cpp http_messages/http_response.cpp http_messages/http_response_code.cpp -o /tmp/status_code_test && /tmp/status_code_test
```

Expected: `status code test PASS`。

- [ ] **Step 5: 提交**

```bash
git add http_messages/http_response_code.h http_messages/http_response_code.cpp tests/status_code_test.cpp
git commit -m "feat: add auth-related http status codes"
```

---

### Task 6: 配置、全局实例与启动接线

**Files:**
- Modify: `global/global.hpp`
- Modify: `global/global.cpp`
- Modify: `global/CMakeLists.txt`（给 `global` 加 `target_link_libraries(global auth)`）
- Modify: `main.cpp`
- Modify: `etc/common.cfg.example`
- Create: `tests/db_startup_test.sh`

**Interfaces:**
- Consumes: `DbPool`、`makeMysqlPool`（Task 4）；`SessionStore`（Task 3）。
- Produces:
  - `extern DbPool* globalDbPool;`（`global.hpp` 只前置声明 `class DbPool;`）
  - `extern SessionStore globalSessionStore;`
  - 启动日志包含 `database pool ready: <host>/<name>`。
  - 配置项 `db.host/port/user/password/name/pool_size`。

- [ ] **Step 1: 写失败测试 `tests/db_startup_test.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
good=/tmp/myhttp-db-good.cfg
bad=/tmp/myhttp-db-bad.cfg
log=/tmp/myhttp-db-test.log
applog=/tmp/myhttp-db-app.log

cleanup() {
    fuser -k 6392/tcp 6393/tcp >/dev/null 2>&1 || true
    mysql -u myweb -pchange-me -e "DROP DATABASE IF EXISTS mywebserver_test" >/dev/null 2>&1 || true
    rm -f "$good" "$bad" "$log" "$applog"
}
trap cleanup EXIT

mysql -u myweb -pchange-me -e "DROP DATABASE IF EXISTS mywebserver_test"
mysql -u myweb -pchange-me < "$root/sql/schema_test.sql"

cp "$root/etc/common.cfg.example" "$good"
sed -i 's/port = 6380;/port = 6392;/' "$good"
sed -i 's/name = "mywebserver";/name = "mywebserver_test";/' "$good"
sed -i "s|log_file_name = \"./rtmsg.log\";|log_file_name = \"$applog\";|" "$good"

cd "$root"
./run.sh --config "$good" >"$log" 2>&1 &

for _ in $(seq 1 40); do
    code=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:6392/welcome.html 2>/dev/null || true)
    [ "$code" = "200" ] && break
    sleep 0.25
done
[ "${code:-000}" = "200" ] || { echo "FAIL: server did not start"; cat "$log"; exit 1; }
grep -q 'database pool ready' "$applog" || { echo "FAIL: missing pool log"; cat "$applog" 2>/dev/null || cat "$log"; exit 1; }

sed 's/password = "change-me";/password = "wrong";/' "$good" > "$bad"
sed -i 's/port = 6392;/port = 6393;/' "$bad"
rm -f "$applog"  # 避免上一轮 "database pool ready" 让断言假通过
if timeout 10 ./run.sh --config "$bad" >"$log" 2>&1; then
    echo "FAIL: server started with bad DB password"; exit 1
fi
grep -q 'database pool init failed' "$applog" 2>/dev/null || grep -qi 'database' "$log" \
    || { echo "FAIL: bad DB error not reported"; cat "$applog" 2>/dev/null || cat "$log"; exit 1; }

echo "DB startup test PASS"
```

- [ ] **Step 2: 运行，确认失败**

```bash
cmake --build build -j4
bash tests/db_startup_test.sh
```

Expected: 启动失败或日志中没有 `database pool ready`。

- [ ] **Step 3: 修改配置、全局实例与 `main`**

`etc/common.cfg.example` 追加（`change-me` 是占位；真实部署改 `etc/common.cfg`，该文件已被 `.gitignore` 忽略）：

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

`global/global.hpp`：

```cpp
#include "../auth/session_store.h"

class DbPool;
extern DbPool* globalDbPool;
extern SessionStore globalSessionStore;
```

`global/global.cpp` 增加定义：

```cpp
DbPool* globalDbPool = nullptr;
SessionStore globalSessionStore;
```

`global/CMakeLists.txt` 末尾加：

```cmake
target_link_libraries(global auth)
```

`main.cpp` 在 `globalAsset.initAsset()` 之后：

1. 加 `#include "db/db_pool.h"` 与 `#include <mysql.h>`，调 `if (mysql_library_init(0, NULL, NULL) != 0) { ... return -1; }`。
2. 从 `cfg.lookup` 读 `db.host/port/user/password/name/pool_size`，填 `DbConfig`；缺项或异常时打印错误并 `return -1`。
3. `try { globalDbPool = makeMysqlPool(dbConfig); } catch (const std::exception& e) { LogRecord(logger, LogLevel::LOG_ERR, "database pool init failed: %s\n", e.what()); return -1; }`
4. 启动自检：`MYSQL* conn = globalDbPool->acquire();`，用 `MYSQL_STMT` 预处理并执行 `SELECT 1 FROM user_basic_info LIMIT 1`（固定语句，同样走 `mysql_stmt_*`）；无论成功失败都先 `mysql_stmt_close`（若有）再 `release(conn)`；失败时记录 `database table check failed: %s` 并 `return -1`。
5. `LogRecord(logger, LogLevel::LOG_INFO, "database pool ready: %s/%s\n", dbConfig.host.c_str(), dbConfig.name.c_str());`
6. 在 `return 0` 前 `delete globalDbPool; globalDbPool = nullptr; mysql_library_end();`（与现有 `delete pool` 同处；worker 线程是 detached，进程即将退出，属已知限制）。

- [ ] **Step 4: 运行，确认通过**

```bash
cmake --build build -j4
bash tests/db_startup_test.sh
```

Expected: `DB startup test PASS`。

- [ ] **Step 5: 提交**

```bash
git add global/global.hpp global/global.cpp global/CMakeLists.txt main.cpp etc/common.cfg.example tests/db_startup_test.sh
git commit -m "feat: wire mysql pool and session store into startup"
```

---

### Task 7: 零长度 POST 请求体

**Files:**
- Modify: `http_utils/http_request_parser.cpp`
- Create: `tests/http_body_test.cpp`

**Interfaces:**
- Consumes: `HttpRequestParser::httpRequestParser(HttpRequest&, HttpResponse&)`。
- Produces: `Content-Length: 0` 的 POST 被接受，`req.getBody()` 为空串；非零长度行为不变。

- [ ] **Step 1: 写失败测试 `tests/http_body_test.cpp`**

```cpp
#include "../http_utils/http_request_parser.h"
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"
#include <iostream>

static bool parse(const std::string& raw, HttpRequest& req, HttpResponse& resp)
{
    HttpRequestParser parser(raw);
    return parser.httpRequestParser(req, resp);
}

int main()
{
    HttpRequest emptyReq;
    HttpResponse emptyResp;
    const std::string empty =
        "POST /api/auth/logout HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n";
    if (!parse(empty, emptyReq, emptyResp) ||
        emptyResp.getStatusCode() == 400 ||
        emptyReq.getMethod() != "POST" || !emptyReq.getBody().empty())
    {
        std::cerr << "FAIL: zero-length body\n";
        return 1;
    }

    HttpRequest bodyReq;
    HttpResponse bodyResp;
    const std::string body =
        "POST /api/auth/login HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nhello";
    if (!parse(body, bodyReq, bodyResp) ||
        bodyResp.getStatusCode() == 400 || bodyReq.getBody() != "hello")
    {
        std::cerr << "FAIL: normal body\n";
        return 1;
    }

    std::cout << "http body test PASS\n";
    return 0;
}
```

- [ ] **Step 2: 运行，确认失败**

```bash
g++ -std=c++11 -I. tests/http_body_test.cpp \
    http_utils/http_request_parser.cpp http_messages/http_request.cpp \
    http_messages/http_response.cpp http_messages/http_response_code.cpp \
    http_messages/http_request_method.cpp -o /tmp/http_body_test && /tmp/http_body_test
```

Expected: `FAIL: zero-length body`（CONTENT 分支把空 body 当 400）。

- [ ] **Step 3: 修复 `http_utils/http_request_parser.cpp`**

在 `httpRequestParser` 的 `PARSER_STATE_HEADER` 分支，处理空行时把：

```cpp
if (contentLengthString.empty())
{
    return true;
}
else
{
    parserState_ = PARSER_STATE_CONTENT;
}
```

改成：

```cpp
if (contentLengthString.empty() || contentLengthString == "0")
{
    return true;  // 无 body 或 Content-Length: 0，body 保持空串
}
else
{
    parserState_ = PARSER_STATE_CONTENT;
}
```

只放行精确的 `Content-Length: 0`，畸形/缺失内容仍按原逻辑 400；不要进入 CONTENT 状态，避免零长度时反复读取。

同时把 `PARSER_STATE_CONTENT` 分支改成读完 body 立即结束解析：

```cpp
case PARSER_STATE_CONTENT:
{
    if (!parseContent(req, resp))
    {
        return false;
    }
    return true;  // body 已按 Content-Length 读完，不要再进 getOneLine
}
```

否则读完正常 body 后循环会再进 CONTENT，`parserPos_ >= parserBuffer_.size()` 把响应误设为 400（当前被 handler 覆盖，属于潜在 bug）。

- [ ] **Step 4: 运行，确认通过**

```bash
g++ -std=c++11 -I. tests/http_body_test.cpp \
    http_utils/http_request_parser.cpp http_messages/http_request.cpp \
    http_messages/http_response.cpp http_messages/http_response_code.cpp \
    http_messages/http_request_method.cpp -o /tmp/http_body_test && /tmp/http_body_test
```

Expected: `http body test PASS`。

- [ ] **Step 5: 提交**

```bash
git add http_utils/http_request_parser.cpp tests/http_body_test.cpp
git commit -m "fix: accept zero-length POST bodies"
```

---

### Task 8: 用户处理器与路由

**Files:**
- Create: `handler/user_handler.h`
- Create: `handler/user_handler.cpp`
- Modify: `handler/CMakeLists.txt`
- Modify: `router/router.cpp`
- Create: `tests/user_integration.sh`

**Interfaces:**
- Consumes: `parseFormUrlEncoded`、`hashPassword`、`verifyPassword`、`SessionStore`、`DbPool`、`globalDbPool`、`globalSessionStore`。
- Produces: `class http_user_handler : public http_handler`，处理 `/api/auth/{register,login,logout,me}`。

- [ ] **Step 1: 写失败集成测试 `tests/user_integration.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
config=/tmp/myhttp-user-test.cfg
body=/tmp/myhttp-user-body.html
cookie=/tmp/myhttp-user-cookie.txt
cookie2=/tmp/myhttp-user-cookie2.txt
log=/tmp/myhttp-user-test.log
applog=/tmp/myhttp-user-app.log

cleanup() {
    fuser -k 6391/tcp >/dev/null 2>&1 || true
    mysql -u myweb -pchange-me -e "DROP DATABASE IF EXISTS mywebserver_test" >/dev/null 2>&1 || true
    rm -f "$config" "$body" "$cookie" "$cookie2" "$log" "$applog"
}
trap cleanup EXIT

mysql -u myweb -pchange-me -e "DROP DATABASE IF EXISTS mywebserver_test"
mysql -u myweb -pchange-me < "$root/sql/schema_test.sql"

cp "$root/etc/common.cfg.example" "$config"
sed -i 's/port = 6380;/port = 6391;/' "$config"
sed -i 's/name = "mywebserver";/name = "mywebserver_test";/' "$config"
sed -i "s|log_file_name = \"./rtmsg.log\";|log_file_name = \"$applog\";|" "$config"

cd "$root"
./run.sh --config "$config" >"$log" 2>&1 &
for _ in $(seq 1 40); do
    code=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:6391/welcome.html 2>/dev/null || true)
    [ "$code" = "200" ] && break
    sleep 0.25
done
[ "${code:-000}" = "200" ] || { echo "FAIL: server did not start"; cat "$log"; cat "$applog" 2>/dev/null || true; exit 1; }

fail() { echo "FAIL: $1"; exit 1; }
post() { curl -s -o "$body" -w '%{http_code}' "$@"; }

# 注册成功，自动登录
code=$(post -c "$cookie" \
    --data-urlencode 'user=alice' \
    --data-urlencode 'password=Passw0rd!' \
    --data-urlencode 'confirm=Passw0rd!' \
    --data-urlencode 'hobby=运动' \
    --data-urlencode 'sex=girl' \
    --data-urlencode 'introduction=<b>world</b>' \
    --data-urlencode 'address=北京' \
    http://127.0.0.1:6391/api/auth/register)
[ "$code" = "302" ] || fail "register returned $code"

# 资料页：字段存在、HTML 转义
code=$(curl -s -b "$cookie" -o "$body" -w '%{http_code}' http://127.0.0.1:6391/api/auth/me)
[ "$code" = "200" ] || fail "me returned $code"
grep -q 'alice' "$body" || fail "me missing user"
grep -q '运动' "$body" || fail "me missing hobby"
grep -q 'girl' "$body" || fail "me missing sex"
grep -q '北京' "$body" || fail "me missing address"
grep -q '&lt;b&gt;world&lt;/b&gt;' "$body" || fail "introduction not escaped"
if grep -q '<b>world</b>' "$body"; then fail "raw html in profile"; fi

# 重复用户名（大小写不敏感）
code=$(post --data-urlencode 'user=Alice' --data-urlencode 'password=Passw0rd!' \
    --data-urlencode 'confirm=Passw0rd!' --data-urlencode 'hobby=运动' \
    --data-urlencode 'sex=girl' --data-urlencode 'introduction=x' \
    --data-urlencode 'address=北京' http://127.0.0.1:6391/api/auth/register)
[ "$code" = "409" ] || fail "duplicate returned $code"

# 错误密码
code=$(post --data-urlencode 'user=alice' --data-urlencode 'password=Wrong123!' \
    http://127.0.0.1:6391/api/auth/login)
[ "$code" = "401" ] || fail "wrong password returned $code"

# 正确登录
code=$(post -c "$cookie2" --data-urlencode 'user=alice' \
    --data-urlencode 'password=Passw0rd!' http://127.0.0.1:6391/api/auth/login)
[ "$code" = "302" ] || fail "login returned $code"

# 畸形百分号、超长字段、SQL 注入式用户名
code=$(post --data 'user=%ZZ' http://127.0.0.1:6391/api/auth/register)
[ "$code" = "400" ] || fail "malformed form returned $code"

# 缺少 confirm
code=$(post --data-urlencode 'user=bob' --data-urlencode 'password=Passw0rd!' \
    --data-urlencode 'hobby=运动' --data-urlencode 'sex=boy' \
    --data-urlencode 'introduction=x' --data-urlencode 'address=北京' \
    http://127.0.0.1:6391/api/auth/register)
[ "$code" = "400" ] || fail "missing confirm returned $code"

long=$(printf 'x%.0s' {1..1025})
code=$(post --data-urlencode 'user=bob' --data-urlencode 'password=Passw0rd!' \
    --data-urlencode 'confirm=Passw0rd!' --data-urlencode 'hobby=运动' \
    --data-urlencode 'sex=boy' --data-urlencode "introduction=$long" \
    --data-urlencode 'address=北京' http://127.0.0.1:6391/api/auth/register)
[ "$code" = "400" ] || fail "oversized introduction returned $code"

code=$(post --data-urlencode "user=' OR '1'='1" --data-urlencode 'password=Passw0rd!' \
    --data-urlencode 'confirm=Passw0rd!' --data-urlencode 'hobby=运动' \
    --data-urlencode 'sex=boy' --data-urlencode 'introduction=x' \
    --data-urlencode 'address=北京' http://127.0.0.1:6391/api/auth/register)
[ "$code" = "400" ] || fail "sql injection username returned $code"

# 方法不符与未知路径
code=$(curl -s -o "$body" -w '%{http_code}' http://127.0.0.1:6391/api/auth/logout)
[ "$code" = "405" ] || fail "GET logout returned $code"
code=$(post --data 'x=1' http://127.0.0.1:6391/api/auth/me)
[ "$code" = "405" ] || fail "POST me returned $code"
code=$(curl -s -o "$body" -w '%{http_code}' http://127.0.0.1:6391/api/auth/nope)
[ "$code" = "404" ] || fail "unknown auth path returned $code"

# 缺失 Cookie
code=$(curl -s -o "$body" -w '%{http_code}' http://127.0.0.1:6391/api/auth/me)
[ "$code" = "401" ] || fail "me without cookie returned $code"

# 注销（浏览器空表单会发 Content-Length: 0）后 me 必须 401
code=$(post -X POST --data '' -b "$cookie2" http://127.0.0.1:6391/api/auth/logout)
[ "$code" = "302" ] || fail "logout returned $code"
code=$(curl -s -b "$cookie2" -o "$body" -w '%{http_code}' http://127.0.0.1:6391/api/auth/me)
[ "$code" = "401" ] || fail "me after logout returned $code"

# 无效 Cookie
code=$(curl -s -o "$body" -w '%{http_code}' -H 'Cookie: session=deadbeef' http://127.0.0.1:6391/api/auth/me)
[ "$code" = "401" ] || fail "invalid cookie returned $code"

echo "user integration test PASS"
```

- [ ] **Step 2: 运行，确认失败**

```bash
cmake --build build -j4
bash tests/user_integration.sh
```

Expected: 注册返回 404（路由尚未实现）。

- [ ] **Step 3: 实现 `handler/user_handler.h/.cpp` 与路由**

`handler/user_handler.h`：

```cpp
#ifndef MYWEBSERVER_HTTP_HANDLER_USER_H
#define MYWEBSERVER_HTTP_HANDLER_USER_H
#include "handler.h"

class http_user_handler : public http_handler {
public:
    http_user_handler();
    ~http_user_handler();
    bool handler_pre(HttpRequest& req, HttpResponse& resp) override;
    bool handler(HttpRequest& req, HttpResponse& resp) override;
    bool handler_post(HttpRequest& req, HttpResponse& resp) override;
};
#endif
```

`handler/user_handler.cpp` 需要包含 `handler.h`、`../auth/form_parser.h`、`../auth/password_hash.h`、`../db/db_pool.h`、`../global/global.hpp`、`<mysql.h>`。关键点：

- `handler_pre` 与 `handler_post` 都只返回 `true`（`handler_post` 不再改响应，避免覆盖 `handler` 的结果）；所有分发与响应写入都在 `handler` 中完成。
- `handler` 按 `req.getURL()` 去掉查询串后分发；未知路径 404；方法不符 405。
- `register`：解析表单 → 校验 → 哈希 → INSERT → 用 `mysql_stmt_insert_id(stmt)` 取 `user_id` → 建 Session → `Set-Cookie` → 302 `/api/auth/me`。**不要先 SELECT 查重**；直接 INSERT，`mysql_stmt_errno(stmt) == 1062`（`ER_DUP_ENTRY`）时返回 409，避免“查重后并发插入”竞态。
- `login`：解析 → `user`/`password` 任一为空则 400 → 按 `user_name` 查用户 → `verifyPassword` → 建 Session → `Set-Cookie` → 302。
- `logout`：读 Cookie → `remove` → `Set-Cookie` 过期 → 302 `/welcome.html`。
- `me`：读 Cookie → `get` → 按 `userId` 查库 → 渲染 HTML；Cookie 缺失/无效/过期或用户行不存在 → 401，DB/内部错误 → 500。
- 登录失败统一 401，错误页文案固定为“用户不存在或密码错误”。
- 所有 HTML 输出对 `user_name/hobby/sex/introduction/address` 做转义（`& < > " '`）。
- 读 Cookie 时同时尝试 `Cookie` 与 `cookie` 两种 header 名（现有 `getHeader` 大小写敏感）；按 `;` 分段、去空格、匹配 `session=`。
- 日志只记录路径与状态码；不要记录表单体、密码、Cookie/token。
- 连接用局部 RAII 包装：析构时调用 `globalDbPool->release(conn)`；`globalDbPool == nullptr` 或 `acquire()` 抛异常时直接 500。每个 `MYSQL_STMT` 也用局部 RAII（`mysql_stmt_close`），所有路径都不泄漏连接或语句。
- 响应统一设 `Content-Type: text/html; charset=utf-8`；HTML 内引用绝对路径 `/styles.css`。
- 成功跳转：`resp.setHeader("Location", "/api/auth/me")`（logout 用 `/welcome.html`）并 `setStatusCodeAndMessage(302)`。
- 登录/注册 Cookie：`resp.setHeader("Set-Cookie", "session=" + token + "; HttpOnly; SameSite=Lax; Path=/")`；注销：`session=; Max-Age=0; HttpOnly; SameSite=Lax; Path=/`。
- SELECT 结果中的 `hobby/sex/introduction/address` 可能为 NULL：绑定 `is_null`，为 NULL 时按空串渲染。

响应模板（错误页与资料页都用 `<main class="page"><section class="card">` 结构）：

```html
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8">
<title>错误</title><link rel="stylesheet" href="/styles.css"></head>
<body><main class="page"><section class="card">
<h1 class="title">操作失败</h1><p class="subtitle">%s</p>
<p class="footer-link"><a href="/login.html">返回登录</a></p>
</section></main></body></html>
```

资料页把 `%s` 换成用户名，列出 `爱好/性别/自我介绍/地址`，并加：

```html
<form action="/api/auth/logout" method="POST">
  <button class="btn btn--primary" type="submit">注销</button>
</form>
```

SQL（预处理语句）：

```sql
SELECT user_id, user_name, hashed_password, salt, hash_algo, iterations,
       hobby, sex, introduction, address
  FROM user_basic_info WHERE user_name = ?;

SELECT user_id, user_name, hashed_password, salt, hash_algo, iterations,
       hobby, sex, introduction, address
  FROM user_basic_info WHERE user_id = ?;

INSERT INTO user_basic_info
  (user_name, hashed_password, salt, hash_algo, iterations,
   hobby, sex, introduction, address)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
```

校验规则：

- 必填：`user`、`password`、`confirm`、`hobby`、`sex`、`address`；`introduction` 可缺省，缺省按空串处理。
- `user`：`[A-Za-z0-9_]{3,32}`。
- `password`：长度 ≥8，且含大写、小写、非字母数字字符各至少一个。
- `confirm == password`。
- `hobby ∈ {运动, 唱歌, 玩游戏, 学习}`；`sex ∈ {boy, girl}`；`address ∈ {北京, 四川, 上海, 武汉}`。
- `introduction.size() <= 1024`。

`router/router.cpp`：加 `#include "../handler/user_handler.h"`，在算出 `url`/`isCgi` 之后、`switch (method)` 之前插入（这样 `PUT`/`DELETE` 等标准方法也会进入 handler 并得到 405，而不是被 `default` 判成 400）：

```cpp
if (url.rfind("/api/auth/", 0) == 0)
{
    return new http_user_handler();
}
```

`handler/CMakeLists.txt`：

```cmake
add_library(http_hanlder SHARED handler.cpp handler_simple.cpp handler_cgi.cpp cgi_exec_args.cpp user_handler.cpp)
target_link_libraries(http_hanlder auth db)
```

- [ ] **Step 4: 运行，确认通过**

```bash
cmake --build build -j4
bash tests/user_integration.sh
```

Expected: `user integration test PASS`。

- [ ] **Step 5: 提交**

```bash
git add handler/user_handler.h handler/user_handler.cpp handler/CMakeLists.txt router/router.cpp tests/user_integration.sh
git commit -m "feat: add user auth handler and routes"
```

---

### Task 9: 前端表单

**Files:**
- Modify: `web/login.html`
- Modify: `web/register.html`
- Create: `tests/frontend_test.sh`

**Interfaces:**
- Consumes: 端点 `/api/auth/login`、`/api/auth/register`（Task 8）。
- Produces: 表单指向新端点；注册页有 `confirm` 输入。

- [ ] **Step 1: 写失败测试 `tests/frontend_test.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
grep -q 'action="/api/auth/login"' "$root/web/login.html" || { echo "FAIL: login action"; exit 1; }
grep -q 'action="/api/auth/register"' "$root/web/register.html" || { echo "FAIL: register action"; exit 1; }
grep -q 'name="confirm"' "$root/web/register.html" || { echo "FAIL: confirm field"; exit 1; }
grep -q 'id="user".*maxlength="32"' "$root/web/register.html" || { echo "FAIL: username maxlength"; exit 1; }
grep -q 'name="hobby"' "$root/web/register.html" || { echo "FAIL: hobby field"; exit 1; }
grep -q 'name="sex"' "$root/web/register.html" || { echo "FAIL: sex field"; exit 1; }
grep -q 'name="introduction"' "$root/web/register.html" || { echo "FAIL: introduction field"; exit 1; }
grep -q 'name="address"' "$root/web/register.html" || { echo "FAIL: address field"; exit 1; }
echo "frontend test PASS"
```

- [ ] **Step 2: 运行，确认失败**

```bash
bash tests/frontend_test.sh
```

Expected: `FAIL: login action`（当前指向 `register.cgi`）。

- [ ] **Step 3: 修改两个 HTML**

- `login.html`：`action="/api/auth/login"`。
- `register.html`：`action="/api/auth/register"`；`user` 的 `maxlength` 改为 32；在密码字段后新增 `confirm` 密码框（`type="password"`、`name="confirm"`、`autocomplete="new-password"`、`required`）；保留 `hobby/sex/introduction/address`。

- [ ] **Step 4: 运行，确认通过**

```bash
bash tests/frontend_test.sh
```

Expected: `frontend test PASS`。

- [ ] **Step 5: 提交**

```bash
git add web/login.html web/register.html tests/frontend_test.sh
git commit -m "feat: point auth forms at new endpoints"
```

---

### Task 10: 全量验证与清理

**Files:**
- Modify: 无（仅运行与清理）

**Interfaces:**
- Consumes: 前 9 个任务的产物。
- Produces: 全绿测试结果；临时文件清理。

- [ ] **Step 1: 全量构建**

```bash
cmake --build build -j4
```

Expected: 无错误。

- [ ] **Step 2: 运行全部单元测试**

```bash
g++ -std=c++11 -I. tests/form_parser_test.cpp auth/form_parser.cpp -o /tmp/form_parser_test && /tmp/form_parser_test
g++ -std=c++11 -I. tests/password_hash_test.cpp auth/password_hash.cpp -lcrypto -o /tmp/password_hash_test && /tmp/password_hash_test
g++ -std=c++11 -I. tests/session_store_test.cpp auth/session_store.cpp -lcrypto -lpthread -o /tmp/session_store_test && /tmp/session_store_test
g++ -std=c++11 -I. -I/usr/include/mysql -I/usr/include/mariadb tests/db_pool_test.cpp db/db_pool.cpp -lmysqlclient -lpthread -o /tmp/db_pool_test && /tmp/db_pool_test
g++ -std=c++11 -I. tests/status_code_test.cpp http_messages/http_response.cpp http_messages/http_response_code.cpp -o /tmp/status_code_test && /tmp/status_code_test
g++ -std=c++11 -I. tests/http_body_test.cpp \
    http_utils/http_request_parser.cpp http_messages/http_request.cpp \
    http_messages/http_response.cpp http_messages/http_response_code.cpp \
    http_messages/http_request_method.cpp -o /tmp/http_body_test && /tmp/http_body_test
```

Expected: 六个测试都输出 `PASS`。

- [ ] **Step 3: 运行全部集成测试**

```bash
bash tests/db_startup_test.sh
bash tests/user_integration.sh
bash tests/frontend_test.sh
bash tests/cgi_integration.sh
```

Expected: 四个脚本都输出 `PASS`（`cgi_integration.sh` 是回归测试，确认旧 CGI 未被破坏）。

- [ ] **Step 4: 清理测试产物**

```bash
rm -f /tmp/form_parser_test /tmp/password_hash_test /tmp/session_store_test \
      /tmp/db_pool_test /tmp/status_code_test /tmp/http_body_test \
      /tmp/myhttp-db-good.cfg /tmp/myhttp-db-bad.cfg /tmp/myhttp-db-test.log \
      /tmp/myhttp-db-app.log \
      /tmp/myhttp-user-test.cfg /tmp/myhttp-user-body.html \
      /tmp/myhttp-user-cookie.txt /tmp/myhttp-user-cookie2.txt \
      /tmp/myhttp-user-test.log /tmp/myhttp-user-app.log \
      devel/rtmsg.log
mysql -u myweb -pchange-me -e "DROP DATABASE IF EXISTS mywebserver_test"
```

Expected: 无残留临时文件；测试库已删除。

- [ ] **Step 5: 提交（如有改动）**

```bash
git status --short
git add -A
git commit -m "test: verify user management end to end"
```
