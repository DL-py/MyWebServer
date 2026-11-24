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

验证：

```bash
bash tests/hexo_hosting.sh
g++ -std=c++11 -Isrc/http_utils tests/gzip_cache_test.cpp src/http_utils/gzip.cpp -lz -o /tmp/gzip_cache_test && /tmp/gzip_cache_test
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
