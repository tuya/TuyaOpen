#!/bin/bash

# 推送 LED PWM Control 分支到远程仓库
# 使用方法：运行此脚本并按提示输入GitHub凭据

echo "=========================================="
echo "推送 feature/led-pwm-control 分支到远程"
echo "=========================================="
echo ""

cd "$(dirname "$0")"

# 检查当前分支
CURRENT_BRANCH=$(git branch --show-current)
echo "当前分支: $CURRENT_BRANCH"

if [ "$CURRENT_BRANCH" != "feature/led-pwm-control" ]; then
    echo "警告: 当前不在 feature/led-pwm-control 分支"
    read -p "是否切换到 feature/led-pwm-control 分支? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git checkout feature/led-pwm-control
    else
        echo "已取消"
        exit 1
    fi
fi

# 显示最近的提交
echo ""
echo "最近的提交:"
git log --oneline -1
echo ""

# 提示用户
echo "准备推送到: origin/feature/led-pwm-control"
echo "如果提示输入凭据:"
echo "  - 用户名: 您的GitHub用户名"
echo "  - 密码: 使用GitHub Personal Access Token (不是账户密码)"
echo ""
echo "如果没有Token，请访问: https://github.com/settings/tokens"
echo "生成新token时请勾选 'repo' 权限"
echo ""
read -p "按回车键开始推送，或 Ctrl+C 取消..."

# 执行推送
echo ""
echo "正在推送..."
git push -u origin feature/led-pwm-control

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 推送成功！"
    echo ""
    echo "下一步："
    echo "1. 访问 https://github.com/tuya/TuyaOpen"
    echo "2. 您会看到提示创建 Pull Request"
    echo "3. 点击 'Compare & pull request' 创建PR"
else
    echo ""
    echo "❌ 推送失败"
    echo ""
    echo "可能的原因："
    echo "1. 认证失败 - 请检查用户名和Token"
    echo "2. 没有推送权限 - 您可能需要Fork仓库"
    echo "3. 网络问题 - 请检查网络连接"
fi

