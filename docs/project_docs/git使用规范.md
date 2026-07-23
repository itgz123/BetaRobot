# Git 使用规范

以下是一份通用的 **Git 使用规范**，适用于团队协作和项目维护。

---

## 一、分支管理规范

| 分支类型     | 命名示例          | 说明                                             |
| ------------ | ----------------- | ------------------------------------------------ |
| **主分支**   | `main` / `master` | 生产环境代码，始终处于可发布状态。禁止直接提交。 |
| **开发分支** | `develop`         | 集成最新功能，用于测试和预发布。                 |
| **功能分支** | `feature/xxx`     | 从 `develop` 拉出，开发新功能。                  |
| **修复分支** | `hotfix/xxx`      | 从 `main` 拉出，紧急修复线上问题。               |
| **发布分支** | `release/v1.0`    | 从 `develop` 拉出，准备发布。                    |

> ✅ 原则：`main` 和 `develop` 受保护，禁止 force push。

---

## 二、提交信息规范

采用 **Conventional Commits** 格式：

```
<type>(<scope>): <subject>

[optional body]
[optional footer]
```

### 常用 type

| type       | 含义                           |
| ---------- | ------------------------------ |
| `feat`     | 新功能                         |
| `fix`      | 修复 Bug                       |
| `docs`     | 仅文档变更                     |
| `style`    | 代码格式（不影响逻辑）         |
| `refactor` | 重构（不是新功能也不是修 Bug） |
| `perf`     | 性能优化                       |
| `test`     | 添加或修正测试                 |
| `chore`    | 构建过程、工具、依赖等变更     |

### 示例

```
feat(auth): 添加OAuth2登录支持

- 集成Google和GitHub登录
- 新增session管理

Closes #123
```

> ✅ 要求：每条提交只做一件事；首字母小写；subject 不超过 72 字符。

---

## 三、操作流程规范

### 日常开发流程

```bash
# 1. 拉取最新 develop
git checkout develop
git pull origin develop

# 2. 创建功能分支
git checkout -b feature/user-profile

# 3. 多次提交
git add .
git commit -m "feat(profile): 添加头像上传组件"

# 4. 保持分支最新
git fetch origin
git rebase origin/develop

# 5. 推送分支
git push origin feature/user-profile
```

### 紧急热修复流程

```bash
# 从 main 拉出 hotfix
git checkout main
git checkout -b hotfix/login-failure

# 修复 + 提交
git commit -m "fix(login): 修复空密码导致的崩溃"

# 合并回 main
git checkout main
git merge --no-ff hotfix/login-failure
git tag v1.0.1

# 同时合并回 develop
git checkout develop
git merge hotfix/login-failure
```

---

## 四、标签与版本管理

- **语义化版本**：`v<major>.<minor>.<patch>`
  - `major`：不兼容的 API 变更
  - `minor`：向后兼容的新功能
  - `patch`：向后兼容的 Bug 修复

```bash
git checkout main
git tag -a v2.0.0 -m "发布2.0.0版本"
git push origin v2.0.0
```

---

## 五、禁止的操作

| 禁止行为                               | 原因                       |
| -------------------------------------- | -------------------------- |
| `git push --force` 到公共分支          | 覆盖他人提交，导致历史丢失 |
| 在公共分支上 `rebase`                  | 改写历史，协作困难         |
| 提交大文件（>50MB）或密钥              | 仓库膨胀或安全泄露         |
| 提交调试代码（`console.log`、`print`） | 污染代码库                 |

---

## 六、快速检查清单

- [ ] 分支命名符合规范（`feature/xxx`, `fix/xxx` …）
- [ ] 提交信息遵循 Conventional Commits
- [ ] 不包含调试代码、TODO 待办项
- [ ] CI（测试、构建、Lint）全部通过
- [ ] 合并前已 rebase 最新的目标分支
- [ ] 热修复同时合并回了 `develop`
