# VeriSilicon NBG 与 NA 容器格式

NBG 以 `VPMN` 标识开头，NA 复合归档可包含多个 NBG 流。NBG 固定头记录格式版本、硬件目标和网络规模；其后是版本相关的节区表和网络元数据表。

开源解析器位于 VeriSilicon TIM-VX：

- `TIM-VX/include/tim/utils/nbg_parser/gc_vip_nbg_format.h`
- `TIM-VX/src/tim/utils/nbg_parser/nbg_parser.c`
- `TIM-VX/src/tim/vx/internal/include/interface/ops.def`

## NA 复合归档

NA 头部包含网络条目和偏移信息。固件版本之间的归档表结构存在差异；每个 NBG 子流以 `VPMN` 标识起始，段间可有 64 或 128 字节对齐填充。

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

## NBG 固定头

| 偏移 | 类型 | 字段 | 说明 |
|---:|---|---|---|
| `0x00` | `char[4]` | `magic` | `VPMN` |
| `0x04` | `uint32_t` | `version` | 格式版本 |
| `0x08` | `uint32_t` | `hw_target` | 硬件目标标识 |
| `0x0C` | `char[64]` | `network_name` | 编译时写入的网络名称 |
| `0x4C` | `uint32_t` | `layer_count` | 逻辑层数量 |
| `0x50` | `uint32_t` | `operation_count` | 算子数量 |
| `0x54` | `uint32_t` | `input_count` | 输入数量 |
| `0x58` | `uint32_t` | `output_count` | 输出数量 |
| `0x5C` | `uint8_t[256]` | `feature_db` | 硬件特性配置，版本 `>= 0x10003` |

`feature_db` 保存硬件能力位、像素管线数量、核心数和保留字段。固定头后还包含内存池、AXI SRAM 与 VIP SRAM 配置。

## 动态节区

节区描述符由两个小端 `uint32_t` 值组成：`offset` 和 `size`。解析顺序如下，部分节区按版本启用：

```text
memory_pool_config
axi_sram_config
vip_sram_config              (version >= 0x10008)
input_table
output_table
layer_table
operation_table
LCD_table
LCD
nn_op_data_table
tp_op_data_table
sh_op_data_table
patch_data_table
layer_param_table
sw_op_data_table
hw_init_op_table             (version >= 0x1000C)
ICD_table                    (version >= 0x1000C)
ICD                           (version >= 0x1000C)
ppu_param_table              (version >= 0x1000E)
```

`LCD_table` 保存系数块描述符；`LCD` 保存对应的数据段。每个描述符包含数据偏移和长度。

## 表项结构

### I/O 表

版本 `>= 0x1000b` 的张量描述符通常为 116 字节；较早版本使用 100 或 108 字节结构。

```c
struct nbg_tensor_desc {
    uint32_t dim_count;
    uint32_t dims[6];
    uint32_t data_format;
    uint32_t data_type;
    uint32_t quant_format;
    int32_t  fixed_point_pos;
    float    tf_scale;
    int32_t  tf_zero_point;
    char     name[64];
};
```

### Layer 表

每项 76 字节，包含层名、层编号、算子数量和 UID。

### Operation 表

每项 24 字节，记录算子类型、类型内索引、所属 Layer、状态索引以及关联 Patch 的起始位置和数量。算子类型可与 `ops.def` 对照。

### Patch 表

每项 28 字节，字段包括 `type`、`offset_in_states`、`source_type`、`index`、`orig_base`、`transformed` 和 `name`。`source_type` 区分输入张量、系数块和状态缓冲区。

## 系数块索引

算子通过 Patch 表引用系数块，Patch 的 `index` 对应 `LCD_table` 下标：

```python
for op in operations:
    for patch in patches[op.patch_index : op.patch_index + op.patch_count]:
        if patch.source_type == 5:
            offset, size = LCD_table[patch.index]
            block = LCD_blob[offset : offset + size]
```

多个 Operation 可以引用同一条 LCD 描述符。系数块按 `offset` 和 `size` 从 LCD 数据段定位。

## 网络加载

协处理器加载器解析固定头和节区描述符，申请 DMA 连续内存，将 LCD 数据段复制到 DMA 缓冲区，再把缓冲区地址交给 NPU 硬件任务。运行时的网络参数由容器表项、LCD 数据段和硬件任务配置共同组成。
