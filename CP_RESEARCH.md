# CP RAM 读取研究

日期：2026-10-28（实机打通）。范围：A6700 的 CP（co-processor，运行 `cpapp-b.bin` 的对端芯片）内存读取。本文总结机制、内核侧依据、实机验证结果与使用限制。**全部工作与配套脚本对 CP 侧只读**；DMA 写方向存在但本文不予展开，另立课题。

## 背景与结论

A6700 的相机固件分为两侧：AP（AArch64 Linux，`CXD90057` 系列 SoC，运行 `lib_appFw.so`）与 CP（裸机芯片，运行 ARM/Thumb 镜像 `cpapp-b.bin`）。两侧通过 PCIe 点对点链路互连，AP 作为 root complex，CP 以单功能端点设备出现。

结论：**AP 用户态即可读取 CP 地址空间中的任意物理地址，CP 不参与、不感知**。实现入口是 AP 内核自带的 `pciedma` DMA 驱动的用户态 ioctl 接口，其传输参数为原始物理地址且无地址校验。

## 1. 机制

依据是 Sony 为 A6700 发布的 GPL 内核源码（`20230712_ILCE-6700`，AP SoC 平台 `cxd900xx`）与实机提取的内核模块，每条均可复核：

| # | 事实 | 依据 |
|---|---|---|
| 1 | AP 内核内置 `pciedma` 驱动，管理 5 条 PCIe 通道 | `drivers/misc/pciedma/`：`PCIDMA_PCIE_I/M0/M1/C/N` 五通道 |
| 2 | CP 以 PCI 设备 `1556:1100` 枚举；`pci_ep_cp.ko` 绑定该设备并在 probe 中调用 `pcidma_connect(ch, 1)` 点亮 DMA 通道 | 实机 `/kmod/pci_ep_cp.ko`：模块别名 `pci:v00001556d00001100sv*sd*bc*sc*i*`；`.rela.text` 重定位反解 |
| 3 | 对端设备呈 64 GB BAR 且 AP 将其固定分配在 PCI 总线基地址 0 —— PCIe 总线地址等价于 CP 物理地址 | `drivers/udif/mach-cxd900xx/pcie.c:54`：`#define BAR_SIZE 0x1000000000ULL /* 64GB */`，`pcibios_assign_resource` 对该尺寸钉死基址 |
| 4 | 消息传输层 `pcie-if.ko` 的 DMA 走同一内核 API `pcidma_transfer_async`，使用 `pcie_ch=PCIDMA_PCIE_I(0)`、`dmac_ch=0` | `pcie-if.ko` 导入符号与调用点反汇编（`pcie_ch` 写 0） |
| 5 | DMA 请求以裸物理地址描述源与目的；`dir=RD` 时源即远端（CP）地址，非链表模式下直接写 `DMA_SRC/DMA_DST` 寄存器，**无任何地址白名单** | `drivers/misc/pciedma/pcie_dma.c:82-113`，`include/linux/pcidma.h` |
| 6 | 用户态接口：字符设备 `/dev/pcidma`（主 240 次 1），ioctl `_IOW('P', 1, struct pcidma_param)` 将参数**逐字节透传**给 DMA 引擎 | `mach/udif/devno.h:65`；`drivers/misc/pciedma/cdev.c:48-105` |
| 7 | 内核对每次 ioctl 自带 30 秒超时 | `pciedma/config.h`：`#define PCIDMA_TIMEOUT 30 /* sec */` |

调用链读法：ioctl（`dir=RD`，`src=CP 物理地址`，`dst=AP 物理地址`）→ AP 的 DMAC 向 PCIe 发起读事务 → 命中 CP 的 64 GB BAR（总线地址 0 起）→ CP 互连接受并返回内存数据 → 写入 AP 内存。

与既有消息通路（OSAL → pcieif → PCIe 的 OSAL 消息链，见 `DEVICE_CHANNEL.md`）的区别在于**源地址选择权**：消息通路中 CP←→AP 各自决定读什么，而此接口把"读哪个地址"交给了 AP。

## 2. 参数

```c
struct pcidma_param {
    u8 pcie_ch, dmac_ch, dir, pad;
    u32 size;
    u64 src, dst;        /* 原始物理地址 */
};
#define PCIDMA_TRANSFER 0x40185001u   /* _IOW('P', 1, struct pcidma_param) */
```

实机打通所用的参数组合：

| 字段 | 值 | 说明 |
|---|---|---|
| `pcie_ch` | `0`（`PCIDMA_PCIE_I`） | CP 端点所在控制器 |
| `dmac_ch` | **`7`** | **禁止用 0**：0 号是 `pcie-if` 消息传输的活通道，实测共用会把链路楔死（见 §5） |
| `dir` | `0`（`PCIDMA_RD`） | 远端读入本地 |
| `src` | CP 物理地址 | CP 的虚拟地址即物理地址（扁平映射）；镜像装载基址 `0x108000` |
| `dst` | AP 物理地址 | 申请匿名页后读 `/proc/self/pagemap` 取 PFN 换算 |

## 3. 读取点的核对

CP 镜像 `cpapp-b.bin`（SHA-256 `61b7fe5c0ce227ea865d799b81fe77ae359b7c5dcd39d13c9c2241d9cad23559`）在运行期的虚拟地址 = 文件偏移 + `0x108000`。以下点已在实机读取并核对：

| CP VA | 内容 | 核对方式 |
|---|---|---|
| `0x55E904` | 字符串 `PCIeIF Trigger to other chip` | 与镜像文件字面比对（自检锚点） |
| `0x015BF430` | movie-size 状态区（+0xE0 关联字段；实测含 `0x3E8` 与双指针 `0x10201830`） | 实机读数 + CP 侧静态分析（本仓库 codes 系列笔记） |
| `0x016962D0` | 指针区 | 实机读数（实数据） |
| `0x01696150` | 全局区 | 实机读数（实数据） |

单块建议 32–64 B；扫描大范围时逐块推进。

## 4. 执行方式

配套模块与宿主脚本见 [`CP/README.md`](CP/README.md)。宿主侧执行与仓库其它工具一致：走 [`wbi/host/jiritsu.py`](wbi/host/jiritsu.py) 的文件通道与 [`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md) §3 描述的加载链：

```
staging：/tmp/jiritsu 下写暂存 .so → 读回校验
触发  ：写 /tmp/jiritsu/libJit.so → 固件 dlopen 执行模块入口
结果  ：模块写结果文件 → 主机读回
```

模块内部要点：

- ioctl 在 `clone(SIGCHLD)` 子进程中执行，父进程限时观察后无条件写结果文件——内核 30 s 超时 + 10 s 观察窗口双保险，任何失败都会留下报告；
- `dst` 所用匿名页先填充哨兵字节再读 PFN，便于判别"DMA 是否真的写进来"；
- 每次成功读取都会落盘一份 hex 报告，便于离线比对。

## 5. 实际教训（都付过代价）

1. **`dmac_ch=0` 会楔死整条链路**。初次尝试把读取 DMA 排进了 `pcie-if` 正在使用的 0 号通道，之后 Jiritsu 会话整体失聪——连零 ioctl 的纯诊断模块都收不到任何结果。复盘特征：轮询窗口内连模块自身写的第一个结果文件都未出现，说明传输层活着但加载/回传链路已坏，而非模块死循环。此后规则：**只用 7 号通道**，每轮失败后完整断电重启再继续。
2. 诊断面板的正确路径是 **`/proc/driver/pcie/{0..4}/dma`**（`proc.c`：`PROC_PCIE_FMT "driver/pcie/%d/dma"`），每通道显示 `state`（`idle/busy/----`）、`ready`、`pending`、`LL/NL`、`abort/go/done` 计数。加载链是否正常、通道是否被 `pcidma_connect` 点亮、上一次读取是否留下残留，看它即可。
3. 同一进程反复重写 `libJit.so` 触发是运行期行为，断电后不会自动执行（见 `DEVICE_CHANNEL.md` 冷启动结论）；因此楔死状态的清理只能靠断电。
4. `pciedma` 的 abort 路径（`ERR:PCI*:DMA#*:abort*`）用于读未映射地址时是"返回错误"而非崩溃，但连续大量 abort 会拖慢中断线程——试探性扫描应小步分块。

## 6. 限制与风险

- 本机制天然包含写方向（`PCIDMA_WR`，local→remote）。本文及配套脚本**未启用**它；对 CP 内存做写入会直接改写运行状态，风险等级完全不同，须另行设计与评审。
- `dst` 物理页未 pin（相机无 swap、页不迁移为现实前提）；大块读取前应自行评估。
- 读取与 CP 的实时活动异步进行，读到的是瞬时快照；对时序敏感的状态应配合消息通路的时间戳使用。

## 参考

- Sony GPL 内核源码（A6700）：`https://prodgpl.blob.core.windows.net/download/DI/20230712_ILCE-6700/linux-kernel.tar.gz`
- 本仓库：[`DEVICE_CHANNEL.md`](DEVICE_CHANNEL.md)（Jiritsu 通道与模块加载）；[`CP/README.md`](CP/README.md)（配套脚本与使用说明）；`codes.md` / `codes_cpdex.md`（CP 侧函数与状态定位，尚未随主分支发布时以仓库更新为准）
