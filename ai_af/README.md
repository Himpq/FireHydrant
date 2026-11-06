# A6700 主体识别 AF 模型架构

主体识别 AF 由 AP 与协处理器上的 Adam/VIP NPU 协同完成。模型包包含 15 个 NBG 网络，分工覆盖全帧检测、姿态关键点、动物眼部检测、ReID、分割和场景分类。AP 负责 ROI 投影、跨帧跟踪和镜头控制。

## 文档

| 文档 | 内容 |
|---|---|
| [模型架构](docs/ARCHITECTURE.md) | 15 个网络的输入输出、层/算子数量和流水线关系 |
| [NBG 格式](docs/NBG_FORMAT.md) | NA/NBG 容器、节区结构和表项关系 |
| [静态解析](docs/METHODOLOGY.md) | 容器解析、拓扑还原和 I/O 契约建模 |

## 工具

配套工具使用 Python 3.9+，读取 NA/NBG 容器元数据：

```text
tools/
├── nbg_meta.py       # 读取归档、NBG 固定头、I/O 表、Layer 和 Operation
├── nbg_coeff_map.py  # 展开 Operation、Patch 与 LCD 表的引用关系
└── dump_nbinfo.sh    # 提取容器结构摘要
```

Ghidra 辅助脚本位于 [`ghidra/`](ghidra/)。
