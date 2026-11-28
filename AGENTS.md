# AGENTS.md

## 1. 常用命令

构建：

```bash
cmake --build build -j4
```

启动（仓库根目录）：

```bash
./shells/start-server.sh
```

验证（服务已在 6380 监听）：

```bash
# Date 头存在且可被 date 解析
date_str=$(curl -sfI http://127.0.0.1:6380/ | tr -d '\r' | sed -n 's/^[Dd]ate: //p')
date -d "$date_str" +%s >/dev/null
# 首页 200、gzip 200（约 745971 B）、带 ETag 重验证 304、HEAD 200 且 0 字节
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:6380/
curl -s -o /dev/null -w '%{http_code} %{size_download}\n' -H 'Accept-Encoding: gzip' http://127.0.0.1:6380/js/libs/mermaid.min.js
etag=$(curl -sI -H 'Accept-Encoding: gzip' http://127.0.0.1:6380/js/libs/mermaid.min.js | tr -d '\r' | grep -i '^etag:' | cut -d' ' -f2-)
curl -s -o /dev/null -w '%{http_code}\n' -H 'Accept-Encoding: gzip' -H "If-None-Match: $etag" http://127.0.0.1:6380/js/libs/mermaid.min.js
curl -s -o /dev/null -w '%{http_code} %{size_download}\n' -I http://127.0.0.1:6380/
```

单元测试（本地 `tests/`，不纳入版本库）：

```bash
g++ -std=c++11 -Isrc/http_utils tests/gzip_cache_test.cpp src/http_utils/gzip.cpp -lz -o /tmp/gzip_cache_test && /tmp/gzip_cache_test
g++ -std=c++11 -Isrc/http_utils tests/http_protocol_test.cpp src/http_utils/http_protocol.cpp -o /tmp/http_protocol_test && /tmp/http_protocol_test
g++ -std=c++11 -Isrc/http_utils tests/etag_cache_bound_test.cpp src/http_utils/etag_cache.cpp -o /tmp/etag_bound_test && /tmp/etag_bound_test
```

系统依赖（Ubuntu）：

```bash
sudo apt-get install -y build-essential cmake libconfig++-dev libconfig-dev libmysqlclient-dev libssl-dev zlib1g-dev
```

首次构建：

```bash
cmake -S . -B build
cmake --build build -j4
```

查看/结束服务（进程名为 `leader`）：

```bash
ss -ltnp | grep :6380
fuser -k 6380/tcp
```
