# 统一浅色 UI 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 welcome、login、register 三页统一为无背景图片的清爽浅色卡片布局。

**Architecture:** 重写一份共享 `web/styles.css` 作为唯一样式来源，三页使用相同的 class 和设计变量；逐页迁移，最后删除不再引用的图片与 Bootstrap 文件。服务器只做静态文件服务，不需要改 C++。

**Tech Stack:** 原生 HTML5 + CSS3，无框架、无外部字体。

**Spec:** `docs/superpowers/specs/2026-11-21-unified-light-ui-design.md`

## Global Constraints

- 页面背景 `#f3f7f4`，主色 `#00B72E`，白色卡片，圆角 16px。
- 普通卡片最大宽度 520px，注册页 640px；小屏自适应。
- 不引入图片、外部字体、框架或构建工具。
- 保留表单 `action="register.cgi"`、`method="POST"` 和字段名 `user`、`password`、`hobby`、`sex`、`introduction`、`address`。
- 不修改 `.cgi` 文件和 `myhttp` 源码。
- 三页都包含 `lang="zh-CN"`、`meta viewport`、`label for`。

## Review Focus

- 注册/登录提交目标与字段名是否保持原样（后端兼容）。
- 页面是否在 375px 宽度下不横向溢出。
- 页面是否不再发起任何图片、Bootstrap、Google Fonts 请求。
- 输入框聚焦时是否有可见的焦点样式（键盘可用性）。
- 旧图片/样式引用是否全部清除，避免 404。

---

### Task 1: 共享样式 + welcome 页

**Files:**
- Modify: `web/styles.css`（整文件重写）
- Modify: `web/welcome.html`

**Interfaces:**
- Produces: CSS 变量与 class：`.page`、`.card`、`.card--wide`、`.title`、`.subtitle`、`.form`、`.field`、`.input`、`.actions`、`.btn`、`.btn--primary`、`.btn--secondary`、`.footer-link`、`.grid`。

- [ ] **Step 1: 写验证命令（当前应失败）**

```bash
curl -s http://127.0.0.1:6380/welcome.html | grep -q 'class="card"' && echo PASS || echo FAIL
```

Expected: `FAIL`（旧页面没有 card）。

- [ ] **Step 2: 重写 `web/styles.css`**

定义 `:root` 变量：`--bg:#f3f7f4`、`--surface:#fff`、`--text:#1f2a24`、`--muted:#5f6f66`、`--primary:#00B72E`、`--primary-dark:#009a27`、`--border:#dbe5de`、`--focus:rgba(0,183,46,.25)`、`--radius:16px`。

实现：

- `body`：系统字体栈、`background:var(--bg)`、`min-height:100vh`、`display:flex`、居中、`padding:24px`、`color:var(--text)`。
- `.card`：白底、`max-width:520px`、`width:100%`、`padding:40px`、圆角 16px、`box-shadow:0 12px 32px rgba(31,42,36,.08)`；`.card--wide` 改 `max-width:640px`。
- `.title` 28px/700；`.subtitle` 15px、`color:var(--muted)`、下边距 24px。
- `.field`：纵向排列、间距 6px、下边距 16px；`label` 14px、`font-weight:600`。
- `.input`：宽 100%、`padding:12px 14px`、`background:#f7f9f8`、`border:1px solid var(--border)`、圆角 10px、字号 16px；`:focus` 时 `outline:3px solid var(--focus)`、`border-color:var(--primary)`。
- `.grid`：`display:grid`、`grid-template-columns:1fr 1fr`、`gap:0 16px`；`@media(max-width:480px)` 改单列。
- `.btn`：`display:inline-flex`、居中、宽 100%、`padding:12px 18px`、圆角 999px、字号 16px、`font-weight:600`、`cursor:pointer`、`border:1px solid transparent`。
- `.btn--primary`：`background:var(--primary)`、白字；`:hover` 用 `--primary-dark`。
- `.btn--secondary`：白底、`border-color:var(--primary)`、绿字。
- `.actions`：按钮纵向排列、间距 12px、上边距 24px。
- `.footer-link`：上边距 20px、居中、14px、`color:var(--muted)`；`a` 为 `--primary`、无下划线、`:hover` 加下划线。
- `@media(max-width:480px)`：`.card` 的 `padding` 改为 24px。

- [ ] **Step 3: 重写 `web/welcome.html`**

结构：`<!doctype html>`、`<html lang="zh-CN">`、`meta charset="utf-8"`、`meta viewport`、`title` 为「欢迎」、`link rel="stylesheet" href="styles.css"`。body 内：

```
<main class="page">
  <section class="card">
    <h1 class="title">Hello, visitor!</h1>
    <p class="subtitle">I am testing my little web server.</p>
    <div class="actions">
      <a class="btn btn--primary" href="login.html">登录</a>
      <a class="btn btn--secondary" href="register.html">注册</a>
    </div>
  </section>
</main>
```

移除原来的 `<style>`、`background.webp`、`<br>` 和行内样式按钮。

- [ ] **Step 4: 验证**

```bash
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:6380/welcome.html   # 200
curl -s http://127.0.0.1:6380/welcome.html | grep -c 'class="card"'           # 1
grep -n 'background.webp\|background-image' web/styles.css web/welcome.html    # 无输出
```

- [ ] **Step 5: 提交**

```bash
git add web/styles.css web/welcome.html
git commit -m "Redesign welcome page with shared light theme"
```

---

### Task 2: login 页

**Files:**
- Modify: `web/login.html`

**Interfaces:**
- Consumes: Task 1 的 `.card`、`.form`、`.field`、`.input`、`.btn`、`.footer-link`。

- [ ] **Step 1: 写验证命令（当前应失败）**

```bash
curl -s http://127.0.0.1:6380/login.html | grep -q 'class="form"' && echo PASS || echo FAIL
```

Expected: `FAIL`。

- [ ] **Step 2: 重写 `web/login.html`**

- `lang="zh-CN"`、viewport、`title` 为「登录」。
- 只引用 `styles.css`；删除 `bootstrap.min.css`、Google Fonts、`#login-bg`、`.bg-img`、`.bg-color`。
- 卡片结构：`<main class="page"><section class="card">`，`h1.title` 为「登录」。
- 表单：`<form class="form" action="register.cgi" method="POST">`。
  - `user`：`<input class="input" type="text" id="user" name="user" autocomplete="username" required>`，label 为「账号」。
  - `password`：`<input class="input" type="password" id="password" name="password" autocomplete="current-password" required>`，label 为「密码」。
- `.actions` 内放 `<button class="btn btn--primary" type="submit">登录</button>`。
- 卡片底部：`<p class="footer-link">还没有账号？<a href="register.html">去注册</a></p>`。

- [ ] **Step 3: 验证**

```bash
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:6380/login.html      # 200
curl -s http://127.0.0.1:6380/login.html | grep -c 'name="user"'               # 1
grep -n 'bootstrap\|fonts.googleapis\|bg-image' web/login.html                 # 无输出
```

- [ ] **Step 4: 提交**

```bash
git add web/login.html
git commit -m "Redesign login page with shared light theme"
```

---

### Task 3: register 页

**Files:**
- Modify: `web/register.html`

**Interfaces:**
- Consumes: Task 1 的样式类，含 `.grid` 和 `.card--wide`。

- [ ] **Step 1: 写验证命令（当前应失败）**

```bash
curl -s http://127.0.0.1:6380/register.html | grep -q 'class="card card--wide"' && echo PASS || echo FAIL
```

Expected: `FAIL`。

- [ ] **Step 2: 重写 `web/register.html`**

- `lang="zh-CN"`、viewport、`title` 为「注册」；引用 `styles.css`。
- 结构：`<main class="page"><section class="card card--wide">`，`h1.title` 为「注册」，可加一行 `.subtitle` 为「填写下面的信息完成注册」。
- 表单：`<form class="form" action="register.cgi" method="POST">`。
- 字段（全部使用 `.field` + `.input`，带 `label for`）：
  - `user`：text，`maxlength="11"`，`required`，`autocomplete="username"`。
  - `password`：password，`required`，`autocomplete="new-password"`。
  - `hobby`：select，选项值/文本：运动、唱歌、玩游戏、学习（修正原「学习」误用 `value="玩游戏"`，改为 `value="学习"`）。
  - `sex`：select，`boy`/男、`girl`/女。
  - `introduction`：textarea，`rows="5"`。
  - `address`：select，北京、四川、上海、武汉。
- 爱好与性别放进 `<div class="grid">` 两列；`introduction` 单独一行。
- `.actions` 内：`<button class="btn btn--primary" type="submit">提交</button>` 和 `<button class="btn btn--secondary" type="reset">重置</button>`。
- 底部：`<p class="footer-link">已有账号？<a href="login.html">去登录</a></p>`。
- 删除 `<center>`、`<table>`、注释掉的死代码和 `image.webp` 引用。

- [ ] **Step 3: 验证**

```bash
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:6380/register.html    # 200
for f in user password hobby sex introduction address; do
  curl -s http://127.0.0.1:6380/register.html | grep -q "name=\"$f\"" && echo "$f OK" || echo "$f MISSING"
done
grep -n '<table\|<center\|image.webp' web/register.html                         # 无输出
```

- [ ] **Step 4: 提交**

```bash
git add web/register.html
git commit -m "Redesign register page with shared light theme"
```

---

### Task 4: 删除旧资源并整体验证

**Files:**
- Delete: `web/image.webp`、`web/background.webp`、`web/bg-image.jpeg`、`web/main.webp`、`web/bootstrap.min.css`

- [ ] **Step 1: 删除文件**

```bash
git rm web/image.webp web/background.webp web/bg-image.jpeg web/main.webp web/bootstrap.min.css
```

- [ ] **Step 2: 全量检查残留引用**

```bash
grep -rn 'image.webp\|background.webp\|bg-image\|main.webp\|bootstrap' web/ || echo "no stale refs"
```

Expected: `no stale refs`。

- [ ] **Step 3: 三页状态码与关键内容**

```bash
for p in welcome.html login.html register.html; do
  code=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:6380/$p")
  echo "$p -> $code"
done
```

Expected: 三行都是 200。

- [ ] **Step 4: 确认无外部/图片请求**

```bash
grep -rn 'src=\|url(' web/*.html web/styles.css || echo "no images or external assets"
```

Expected: 除表单 action 外无 `src=`，无 `url(`。

- [ ] **Step 5: 提交**

```bash
git add -A web/
git commit -m "Remove unused background images and Bootstrap"
```
