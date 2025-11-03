# Git 上传代码指南

## 当前状态
- 您有新的项目：`apps/tuya.ai/your_chat_bot_led_ctl/`
- 原 `your_chat_bot` 目录下的文件被删除（可能已移动到新目录）

## 上传步骤

### 1. 检查当前更改
```bash
git status
```

### 2. 添加新文件到暂存区

#### 添加新项目目录（推荐）
```bash
# 只添加新项目目录
git add apps/tuya.ai/your_chat_bot_led_ctl/
```

#### 或者添加所有更改
```bash
# 添加所有更改（包括删除的文件）
git add -A
```

#### 或者分别添加不同类型
```bash
# 只添加新文件和修改的文件（不包括删除）
git add apps/tuya.ai/your_chat_bot_led_ctl/

# 如果需要提交删除的文件
git add -u
```

### 3. 检查即将提交的内容
```bash
git status
git diff --cached  # 查看已暂存的更改
```

### 4. 提交更改
```bash
git commit -m "添加LED PWM控制功能到your_chat_bot_led_ctl项目"
```

或者更详细的提交信息：
```bash
git commit -m "feat: 添加LED PWM控制功能

- 新增LED开关控制（DP ID: 20）
- 新增LED亮度控制（DP ID: 22）
- 支持本地触摸按键控制（GPIO29）
- 更新README文档（中英文）
- 使用PWM5通道控制LED
"

### 5. 推送到远程仓库

#### 如果您有推送权限（原始仓库）
```bash
# 推送到master分支（不推荐直接推送到master）
git push origin master

# 推荐：创建并推送到新分支
git checkout -b feature/led-pwm-control
git push origin feature/led-pwm-control
```

#### 如果您需要Fork仓库
```bash
# 1. 首先fork仓库到您的GitHub账户（在GitHub网站上操作）

# 2. 添加您的fork作为远程仓库
git remote add myfork https://github.com/您的用户名/TuyaOpen.git

# 3. 推送到您的fork
git push myfork master

# 或推送到新分支
git checkout -b feature/led-pwm-control
git push myfork feature/led-pwm-control
```

### 6. 创建Pull Request（如果使用Fork）
- 访问 https://github.com/tuya/TuyaOpen
- 点击 "New Pull Request"
- 选择您的fork和分支
- 填写PR描述
- 提交PR等待审核

## 常用Git命令

### 查看更改
```bash
git status                    # 查看工作区状态
git diff                      # 查看未暂存的更改
git diff --cached             # 查看已暂存的更改
git log --oneline            # 查看提交历史
```

### 撤销更改（谨慎使用）
```bash
# 撤销未暂存的更改（危险！）
git restore <文件>

# 撤销已暂存但未提交的更改
git restore --staged <文件>

# 修改最后一次提交
git commit --amend
```

### 分支操作
```bash
git branch                    # 查看本地分支
git branch -a                 # 查看所有分支
git checkout -b <分支名>      # 创建并切换到新分支
git checkout <分支名>         # 切换到分支
```

## 注意事项

1. **不要直接推送到master分支**：建议创建新分支或使用fork
2. **提交前检查**：使用 `git status` 和 `git diff` 确认更改
3. **提交信息清晰**：使用有意义的提交信息
4. **忽略文件**：确保 `.DS_Store` 等文件在 `.gitignore` 中

## 如果遇到问题

### 撤销所有本地更改
```bash
git restore .
git clean -fd  # 删除未跟踪的文件
```

### 拉取最新代码
```bash
git fetch origin
git pull origin master
```

### 解决冲突
如果推送时出现冲突，需要先拉取最新代码并解决冲突：
```bash
git pull origin master
# 解决冲突后
git add .
git commit -m "解决合并冲突"
git push origin master
```

