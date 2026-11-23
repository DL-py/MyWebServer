#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
port="${PORT:-6399}"
tmp="$root/tests/.tmp-hexo"
rm -rf "$tmp"; mkdir -p "$tmp/site/about" "$tmp/site/css" "$tmp/site/.git" "$tmp/site/2025/09/06/中文文章"
printf '<!DOCTYPE html><html><body>fixture-home</body></html>' > "$tmp/site/index.html"
printf '<!DOCTYPE html><html><body>fixture-about</body></html>' > "$tmp/site/about/index.html"
printf '<!DOCTYPE html><html><body>fixture-post</body></html>' > "$tmp/site/2025/09/06/中文文章/index.html"
printf 'body{}' > "$tmp/site/css/style.css"
printf '{"url":"/x"}' > "$tmp/site/search.json"
printf '<?xml version="1.0"?><urlset/>' > "$tmp/site/sitemap.xml"
printf '<!DOCTYPE html><html><body>fixture-404</body></html>' > "$tmp/site/404.html"
printf '[core]\n' > "$tmp/site/.git/config"
cat > "$tmp/common.cfg" <<CFG
conn = { ip = "127.0.0.1"; port = $port; };
log = { log_file_name = "$tmp/rtmsg.log"; log_level = "LOG_INFO"; max_log_size = "1M"; period_timeout = "20m"; };
asset = { doc_root = "$tmp/site"; default_page = "index.html"; };
CFG

libs="$(find "$root/devel/lib" -type d -not -path '*/vendor*' | paste -sd:):$root/devel/lib/vendor"
( cd "$tmp" && LD_LIBRARY_PATH="$libs" exec "$root/devel/myhttp" --config "$tmp/common.cfg" ) &
pid=$!
trap 'kill $pid 2>/dev/null || true; rm -rf "$tmp"' EXIT
for _ in $(seq 1 50); do
  curl -sf "http://127.0.0.1:$port/" >/dev/null 2>&1 && break
  sleep 0.2
done

expect() { [ "$2" = "$3" ] || { echo "FAIL $1: got '$2' want '$3'"; exit 1; }; echo "ok $1"; }
code_of() { curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port$1"; }
ctype_of() { curl -s -o /dev/null -w '%{content_type}' "http://127.0.0.1:$port$1"; }
body_of() { curl -s "http://127.0.0.1:$port$1"; }

expect "home status" "$(code_of /)" "200"
expect "home body" "$(body_of /)" "<!DOCTYPE html><html><body>fixture-home</body></html>"
expect "encoded path" "$(body_of '/2025/09/06/%E4%B8%AD%E6%96%87%E6%96%87%E7%AB%A0/')" "<!DOCTYPE html><html><body>fixture-post</body></html>"
expect "about ctype" "$(ctype_of /about/)" "text/html; charset=utf-8"
expect "css ctype" "$(ctype_of /css/style.css)" "text/css"
expect "json ctype" "$(ctype_of /search.json)" "application/json"
expect "xml ctype" "$(ctype_of /sitemap.xml)" "application/xml"
expect "query ignored" "$(body_of '/index.html?v=1')" "<!DOCTYPE html><html><body>fixture-home</body></html>"
expect "dir redirect" "$(code_of /about)" "301"
expect "redirect location" "$(curl -s -o /dev/null -w '%{redirect_url}' "http://127.0.0.1:$port/about?x=1")" "http://127.0.0.1:$port/about/?x=1"
expect "missing status" "$(code_of /nope)" "404"
expect "missing body" "$(body_of /nope)" "<!DOCTYPE html><html><body>fixture-404</body></html>"
expect "dot path" "$(code_of /.git/config)" "403"
rm "$tmp/site/404.html"
expect "no 404 page" "$(code_of /nope2)" "404"
expect "no 404 page empty" "$(body_of /nope2)" ""
echo "ALL PASS"
