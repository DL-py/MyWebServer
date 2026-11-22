#!/usr/bin/env bash
# Integration test for the CGI support module. Uses an isolated port (6390).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
config="/tmp/myhttp-cgi-test.cfg"
body="/tmp/myhttp-cgi-body.html"
evil="$root/evil-cgi-test.cgi"

cleanup() {
    fuser -k 6390/tcp >/dev/null 2>&1 || true
    rm -f "$config" "$body" "$evil"
}
trap cleanup EXIT

cp "$root/etc/common.cfg.example" "$config"
sed -i 's/port = 6380;/port = 6390;/' "$config"

cat > "$evil" <<'EOF'
#!/bin/sh
echo "EVIL_CGI_EXECUTED"
EOF
chmod +x "$evil"

cd "$root"

./run.sh --config "$config" >/tmp/myhttp-cgi-test.log 2>&1 &

for _ in $(seq 1 40); do
    code=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:6390/welcome.html 2>/dev/null || true)
    [ "$code" = "200" ] && break
    sleep 0.25
done
if [ "${code:-000}" != "200" ]; then
    echo "FAIL: test server did not start (last code $code)"
    exit 1
fi

fail() {
    echo "FAIL: $1"
    exit 1
}

# Static page regression.
code=$(curl -s -o "$body" -w '%{http_code}' http://127.0.0.1:6390/welcome.html)
[ "$code" = "200" ] || fail "welcome.html returned $code"

# register.cgi: Is_Repeat defaults to 0, CONTENT_LENGTH is set, so the
# existing script should produce the registration-success page.
code=$(curl -s -o "$body" -w '%{http_code}' -X POST -d 'user=test&password=123' \
    http://127.0.0.1:6390/register.cgi)
[ "$code" = "200" ] || fail "POST register.cgi returned $code"
grep -q '注册成功' "$body" || fail "POST register.cgi body missing 注册成功"

# login.cgi: IsCorrect defaults to 0, so the failure page is expected.
code=$(curl -s -o "$body" -w '%{http_code}' -X POST -d 'user=test&password=123' \
    http://127.0.0.1:6390/login.cgi)
[ "$code" = "200" ] || fail "POST login.cgi returned $code"
grep -q '用户不存在或密码输入有误' "$body" || fail "POST login.cgi body missing error message"

# GET on a .cgi script is still a CGI request.
code=$(curl -s -o "$body" -w '%{http_code}' http://127.0.0.1:6390/register.cgi)
[ "$code" = "200" ] || fail "GET register.cgi returned $code"

# Missing CGI script.
code=$(curl -s -o "$body" -w '%{http_code}' http://127.0.0.1:6390/notfound.cgi)
[ "$code" = "404" ] || fail "GET notfound.cgi returned $code, expected 404"

# Path traversal: a .cgi file outside doc_root must not be executed.
code=$(curl --path-as-is -s -o "$body" -w '%{http_code}' http://127.0.0.1:6390/../evil-cgi-test.cgi)
[ "$code" != "200" ] || fail "path traversal executed CGI outside doc_root"
if grep -q 'EVIL_CGI_EXECUTED' "$body"; then
    fail "path traversal returned script output"
fi

# POST to a static resource is not a CGI request.
code=$(curl -s -o "$body" -w '%{http_code}' -X POST -d 'x=1' \
    http://127.0.0.1:6390/welcome.html)
[ "$code" = "400" ] || fail "POST welcome.html returned $code, expected 400"

echo "CGI integration test PASS"
