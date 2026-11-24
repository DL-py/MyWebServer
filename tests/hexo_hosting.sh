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
printf '{"name":"x"}' > "$tmp/site/site.webmanifest"
printf 'eot' > "$tmp/site/font.eot"
printf 'otf' > "$tmp/site/font.otf"
printf '\0asm' > "$tmp/site/mod.wasm"
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
hdr_of() { curl -s -D - -o /dev/null "http://127.0.0.1:$port$1" | tr -d '\r' | grep -i "^$2:" | cut -d' ' -f2-; }
head_of() { curl -s -I "http://127.0.0.1:$port$1" | tr -d '\r'; }

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
expect "head status" "$(curl -s -I -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/")" "200"
expect "head body bytes" "$(curl -s -I -o /dev/null -w '%{size_download}' "http://127.0.0.1:$port/")" "0"
expect "head length" "$(head_of / | grep -i '^content-length:' | cut -d' ' -f2)" "$(stat -c %s "$tmp/site/index.html")"
expect "head 404 status" "$(curl -s -I -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/nope3")" "404"
expect "head 404 length" "$(head_of /nope3 | grep -i '^content-length:' | cut -d' ' -f2)" "$(stat -c %s "$tmp/site/404.html")"
expect "head redirect" "$(curl -s -I -o /dev/null -w '%{http_code} %{size_download}' "http://127.0.0.1:$port/about")" "301 0"
etag=$(hdr_of /css/style.css etag || true)
expect "etag present" "$([ -n "$etag" ] && echo yes || echo no)" "yes"
expect "etag stable" "$(hdr_of /css/style.css etag || true)" "$etag"
expect "304 status" "$(curl -s -o /dev/null -w '%{http_code}' -H "If-None-Match: $etag" "http://127.0.0.1:$port/css/style.css")" "304"
expect "304 body" "$(curl -s -H "If-None-Match: $etag" "http://127.0.0.1:$port/css/style.css" | wc -c)" "0"
expect "304 keeps etag" "$(curl -s -D - -o /dev/null -H "If-None-Match: $etag" "http://127.0.0.1:$port/css/style.css" | tr -d '\r' | grep -i '^etag:' | cut -d' ' -f2-)" "$etag"
expect "wildcard 304" "$(curl -s -o /dev/null -w '%{http_code}' -H 'If-None-Match: *' "http://127.0.0.1:$port/css/style.css")" "304"
expect "list 304" "$(curl -s -o /dev/null -w '%{http_code}' -H "If-None-Match: \"bogus\", $etag" "http://127.0.0.1:$port/css/style.css")" "304"
expect "weak prefix 304" "$(curl -s -o /dev/null -w '%{http_code}' -H "If-None-Match: W/$etag" "http://127.0.0.1:$port/css/style.css")" "304"
expect "html cache" "$(hdr_of /about/ cache-control || true)" "no-cache"
expect "css cache" "$(hdr_of /css/style.css cache-control || true)" "public, max-age=600"
expect "ims 304" "$(curl -s -o /dev/null -w '%{http_code}' -H "If-Modified-Since: $(hdr_of /about/ last-modified || true)" "http://127.0.0.1:$port/about/")" "304"
expect "301 status line" "$(curl -s -D - -o /dev/null "http://127.0.0.1:$port/about" | head -1 | tr -d '\r')" "HTTP/1.1 301 Moved Permanently"
expect "304 no content-length" "$(curl -s -D - -o /dev/null -H "If-None-Match: $etag" "http://127.0.0.1:$port/css/style.css" | tr -d '\r' | grep -ci '^content-length:' || true)" "0"
expect "304 lowercase header name" "$(curl -s -o /dev/null -w '%{http_code}' -H "if-none-match: $etag" "http://127.0.0.1:$port/css/style.css")" "304"
expect "etag miss 200" "$(curl -s -o /dev/null -w '%{http_code}' -H 'If-None-Match: "nope"' "http://127.0.0.1:$port/css/style.css")" "200"
expect "etag miss body" "$(curl -s -H 'If-None-Match: "nope"' "http://127.0.0.1:$port/css/style.css")" "body{}"
expect "head 304" "$(curl -s -I -o /dev/null -w '%{http_code}' -H "If-None-Match: $etag" "http://127.0.0.1:$port/css/style.css")" "304"
expect "last-modified format" "$(hdr_of /about/ last-modified | grep -Ec '^[A-Z][a-z]{2}, [0-9]{2} [A-Z][a-z]{2} [0-9]{4} [0-9]{2}:[0-9]{2}:[0-9]{2} GMT$' || true)" "1"
expect "404 star" "$(curl -s -o /dev/null -w '%{http_code}' -H 'If-None-Match: *' "http://127.0.0.1:$port/nope4")" "404"
expect "head cgi 405" "$(curl -s -I -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/echo.cgi")" "405"
expect "head cgi allow" "$(head_of /echo.cgi | grep -i '^allow:' | cut -d' ' -f2- || true)" "GET, POST"
printf 'body[]' > "$tmp/site/css/style.css"
touch -d "@$(( $(stat -c %Y "$tmp/site/css/style.css") + 1 ))" "$tmp/site/css/style.css"
expect "etag changed" "$([ "$(hdr_of /css/style.css etag || true)" != "$etag" ] && echo yes || echo no)" "yes"
expect "200 after change" "$(code_of /css/style.css)" "200"
expect "webmanifest ctype" "$(ctype_of /site.webmanifest)" "application/manifest+json"
expect "eot ctype" "$(ctype_of /font.eot)" "application/vnd.ms-fontobject"
expect "otf ctype" "$(ctype_of /font.otf)" "font/otf"
expect "wasm ctype" "$(ctype_of /mod.wasm)" "application/wasm"
rm "$tmp/site/404.html"
expect "no 404 page" "$(code_of /nope2)" "404"
expect "no 404 page empty" "$(body_of /nope2)" ""
echo "ALL PASS"
