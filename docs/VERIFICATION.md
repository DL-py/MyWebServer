# 验证与测试手册

## 1. 文档定位

本文件面向开发者与维护者，用于验证服务的行为是否符合预期。

- 终端用户如何访问站点，见 [`README.md`](../README.md)
- 架构与模块设计，见 [`MyWebServer.md`](MyWebServer.md)
- 本文件覆盖：功能冒烟、HTTP 协议行为、缓存与压缩、日志与退出、单元测试
- 非目标：压力测试、TLS/HTTPS、容量评估（当前搁置）

## 2. 验证环境与前置条件

### 2.1 前置条件

- 已完成构建，`devel/common.cfg` 存在
- `asset.doc_root` 指向可读的静态站点目录
- 注册/登录相关用例需要 `db.enabled = true` 且已执行 `sql/schema.sql`
- 服务已在 `6380` 监听：

```bash
ss -ltnp | grep :6380
```

### 2.2 启动与停止

```bash
./shells/start-server.sh
```

另开终端执行验证。停止服务：

```bash
kill -TERM "$(pgrep -x leader | head -1)"
```

日志位置：`devel/rtmsg.log`。

### 2.3 通用变量

以下命令假设在同一 shell 中执行：

```bash
export BASE=http://127.0.0.1:6380
export DOC_ROOT=$(sed -n 's/.*doc_root *= *"\(.*\)".*/\1/p' devel/common.cfg)
```

端口或站点根目录不同时，按 `devel/common.cfg` 调整。

<a id="conventions"></a>
## 3. 通用约定

| 概念 | 说明 |
|------|------|
| `curl` 退出码 | `0` 成功；`7` 连接失败；`22` HTTP 4xx/5xx（配合 `-f`） |
| `http_code=000` | 没有收到 HTTP 响应，通常是服务未启动或端口不通，**不是**业务状态码 |
| `-fsS` | HTTP 错误或连接失败时返回非零并打印错误，避免静默通过 |
| `-o /dev/null` | 丢弃响应体，只关心状态码/字节数 |
| 期望 4xx 的用例 | 不要加 `-f`，否则 curl 会按失败返回 |
| 判定失败先看 | `devel/rtmsg.log`、`ss -ltnp`、`curl -v` |

排查连接问题的示例：

```bash
curl -fsS -o /dev/null -w 'home: %{http_code}\n' "$BASE/" || echo "curl exit=$?"
```

若输出 `000` 且退出码为 `7`，先确认服务是否启动，而不是继续排查业务逻辑。

<a id="cases"></a>
## 4. 验证用例

每个用例按「目的 → 操作 → 期望 → 说明」组织，标题前的编号即用例 ID；交叉引用一律使用可跳转链接，不写纯文本用例名。

| 前缀 | 含义 | 覆盖 |
|------|------|------|
| `S` | Smoke，冒烟 | S1 首页、S2 `Date`、S3 404 |
| `H` | HEAD，无正文响应 | H1 |
| `R` | Route，路径与状态码 | R1 301、R2 403 |
| `C` | Connection，连接复用 | C1 keep-alive、C2 关闭 |
| `E` | ETag，条件请求 | E1 identity 304、E2 gzip 304 |
| `G` | Gzip，压缩协商 | G1–G3 |
| `L` | Log，日志 | L1、L2 |
| `Q` | Quit，退出 | Q1、Q2 |

编号约定：新增用例追加编号，不复用已删除的编号（原 `R3` 已并入 `S3`）。

### 4.1 冒烟

<a id="case-s1"></a>
#### S1 首页可访问

```bash
curl -fsS -o /dev/null -w 'home: %{http_code}\n' "$BASE/"
```

期望：`home: 200`。

说明：若为 `000`，见 [§3 通用约定](#conventions)；若为 `404`，检查 `asset.doc_root` 与 `asset.default_page`。

<a id="case-s2"></a>
#### S2 `Date` 头存在且可解析

```bash
date_str=$(curl -fsSI "$BASE/" | tr -d '\r' | sed -n 's/^[Dd]ate: //p')
echo "Date: $date_str"
date -d "$date_str" +%s >/dev/null
```

期望：打印 IMF-fixdate 格式时间，`date -d` 返回 0。

说明：`Date` 缺失或格式错误属于协议回归。

<a id="case-s3"></a>
#### S3 不存在的路径

```bash
curl -s -o /dev/null -w 'missing: %{http_code}\n' "$BASE/nope-xyz"
```

期望：`missing: 404`。

说明：站点提供 `404.html` 时仍返回 404 状态码，正文为自定义页面。

### 4.2 HTTP 协议行为

<a id="case-h1"></a>
#### H1 HEAD 与 GET 长度一致

```bash
head_len=$(curl -fsSI "$BASE/" | tr -d '\r' | sed -n 's/^[Cc]ontent-[Ll]ength: //p')
get_len=$(curl -fsS -o /dev/null -w '%{size_download}' "$BASE/")
echo "head=$head_len get=$get_len"
curl -fsS -o /dev/null -w 'head_bytes: %{size_download}\n' -I "$BASE/"
```

期望：`head_len` 与 `get_len` 相等，`head_bytes: 0`。

说明：HEAD 必须返回与 GET 相同的头，但不发送正文。

<a id="case-r1"></a>
#### R1 目录缺少尾斜杠返回 301

```bash
curl -s -o /dev/null -D - "$BASE/2025/09/06?from=test" | tr -d '\r' | grep -Ei '^(HTTP/|Location:)'
```

期望：`301`，`Location` 指向带尾斜杠的路径，查询串保留。

说明：示例路径以实际站点为准；不存在的目录会返回 404。

<a id="case-r2"></a>
#### R2 点路径返回 403

```bash
curl -s -o /dev/null -w 'dotpath: %{http_code}\n' "$BASE/.git/config"
```

期望：`dotpath: 403`。

<a id="case-c1"></a>
#### C1 显式 keep-alive 可复用连接

```bash
BASE="$BASE" python3 - <<'PY'
import os, socket, urllib.parse

u = urllib.parse.urlparse(os.environ["BASE"])
s = socket.create_connection((u.hostname, u.port or 80), timeout=5)

def read_response(sock):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(65536)
        if not chunk:
            raise SystemExit("连接在响应头前关闭")
        data += chunk
    head, rest = data.split(b"\r\n\r\n", 1)
    length = 0
    for line in head.split(b"\r\n")[1:]:
        if line.lower().startswith(b"content-length:"):
            length = int(line.split(b":", 1)[1].strip())
    while len(rest) < length:
        rest += sock.recv(65536)
    return int(head.split(b" ")[1])

for _ in range(2):
    s.sendall(b"GET / HTTP/1.1\r\nHost: t\r\nConnection: Keep-Alive\r\n\r\n")
    assert read_response(s) == 200
print("keep-alive: 两次请求复用成功")
PY
```

期望：`keep-alive: 两次请求复用成功`。

<a id="case-c2"></a>
#### C2 无 `Connection` 头时响应后关闭

```bash
BASE="$BASE" python3 - <<'PY'
import os, socket, urllib.parse

u = urllib.parse.urlparse(os.environ["BASE"])
s = socket.create_connection((u.hostname, u.port or 80), timeout=5)
s.sendall(b"GET / HTTP/1.1\r\nHost: t\r\n\r\n")

data = b""
while b"\r\n\r\n" not in data:
    data += s.recv(65536)
head, rest = data.split(b"\r\n\r\n", 1)
length = 0
for line in head.split(b"\r\n")[1:]:
    if line.lower().startswith(b"content-length:"):
        length = int(line.split(b":", 1)[1].strip())
while len(rest) < length:
    rest += s.recv(65536)

s.settimeout(3)
try:
    closed = s.recv(1) == b""
except ConnectionResetError:
    closed = True
except socket.timeout:
    closed = False
assert closed, "服务端在无 keep-alive 时未关闭连接"
print("close: 响应后连接已关闭")
PY
```

期望：`close: 响应后连接已关闭`。

说明：本服务只对显式 `Connection: keep-alive` 复用连接，HTTP/1.1 默认持久连接未实现。

<a id="case-e1"></a>
#### E1 identity 表示命中 304

```bash
etag=$(curl -fsSI "$BASE/" | tr -d '\r' | sed -n 's/^[Ee][Tt]ag: //p')
curl -s -o /dev/null -w '304: %{http_code}\n' -H "If-None-Match: $etag" "$BASE/"
```

期望：`304: 304`。

<a id="case-e2"></a>
#### E2 gzip 变体命中 304

```bash
url="$BASE/js/libs/mermaid.min.js"
etag_gzip=$(curl -fsSI -H 'Accept-Encoding: gzip' "$url" | tr -d '\r' | sed -n 's/^[Ee][Tt]ag: //p')
curl -s -o /dev/null -w 'gzip304: %{http_code}\n' \
  -H 'Accept-Encoding: gzip' -H "If-None-Match: $etag_gzip" "$url"
```

期望：`gzip304: 304`。

说明：URL 请替换为实际站点中会被压缩的资源。

### 4.3 压缩与缓存

<a id="case-g1"></a>
#### G1 gzip 协商

```bash
url="$BASE/js/libs/mermaid.min.js"
curl -fsS -D /tmp/myweb-gzip.h -o /dev/null -H 'Accept-Encoding: gzip' "$url"
grep -i '^content-encoding: gzip' /tmp/myweb-gzip.h
grep -i '^vary:.*Accept-Encoding' /tmp/myweb-gzip.h
```

期望：两个 `grep` 都能匹配。

说明：文件需大于 `asset.gzip_min_size`，且 `asset.gzip = true`。

<a id="case-g2"></a>
#### G2 小于阈值不压缩

```bash
small=$(find "$DOC_ROOT" -type f -size -1k \( -name '*.css' -o -name '*.js' -o -name '*.html' \) | head -1)
if [ -n "$small" ]; then
  echo "small=$small"
  curl -fsS -D - -o /dev/null -H 'Accept-Encoding: gzip' "$BASE/${small#"$DOC_ROOT"/}" | grep -i '^content-encoding:' || echo "no content-encoding (expected)"
else
  echo "skip: 未找到小于 gzip_min_size 的文本文件"
fi
```

期望：不出现 `Content-Encoding: gzip`。

说明：找不到小于阈值的文本文件时跳过本用例。

<a id="case-g3"></a>
#### G3 gzip 与 identity 的 ETag 不同

```bash
url="$BASE/js/libs/mermaid.min.js"
etag_id=$(curl -fsSI "$url" | tr -d '\r' | sed -n 's/^[Ee][Tt]ag: //p')
etag_gz=$(curl -fsSI -H 'Accept-Encoding: gzip' "$url" | tr -d '\r' | sed -n 's/^[Ee][Tt]ag: //p')
echo "identity=$etag_id gzip=$etag_gz"
[ -n "$etag_id" ] && [ -n "$etag_gz" ] && [ "$etag_id" != "$etag_gz" ]
```

期望：两个 ETag 均非空且不相等（gzip 变体带 `-gzip` 后缀）。

### 4.4 日志与退出

<a id="case-l1"></a>
#### L1 ACCESS 日志

```bash
curl -fsS -o /dev/null "$BASE/"
grep 'ACCESS ' devel/rtmsg.log | tail -n 3
```

期望：每个请求一行，格式：

```text
ACCESS <方法> <URL> <状态码> <响应体字节> <耗时>ms
```

说明：URL 取自请求行原始路径，超过 512 字节会截断；控制字符会被替换为 `?`。

<a id="case-l2"></a>
#### L2 关闭 ACCESS（可选）

将 `devel/common.cfg` 中 `log.access_log` 改为 `false`，重启服务后重复 [L1](#case-l1)。

期望：不再新增 `ACCESS` 行。验证完成后恢复为 `true`。

<a id="case-q1"></a>
#### Q1 `SIGTERM` 优雅退出

```bash
./shells/start-server.sh >/tmp/myweb.log 2>&1 &
server=$!
for _ in $(seq 1 50); do curl -fsS -o /dev/null "$BASE/" 2>/dev/null && break; sleep 0.2; done
kill -TERM "$server"
wait "$server"
echo "exit=$?"
tail -n 1 devel/rtmsg.log
```

期望：`exit=0`，最后一行包含 `server stopped`。

说明：执行前确保 6380 没有其他实例。

<a id="case-q2"></a>
#### Q2 空闲连接下的退出耗时

在第 [Q1](#case-q1) 的基础上，保持一个空闲 keep-alive 连接再发送 `SIGTERM`，测量从发信号到进程退出的时间：

```bash
cd "$(git rev-parse --show-toplevel)"
export BASE=http://127.0.0.1:6380
pgrep -x leader >/dev/null && { echo "先停止已有实例"; exit 1; }

./shells/start-server.sh >/tmp/myweb.log 2>&1 &
server=$!
for _ in $(seq 1 50); do curl -fsS -o /dev/null "$BASE/" 2>/dev/null && break; sleep 0.2; done

# 建一个空闲 keep-alive 连接，读完响应后保持 10 秒不关闭
BASE="$BASE" python3 - <<'PY' &
import os, socket, time, urllib.parse
u = urllib.parse.urlparse(os.environ["BASE"])
s = socket.create_connection((u.hostname, u.port or 80), timeout=5)
s.sendall(b"GET / HTTP/1.1\r\nHost: t\r\nConnection: keep-alive\r\n\r\n")
data = b""
while b"\r\n\r\n" not in data:
    data += s.recv(65536)
head, rest = data.split(b"\r\n\r\n", 1)
length = int(next(l for l in head.split(b"\r\n")
              if l.lower().startswith(b"content-length:")).split(b":", 1)[1])
while len(rest) < length:
    rest += s.recv(65536)
time.sleep(10)
PY
idle=$!
sleep 0.5

start=$(date +%s%N)
kill -TERM "$server"
wait "$server"
code=$?
end=$(date +%s%N)
echo "exit=$code elapsed_ms=$(( (end - start) / 1000000 ))"
tail -n 1 devel/rtmsg.log

kill "$idle" 2>/dev/null
wait "$idle" 2>/dev/null
true
```

期望：

- `exit=0`
- `elapsed_ms` 约 300–400（本机实测 309）
- 日志末行包含 `server stopped, remaining connections: 0`

说明：

- 空闲 keep-alive 连接不产生事件，drain 循环连续 3 次空轮询（每次 100ms）后提前结束
- 若 `elapsed_ms` 接近 5000，说明当时有在途响应，不属于空闲连接场景
- 有在途响应时最多等待 5 秒，慢客户端也不应导致响应被截断
- 执行前确保 6380 没有其他实例，脚本已做检查

### 4.5 单元测试

仅在本地 `tests/` 存在时执行：

```bash
g++ -std=c++11 -Isrc/http_utils tests/gzip_cache_test.cpp src/http_utils/gzip.cpp -lz -o /tmp/gzip_cache_test && /tmp/gzip_cache_test
g++ -std=c++11 -Isrc/http_utils tests/http_protocol_test.cpp src/http_utils/http_protocol.cpp -o /tmp/http_protocol_test && /tmp/http_protocol_test
g++ -std=c++11 -Isrc/http_utils tests/etag_cache_bound_test.cpp src/http_utils/etag_cache.cpp -o /tmp/etag_bound_test && /tmp/etag_bound_test
```

| 测试 | 覆盖内容 | 期望输出 |
|------|----------|----------|
| `gzip_cache_test` | 压缩、`Accept-Encoding` 协商、缓存淘汰 | `gzip cache ok` |
| `http_protocol_test` | `Date`、RFC 850/asctime、Connection token | `http protocol ok` |
| `etag_cache_bound_test` | ETag 索引上限与淘汰 | `etag bound ok` |

`tests/` 不纳入版本库，缺失时跳过本节。

## 5. 回归矩阵

每次改动 HTTP 行为后按表执行（编号释义见 [§4](#cases)）：

| 项目 | 用例 | 期望 |
|------|------|------|
| 首页 | [S1](#case-s1) | `200` |
| `Date` | [S2](#case-s2) | 可解析 |
| 404 | [S3](#case-s3) | `404` |
| HEAD | [H1](#case-h1) | 长度一致、0 字节正文 |
| 301 | [R1](#case-r1) | `301` + `Location` |
| 403 | [R2](#case-r2) | `403` |
| keep-alive | [C1](#case-c1) | 连接复用 |
| 无 keep-alive | [C2](#case-c2) | 响应后关闭 |
| 304 identity | [E1](#case-e1) | `304` |
| 304 gzip | [E2](#case-e2) | `304` |
| gzip | [G1](#case-g1) | `Content-Encoding: gzip` |
| 小文件 | [G2](#case-g2) | 不压缩 |
| 变体 ETag | [G3](#case-g3) | 两者不同 |
| ACCESS | [L1](#case-l1) | 有日志行 |
| SIGTERM | [Q1](#case-q1) | 退出码 0 |
| 空闲退出 | [Q2](#case-q2) | 约 300–400ms |

## 6. 常见失败与排查

| 现象 | 可能原因 | 检查 |
|------|----------|------|
| `000`，curl 退出码 7 | 服务未启动、端口不匹配、监听地址错误 | `ss -ltnp \| grep :6380`、`devel/common.cfg` 的 `conn.port` |
| 首页 `404` | `doc_root` 或 `default_page` 配错 | 配置值与目录权限 |
| 点路径未返回 `403` | URL 被代理或客户端改写 | 直接访问 `$BASE/.git/config` |
| gzip 不生效 | 文件小于阈值、`asset.gzip=false`、客户端未声明 | [G1](#case-g1)/[G2](#case-g2)、配置项 |
| 304 不命中 | ETag 变化、文件大小或 mtime 变化 | 对比响应头 ETag |
| 无 ACCESS 行 | `access_log=false`、日志级别过滤、路径不对 | 配置与 `devel/rtmsg.log` |
| SIGTERM 退出慢 | 有在途请求或慢客户端 | 日志中的 `remaining connections` |
| 端口被占用 | 旧实例未退出 | `pgrep -x leader`、`fuser -k 6380/tcp` |

## 7. 验证记录模板

```text
日期：
commit：
环境：OS / 构建方式 / 配置差异
执行人：

通过项：
失败项：
日志片段：
结论：
```

## 8. 维护约定

- HTTP 协议行为、配置项或退出流程变化时，同步更新本文件
- 用户访问说明只放在 `README.md`，不要写回本文件
- 压力测试与 TLS 恢复时，另建文档，不混入功能验证
