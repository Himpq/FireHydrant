# FireHydrant

A6700 固件研究笔记。

记录研究过程中形成的判断与方法：如何确认某项能力是否已存在于固件中、如何区分“界面未呈现”与“功能未生效”、如何在不可逆写入之前用可恢复的方式验证行为。

## 目录

- [`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md) —— Jiritsu 通道与固件内存访问：通信建立、`libJit.so` 模块加载、`/proc/self/maps` 基址定位与两段偏移换算、运行时修改的可恢复性来源。
- [`fl2_fl3/`](fl2_fl3/) —— 移植类改动的拆解方法、落点与验收数字。
- [`cine_ei/`](cine_ei/) —— Cine EI 三层准入、计算基准与固化包说明。
- [`wbi/`](wbi/) —— 通用快照头工具：读、备份、清零、写回；C 源码 + 主机 CLI。

## 说明

- 文中涉及的 Jiritsu 通道、`libJit.so` 加载入口等机制，社区已有公开研究，来源清单见 [`DEVICE_CHANNEL.md` §8 参考资料](DEVICE_CHANNEL.md)；本仓库的工作是在 A6700 固件 2.00 上的确认；
- 本仓库不含固件镜像、补丁表与补丁载荷；`wbi/` 是唯一的可运行组件，只读写 `/dev/nflasha5` 的前 512 B；
- 不同固件版本的结论可能存在差异；内容与厂商无关，不构成操作建议。

## 参考项目

| 项目 | 用途 |
|---|---|
| [`ma1co/fwtool.py`](https://github.com/ma1co/fwtool.py) | 固件镜像解包（FDAT 解密、分区与文件系统提取） |
| `pmca-re` | 索尼相机 USB/PTP 会话与鉴权链 |
| [`YuuLuo/a7c2-service-preload`](https://github.com/YuuLuo/a7c2-service-preload) | Jiritsu 通道与 `libJit.so` 加载入口（CC BY-SA 4.0） |
| `MDHan/a7c2-fl23-macos` | A7C II FL2/FL3 工具链前端与流程记录 |
