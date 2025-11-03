#!/bin/bash

# LED PWM Control 项目更新脚本
# 使用方法: ./update_led_ctl.sh "提交信息"

cd "$(dirname "$0")"

echo "=========================================="
echo "更新 LED PWM Control 代码到 GitHub"
echo "=========================================="
echo ""

# 检查是否有未提交的更改
if [ -z "$(git status --porcelain)" ]; then
    echo "✅ 没有未提交的更改"
    exit 0
fi

# 显示更改状态
echo "📋 当前更改："
git status --short
echo ""

# 如果没有提供提交信息，提示输入
if [ -z "$1" ]; then
    echo "请输入提交信息："
    read -r commit_msg
else
    commit_msg="$1"
fi

if [ -z "$commit_msg" ]; then
    echo "❌ 错误：提交信息不能为空"
    exit 1
fi

# 添加所有更改
echo ""
echo "📦 添加更改到暂存区..."
git add .

# 提交
echo "💾 提交更改..."
git commit -m "$commit_msg"

if [ $? -ne 0 ]; then
    echo "❌ 提交失败"
    exit 1
fi

# 推送到远程
echo ""
echo "🚀 推送到 GitHub..."
git push tuyaopenAILED feature/led-pwm-control

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 更新成功！"
    echo ""
    echo "查看代码: https://github.com/robeortZ/tuyaopenAILED"
else
    echo ""
    echo "❌ 推送失败，请检查网络连接和权限"
    exit 1
fi

