# NBG 静态解析流程

NBG 分析从容器分段开始，依次解析固定头、动态节区、表项关系和 I/O 契约。解析结果可用于还原各网络的接口和计算图连接关系。

## 1. 容器与分段

NA 复合归档以 `NA` 标识开头，内部按对齐规则存放多个 NBG 流。每个 NBG 以 `VPMN` 开头。扫描 `VPMN` 可定位子网络起点，再按 NBG 头部和节区信息整理网络片段。

```python
pos = 0
magic = b"VPMN"
while True:
    pos = data.find(magic, pos)
    if pos < 0:
        break
    next_pos = data.find(magic, pos + len(magic))
    end = next_pos if next_pos >= 0 else len(data)
    yield data[pos:end]
    if next_pos < 0:
        break
    pos = next_pos
```

## 2. 固定头与节区

固定头包含格式版本、硬件目标、网络名、层数、算子数和 I/O 数量。头部之后是由偏移和大小构成的节区描述符。节区集合随版本变化，解析顺序与 `nbg_parser.c` 的读取流程一致。

解析器按版本读取内存池配置、SRAM 配置以及 I/O、Layer、Operation、LCD 和各类算子数据表。每个节区的偏移和大小共同决定其在 NBG 流中的位置。

## 3. 表项关系

I/O 表描述网络输入输出；Layer 表把算子归入逻辑层；Operation 表记录算子类型、所属层和 Patch 范围；Patch 表引用输入张量、状态缓冲区或系数块。

系数块的索引通过 Patch 间接映射到 `LCD_table`：

```text
Operation.patch_index / patch_count
    → Patch[source_type == 5]
    → LCD_table[Patch.index]
    → LCD[offset : offset + size]
```

多个 Operation 可以引用同一个 LCD 条目，因此表项关系需要按索引展开，而不是按算子顺序累加数据长度。

## 4. I/O 契约

I/O 描述符包含维度数量、维度数组、数据格式、数据类型和量化参数。将这些字段用于构造无权重的虚拟网络，可以检查前后级接口的维度衔接、NHWC/CHW 转换、ROI 坐标映射和宿主侧后处理。

虚拟网络保留真实网络的接口关系，以零值或伪随机数据填充张量和系数。它用于跑通从预览帧候选到 ROI、特征和关联结果的宿主侧数据流。
