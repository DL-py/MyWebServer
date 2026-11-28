# AGENTS.md

本文件只记录 Agent 在本仓库工作所需的约束与最短命令；安装、运行与配置的完整说明见 `README.md`，验证流程见 `docs/VERIFICATION.md`。

## 仓库约定

- master 不自动暂存或提交，除非用户明确要求
- `tests/` 只作本地验证，不纳入版本库；不要为了跑测试把测试文件加入 git
- 中间设计、计划与审查记录放在 `superpowers/` 或 `.superpowers/`，均已忽略
- `/home/dl/DL-py.github.io`（hexo 站点根目录）只读，不要修改
- 修改行为后同步 `docs/MyWebServer.md`，保持文档与代码一致

## 快速命令

构建：

```bash
cmake --build build -j4
```

首次构建、依赖安装、配置说明见 `README.md`。

启动（仓库根目录）：

```bash
./shells/start-server.sh
```

验证：按 `docs/VERIFICATION.md` 执行；提交前至少完成一次 clean build 且无新增告警：

```bash
cmake --build build -j4 --clean-first
```

## 环境要点

- 服务端口 `6380`，进程名 `leader`
- 运行日志：`devel/rtmsg.log`
- 本地配置：`devel/common.cfg`（首次启动由 `src/common.cfg.example` 生成）
- 优雅停止：`kill -TERM "$(pgrep -x leader | head -1)"`；`fuser -k 6380/tcp` 仅作无响应兜底
- 测试或手工验证结束后不要保留 6380 上的运行实例
- `Connection: keep-alive` 为显式 opt-in；无该请求头时响应后关闭连接
- 未内置 TLS，生产环境建议由反向代理终止
