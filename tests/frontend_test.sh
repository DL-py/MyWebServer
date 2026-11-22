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
