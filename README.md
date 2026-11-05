# FireHydrant

A6700 固件研究笔记。

## 目录

- [`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md) —— 相机通道与内存访问
- [`fl2_fl3/`](fl2_fl3/) —— FL2/FL3 滤镜刷入与 [delta 工具](fl2_fl3/delta/)
- [`cine_ei/`](cine_ei/) —— Cine EI 固化与运行期补丁
- [`wbi/`](wbi/) —— 清头与 AP 程序头工具
- [`upm/`](upm/) —— UPM 读模式语义与 `/sabin` PSF 表
- [`ai_af/`](ai_af/) —— a6700 AI 对焦模块 ONNX (not yet released)
- [`isp/`](isp/) —— 索尼 av-cam ISP 固件研究 (not yet released)
- [`1080p_binning`](1080p_binning) —— 1080p  超采样
- ...

## Ongoing
- OpenGate
- AI 对焦权重 ONNX化

## Already Done
- 图像伽马曲线
- 自定义滤镜刷入 ( FL2 & FL3 )
- CineEI
- 1080p 超采样

## 参考项目

| 项目 | 用途 |
|---|---|
| [`ma1co/fwtool.py`](https://github.com/ma1co/fwtool.py) | 固件镜像解包（FDAT 解密、分区与文件系统提取） |
| [`pmca-re`](https://github.com/ma1co/Sony-PMCA-RE/) | 索尼相机 USB/PTP 会话与鉴权链 |
| [`YuuLuo/a7c2-service-preload`](https://github.com/YuuLuo/a7c2-service-preload) | Jiritsu 通道与 `libJit.so` 加载入口（CC BY-SA 4.0） |
| [`MDHan/a7c2-fl23-macos`](https://github.com/MDHan/a7c2-fl23-macos) | A7C II FL2/FL3 工具链前端与流程记录 |


