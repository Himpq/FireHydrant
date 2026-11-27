# FireHydrant

A6700 固件研究笔记。

模块发布与刷写：[Himpq/Pilot](https://github.com/Himpq/Pilot)。

## 目录

- [`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md) —— 相机通道与内存访问
- [`fl2_fl3/`](fl2_fl3/) —— FL2/FL3 滤镜刷入与 [delta 工具](fl2_fl3/delta/)
- [`cine_ei/`](cine_ei/) —— Cine EI 固化与运行期补丁
- [`wbi/`](wbi/) —— 清头与 AP 程序头工具
- [`upm/`](upm/) —— UPM 读模式语义与 `/sabin` PSF 表
- [`ai_af/`](ai_af/) —— a6700 AI 对焦研究
- [`bypass_sd_limit/`](bypass_sd_limit/) —— 卡速限制绕过
- [`1080p_binning/`](1080p_binning) —— 1080p 超采样
- [`isp/`](isp/) —— 索尼 av-cam ISP 固件研究 (not yet released)

## Ongoing
- 照片 PPLUT
- 快门角度
  
## Already Done
- 6K30
- 图像伽马曲线
- 自定义滤镜刷入 ( FL2 & FL3 )
- CineEI
- 1080p 超采样
- 绕过卡速
- 2K OpenGate
- 5.7K & 4K OpenGate Test [`ArcaneLantern/a6700-opengate-test`](https://github.com/ArcaneLantern/a6700-open-gate-test)
- 反挤压

## 已完成模块
- 反挤压  
  <img width="400" alt="image" src="https://github.com/user-attachments/assets/2f12a67d-7b28-4aea-8a0c-a36d9823b7e5" />
- 变焦焦段提示 (我真的很喜欢这个小玩意，但是索尼就是没有)  
  <img width="400" alt="image" src="https://github.com/user-attachments/assets/e4f128dc-8cea-4781-8591-8d186c2586f7" />
- 波形图  
  <img width="400" alt="image" src="https://github.com/user-attachments/assets/99cf1ee6-1e0a-4090-a3fa-3500fa75d665" />
- 6K、OpenGate、1080p超采热切换  
  <img width="400" alt="image" src="https://github.com/user-attachments/assets/73f2e908-f39b-46e0-876b-f29020ea20ff" />  

参见Pilot

## 参考项目

| 项目 | 用途 |
|---|---|
| [`ma1co/fwtool.py`](https://github.com/ma1co/fwtool.py) | 固件镜像解包（FDAT 解密、分区与文件系统提取） |
| [`pmca-re`](https://github.com/ma1co/Sony-PMCA-RE/) | 索尼相机 USB/PTP 会话与鉴权链 |
| [`YuuLuo/a7c2-service-preload`](https://github.com/YuuLuo/a7c2-service-preload) | Jiritsu 通道与 `libJit.so` 加载入口（CC BY-SA 4.0） |
| [`MDHan/a7c2-fl23-macos`](https://github.com/MDHan/a7c2-fl23-macos) | A7C II FL2/FL3 工具链前端与流程记录 |
| *[`ArcaneLantern/a7iv-open-gate-test`](https://github.com/ArcaneLantern/a7iv-open-gate-test) | A7 IV OpenGate 与 5.7K 录制研究、Jiritsu 运行期补丁 |
| *[`ArcaneLantern/a6700-opengate-test`](https://github.com/ArcaneLantern/a6700-open-gate-test) | a6700 5.7K & 4K Opengate |

