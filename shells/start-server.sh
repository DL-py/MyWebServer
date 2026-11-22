#!/usr/bin/env bash
# 启动 MyWebServer 服务。
#
# 做三件事：
#   1. 确认 devel/myhttp 已构建；
#   2. 若 devel/common.cfg 不存在，从 src/common.cfg.example 生成；
#   3. 设置共享库搜索路径并启动服务。
#
# 用法：
#   ./shells/start-server.sh [myhttp 参数]
#
# 示例：
#   ./shells/start-server.sh
#   ./shells/start-server.sh --config /tmp/myhttp.cfg
#   MYHTTP_CONFIG=/tmp/myhttp.cfg ./shells/start-server.sh
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
devel="$root/devel"
template="$root/src/common.cfg.example"
default_config="$devel/common.cfg"

usage() {
    cat <<'EOF'
用法: start-server.sh [myhttp 参数]

选项:
  -h, --help              显示本帮助
  --config <path>         指定配置文件（默认 devel/common.cfg）
  --config=<path>         同上

环境变量:
  MYHTTP_CONFIG           默认配置路径，可被 --config 覆盖
EOF
}

case "${1:-}" in
    -h|--help)
        usage
        exit 0
        ;;
esac

if [[ ! -x "$devel/myhttp" ]]; then
    echo "错误：未找到 $devel/myhttp" >&2
    echo "请先构建：cmake --build build -j4" >&2
    exit 1
fi

if [[ ! -f "$template" ]]; then
    echo "错误：缺少配置模板 $template" >&2
    exit 1
fi

if [[ ! -f "$default_config" ]]; then
    cp "$template" "$default_config"
    echo "已从 $template 生成 $default_config" >&2
fi

cd "$devel"

# 项目共享库位于 lib/<module>/，vendor 库位于 lib/vendor/。
project_libs="$(find lib -type d -not -path 'lib/vendor*' | paste -sd:)"
export LD_LIBRARY_PATH="${project_libs}:$(pwd)/lib/vendor${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

echo "启动 MyWebServer（工作目录：$devel）..." >&2
exec ./myhttp "$@"
