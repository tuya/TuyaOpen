# 贡献指南

欢迎通过 github 的 Fork 和 [Push Requests](https://docs.github.com/zh/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests) 流程为 [TuyaOpen](https://github.com/tuya/TuyaOpen) 贡献内容，包括修复功能代码、文档错误，新增功能和文档等。

## 修改内容
1. 在本地 PC 中安装 git 工具。
2. 将 TuyaOpen 仓库 fork 到自己的 github 仓库中。
3. 将 fork 后的 TuyaOpen 仓库通过 `git clone` 命令 clone 到本地 PC 中。
4. 从待合并的分支中，通过 `git checout -b <BranchName>` 命令创建自己的本地开发分支。
5. 在本地修改待提交的代码/文档等内容。
6. 提交修改至自己的 github 仓库。

## 准备工作
1. TuyaOpen 采用 Apache License 2.0 开源许可，检查贡献内容是否已获得  Apache License 2.0 兼容的开源许可。如果不是我们不接受该内容。
3. 检查代码是否符合 [TuyaOpen 编码规范](./code_style_guide.md)，代码是否注释充分便于读者理解其结构。
3. 检查是否为贡献的代码提供文档或示例，要写出好的示例和文档才可以帮助其他开发者更好的使用。
4. 检查文档是否符合 [文档编写规范]()。
5. 如需提交多个内容，检查是否将所有内容按照改动的类型（每个 Pull Request 对应一个主要改动）进行分组。
6. 如提交 board 下芯片适配相关内容，需符合 [TuyaOpen 芯片适配规范]()。

检查完成后，可进入 Push Requests 流程。

## 提交 Push Requests
在自己的 github 账号下 TuyaOpen 仓库中选择当前修改的分支，点击 New pull request 按钮发起 Pull Request。

开发者第一次提交贡献内容时，需要签署 Contributor License Agreement，请确认 CLA 显示签署成功。

创建 Pull Request 后，会进入自动化 ci 编译测试及人工审核流程。

Pull Request 过程中，PR 评论区中可能有一些关于该请求的讨论，请及时查看 PR 状态并根据评审意见更新代码与文档。

测试及审核通过后会被合并到 TuyaOpen 仓库中。

至此，我们就完成了一次贡献的过程。
