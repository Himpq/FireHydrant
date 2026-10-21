# FireHydrant

A6700 固件研究笔记。

记录研究过程中形成的判断与方法：如何确认某项能力是否已存在于固件中、如何区分“界面未呈现”与“功能未生效”、如何在不可逆写入之前用可恢复的方式验证行为。内容停留在方法层面，不包含项目内部的具体地址、数据与工具实现。

## 目录

- [`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md) —— Jiritsu 通道与固件内存访问：通信建立、`libJit.so` 模块加载、`/proc/self/maps` 基址定位与两段偏移换算、运行时修改的可恢复性来源。
- [`fl2_fl3/`](fl2_fl3/) —— 移植类改动的拆解方法：分层验证、常见误判、回退设计。

## 说明

- 文中涉及的 Jiritsu 通道、`libJit.so` 加载入口等机制，社区已有公开研究，来源清单见 [`DEVICE_CHANNEL.md` §8 参考资料](DEVICE_CHANNEL.md)；本仓库的工作是在 A6700 固件 2.00 上的确认；
- 本仓库为文字记录，不包含固件二进制、补丁数据与可执行脚本；
- 不同固件版本的结论可能存在差异；内容与厂商无关，不构成操作建议。

## 参考项目

| 项目 | 用途 |
|---|---|
| [`ma1co/fwtool.py`](https://github.com/ma1co/fwtool.py) | 固件镜像解包（FDAT 解密、分区与文件系统提取） |
| `pmca-re` | 索尼相机 USB/PTP 会话与鉴权链 |
| [`YuuLuo/a7c2-service-preload`](https://github.com/YuuLuo/a7c2-service-preload) | Jiritsu 通道与 `libJit.so` 加载入口（CC BY-SA 4.0） |
| `MDHan/a7c2-fl23-macos` | A7C II FL2/FL3 工具链前端与流程记录 |
