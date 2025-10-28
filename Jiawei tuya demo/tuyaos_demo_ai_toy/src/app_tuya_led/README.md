# 组件模板说明

## 模板文件列表

| 名称     | 是否必须 | 说明      |
| -------- | ------- | --------- |
| include  | 可选    | 头文件目录 |
| src      | 可选    | 源文件目录 |
| local.mk | 可选    | 组件编译描述文件 |
| template.yaml | 必选 | 组件版本定义文件 |

## template.yaml 文件说明

此文件由 embed 工具读取并维护，用于组件仓库地址和版本管理。

[embed 工具使用说明](https://code-odm.tuya-inc.com/embed/embeddocs/blob/master/README.md)

自定义模板时，此文件必须加上，且文档内的以下内容有特殊含义，不可修改：

| 名称 | 含义 |
| --- | --- |
| EmbedTemplate  | 组件名称 |
| EMBED_VERSION  | 组件版本 |
| GIT_USERNAME   | 所有者用户名 |
| GIT_USER_EMAIL | 所有者邮箱 |
| GIT_ADDRESS    | 仓库地址 |
| EMBED_VERSION  | GIT TAG |

## local.mk 说明

此文件非必要文件，当采用 [xmake 编译框架](https://code.registry.wgine.com/embed_ci_space_group/xmake/blob/master/README.md)时，使用此文件。

local.mk 语法参照自 [NDK Android.mk 语法](https://developer.android.google.cn/ndk/guides/android_mk)。
