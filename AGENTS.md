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
bash tests/cgi_integration.sh
g++ -std=c++11 -I. tests/cgi_exec_args_test.cpp handler/cgi_exec_args.cpp -o /tmp/cgi_exec_args_test && /tmp/cgi_exec_args_test
```

新环境重配（本机无系统 libconfig++，仅 `devel/lib/vendor/`，该目录被 git 忽略）：

```bash
cmake -S . -B build \
  -DCMAKE_CXX_FLAGS=-I$PWD/devel/lib/vendor/include \
  -DCMAKE_EXE_LINKER_FLAGS=-L$PWD/devel/lib/vendor
```

查看/结束服务（进程名为 `leader`）：

```bash
ss -ltnp | grep :6380
fuser -k 6380/tcp
```
