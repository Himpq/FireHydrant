# FireHydrant

A6700 固件研究笔记。

## 目录

- [`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md) —— 相机通道与内存访问
- [`fl2_fl3/`](fl2_fl3/) —— FL2/FL3 滤镜刷入
- [`cine_ei/`](cine_ei/) —— Cine EI 固化
- [`wbi/`](wbi/) —— Jiritsu 模块：快照头、AP 程序头、CP delta

## 参考项目

| 项目 | 用途 |
|---|---|
| [`ma1co/fwtool.py`](https://github.com/ma1co/fwtool.py) | 固件镜像解包（FDAT 解密、分区与文件系统提取） |
| `pmca-re` | 索尼相机 USB/PTP 会话与鉴权链 |
| [`YuuLuo/a7c2-service-preload`](https://github.com/YuuLuo/a7c2-service-preload) | Jiritsu 通道与 `libJit.so` 加载入口（CC BY-SA 4.0） |
| `MDHan/a7c2-fl23-macos` | A7C II FL2/FL3 工具链前端与流程记录 |
