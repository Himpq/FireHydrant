# A6700 的 UPM 读模式与 /sabin PSF 表

`upm.ko` 是 A6700 AP 侧的内核模块，位于 `/kmod`，460,368 字节，AArch64，保留完整
`.symtab`。它导出 `k_upm_api_set_imager_read_mode`，函数名容易被理解为设置传感器
读出几何，实际行为是按帧率档位选择 H/V 带宽电平，供 DFS 变频使用。模块内没有
sensor、i2c、SLVS 或寄存器访问相关字符串，与传感器链路无直接关系。

读出档位实际由两条互不交叉的路径决定：

| 输入 | 路径 | 影响 |
|---|---|---|
| 帧率 24 / 30 / 60 / 60i / 120 / 240 | UPM read mode 0..5 → H/V 带宽电平 → DFS | 功耗 |
| 格式 1080 / 4K | `0x4000` 报文的 `MOV_SIZE`(5/3) → CP 选择 imager 记录 | 读出几何 |

4K60 与 HD60 在 CP 侧的判定字段取值相同，原因是这些字段属于帧率一侧，两种格式
帧率一致。格式区分不在这些字段上，而在 `0x4000` 报文的 `MOV_SIZE`，CP 据此选取
`A000`、`A001` 一类的 imager 记录。两条路径没有调用关系，把前者当作后者会得到
错误的读出模型。

## 模块概况

字符串按前缀统计（≥4 字符、去重）：

| 前缀 | 数量 | 内容 |
|---|---:|---|
| `DFS_*` | 132 | 动态频率、电压调节 |
| `UPM_*` | 94 | 状态机与任务 |
| `UPM_TSK_*` | 12 | framework、heat_info、power_status_observer、boot_factor、off_factor、imager_mode、lens_pwr_exc_factor、MSS、APO_observer、TC_power_exclusive_comp、power_save_set |
| `UPM_CMD_*` | 50 | OPEN/CLOSE、ON/OFF、IM_SUSPEND、LIRO_RESUME、SET_BATTERY_OPTION、TIMER、DUMP 等 |
| `k_upm_*` | 294 | 对外 API |
| `DFS_FACTOR_*` | 29 | MOVIE_REC、PLAY、PROXY、HDMI_60_TO_30、EFRAMING、SUSPEND 等 |

分布集中在电源与频率管理，没有外设编程相关的字符串。

## `k_upm_api_set_imager_read_mode` 的实现

函数位于 `0xD0B8`，长度 328 字节。入口做范围检查，仅接受 0..5：

```asm
00d0c4  ldr  w0,[x0,#4]        ; mode
00d0c8  cmp  w0,#5
00d0d8  b.ls #0xd150           ; 只有 0..5 合法
00d150  ldr  x1,=.rodata(0x2f838)
00d154  ldrb w0,[x1,w0]        ; mode → 有符号字节偏移
00d160  br   x0                ; 目标 = 0xd164 + 偏移×4
```

`.rodata:0x2f838` 的跳转表为 `[35, 33, 1, 1, 1, 236]`，对应位号：
mode 0 为位 1，mode 1 为位 2，mode 2、3、4 为位 4，mode 5 跳过。
函数将该位与 `.bss+0x168` 的使能位图相与，再从两张 32 字节表中取对应位置的最小值：

```asm
00d1dc  strb w2,[x29,#0x1d]    ; V ← 表 B 的最小值
00d1f8  strb w3,[x29,#0x1c]    ; H ← 表 A 的最小值
```

两张表由服务 `0x920062`（H）与 `0x920061`（V）在模块加载时填充，未匹配时保留
默认值 `0x60`。

在 unicorn 中单独执行该函数，喂入合成表（`A[i]=0x10+i`、`B[i]=0x20+i`、位图全 1），
结果为：

```text
mode 0     → H=0x10 V=0x20
mode 1     → H=0x11 V=0x21
mode 2/3/4 → H=0x12 V=0x22
mode 5     → 不写任何值
```

六个输入对应三组输出。三组输出与三档带宽一致，与五档分辨率不符。

## 帧率枚举与 mode 编号

模块内的帧率枚举：

```text
BO_STATE_CAM_MOV_FRAME_RATE_24P
                        _30_25P
                        _60_50P
                        _60_50I
                        _120_100P
                        _240_200P
                        _UNKNOWN
BO_ID_CAM_MOV_FRAME_RATE         录制帧率
BO_ID_CAM_MOV_IMAGE_FRAME_RATE   影像 / 读出帧率
BO_ID_CAM_MOV_PROXY_MOV_FRAME_RATE   代理帧率
```

六个有效档与六个 mode 数量相同，但两者之间缺少连接证据：`k_upm_api_set_imager_read_mode`
在 `upm.ko` 内无直接调用（该符号经 `__ksymtab` 导出），其它 `.ko` 也不导入它；
枚举顺序到 mode 编号的映射表未找到；unicorn 结果显示 mode 2、3、4 输出完全相同，
不构成三个独立档位。

可以确定的范围是：模块属于电源与频率管理；`set_imager_read_mode` 接受 0..5，
实际产生三组 H/V，mode 5 为空操作。哪个 mode 对应哪个帧率、调用时机在何处，
均未确定。帧率枚举更可能属于 `BO_ID_CAM_MOV_*` 状态位族，与 mode 编号不在同一
编号空间。

## 与 CP 的通信

配套接口 `k_upm_notify_imager_read_mode_change_for_upm_cp` 位于 `0x026A90`：

```asm
026a94  mov  w2,#0x259
026a98  movk w2,#0x92,lsl#16     ; 载荷首字 0x00920259
026a9c  mov  w1,#0xc             ; 长度 12
026aa8  ldr  x3,[x0]
026ab8  bl   ...                 ; k_upm_snd_cp_comm_c3p
```

`0xD144` 的重定位指向 `k_upm_snd_cp_comm_c3p`，即 CPU 到 CP 的单向通道。
CP 侧对应实现为 `UPM_CP` 任务，起始 `0x0058B0B0`，日志族
`[UPM_CP][DFS]CUR_STATE / CUR_FREQ / SET_MODE / SET_FACTOR LIST`，
接收队列 `0x909207FC`，命令基值 `0x9200015D`。

AP 发送的 `0x00920259` 与 CP 比对的 `0x9200015D` 不在同一命名空间，且
`0x00920259` 在 CP 镜像中没有字面量引用。该消息是否到达 CP 的 UPM 任务无法确认。

## CP 侧的执行层

链路硬件由 CP 驱动控制。CP 固件内 `CetusSlvsec` 出现 31 次、`IMX684` 3 次、
`ImagerDrvParam` 2 次，并有 `"IMG:CetusSlvsecDriver::start ERROR link is not stopped %d %d"`
一类运行期日志。

相关地址位于 CP 的 `0x380000..0x3B0000` 物化段：

```text
0xF1248000   SLVS-EC 接收器 lane 基址
0xF3481020
0xF34904A8
0xF1388004
```

`0x38E53A` 处按 lane 号选择寄存器，步长为 `0x100`：

```asm
movw r3,#0x8000 / movt r3,#0xf124    ; 0xF1248000
add.w r5,r0,r4,lsl #8                ; + lane×0x100
ldr/str r0,[r5,#0x48]
ldr/str r0,[r5,#0x50]
```

AP 侧决定带宽与频率档位，CP 侧配置 SLVS-EC 链路。AP 侧不存在读出几何相关的
配置代码，与此分工一致。

## AP 用户态路径

`lib_appFw.so` 中与读出尺寸相关的符号为：

```text
model::sequence::set_imager_frame_size_info
infra_wrapper_imaging::detail::convert_imager_frame_size_info_notify
PRM_HAITA_..._imager_frame_rate_in_file_format_fhd
PRM_HAITA_..._imager_frame_rate_in_file_format_4k
```

三个转换器（`0x34DD86C`、`0x34DD79C`、`0x34ED6EC`）行为一致，只做枚举校验与复制：

```asm
uxtb w0,w0
cmp  w0,#1  → str w0,[x1,#4]
cbz  w0     → str wzr,[x1,#4]
cmp  w0,#2  → str w0,[x1,#4]
其它        → 断言（__PRETTY_FUNCTION__）
```

`set_imager_frame_size_info` 携带的是 0/1/2 三个枚举值，不含宽高字段。

## `/sabin` 表结构

`/sabin` 目录来自 A6700 的 `/usr` 分区镜像 `nflasha15.img`，ext4 格式
（超级块 `0x438` 处魔数 `0xEF53`），只读挂载后可读。根目录包含
`bin codec data kmod lib local sabin sbin scenario share tool wifi-scripts wlan`；
`/kmod` 为 120 个 `.ko`，`/sabin` 为表文件，两份清单随文附上。

电影读出的 PSF 表按宽度分五档：

```text
dcv_psf_table_movie_2k.bin       33,408 B
                _movie_3k.bin    33,408 B
                _movie_4k.bin    33,408 B
                _movie_6k.bin    33,408 B
                _movie_8k.bin    33,408 B
                _movie_hdmiraw.bin   HDMI RAW 抽头专用
                _still.bin       63,608 B
                _still_macro.bin
```

五张 movie 表大小相同、MD5 不同，属同一布局的五组系数：

```text
2k  e72ab27abfd05babfe91411062bd747e
3k  4954a093951f0f75818bb1fc6f1f1e57
4k  89035256c537192a318442d52ace688b
6k  32eb84fd3080193f929443ae1b4abbec
8k  cbf0d8bcacb04484b67bfd87e59f52b8
```

表头三段在 2k、4k、6k 上相同（`"imager_test"`、`"lens_test_5"`、`"20190403_2017051"`），
其后为系数。表内嵌有与 CP 侧一致的分档门限（u16 计数）：2k 表含 3584×378，
3k 表含 4352×378，6k 表含 4352×252、5120×126、7168×252，8k 表含 3584×63。
该组数值与 CP 中 `0x906D72` 使用的 7168 / 5120 / 3584 / 2560 对应，可确认五张表
是 CP 选择 PSF 时的系数来源。

同目录另有 `bining` 系列：

```text
spica_cetus_mov_bining_c_tap_step1.bin   42,504 B
spica_cetus_mov_bining_y_tap_step1.bin   51,000 B
spica_cetus_mov_c_tap_step1.bin          42,504 B
spica_cetus_mov_y_tap_step1.bin          51,000 B
```

binning 与非 binning 各有一套，MD5 不同，文件开头即为系数，无文件头。
两套系数的存在表明固件具备两种读出形态的配置，不能据此判断 HD 实际使用哪一套。

## PSF 表的选择逻辑

六条 movie 路径由同一函数 `0x906D72` 构造，唯一调用点在 `0x919520`：

```asm
00906d7c  ldrb.w r0,[r1,#0x394]
00906d80  add.w  r3,r1,#0x340
00906d86  movw   r0,#0x745b / movt #0xf5     ; 默认 = movie_2k
00906d8c  addne.w r3,r1,#0x37c
00906d94  ldr    r1,[r3]                     ; 描述符指针
00906d96  cbz    r1, <用默认 2k>
00906d98  ldrh   r5,[r1,#8]                  ; 读出宽度
00906d9a  cmp.w  r5,#0x1C00                  ; 7168
00906dc0  cmp.w  r5,#0x1400                  ; 5120
00906de2  cmp.w  r5,#0xE00                   ; 3584
00906e04  cmp.w  r5,#0xA00                   ; 2560
```

分档关系为：宽度大于 7168 取 8k 表，大于 5120 取 6k，大于 3584 取 4k，
大于 2560 取 3k，其余取 2k；2k 同时是默认分支。各比较受 `[0x017DF390+0x21]`
的位控制，8k 对应 bit5、6k 对应 bit4、4k 对应 bit3、3k 对应 bit2。另有独立分支：
`[0x017E9590+0x4F]==1` 且 `[r2]==2` 时改用 `_movie_hdmiraw.bin`。

调用点参数：

```asm
00919514  ldr    r0,[r4,#0xc]
00919516  mov    r1,r6                 ; r6 = base + 0x1284
00919518  add.w  r2,r0,#0x128
0091951c  add.w  r0,r8,#0x78           ; 输出缓冲（0x60 槽位）
00919520  bl     0x906d72
```

选择只依赖一个量：

```text
width = *(u16*)( *(void**)( obj + (flag ? 0x1600 : 0x15C4) ) + 8 )
```

`0x906D72` 的 `r1` 为调用点传入的 `base + 0x1284`，`base` 取自
`*(0x016A3CF0 + 0xC)`。`0x016A3CF0` 是成像上下文全局，镜像内引用一百余处，
集中在 `0x4Cxxxx` 函数族，与构造 snapshot 的 `0x01694C60` 属同一批代码。
若 `base` 为备份对象 `0x01699C90`，则 `base+0x15C4` 落在 `0x0169B254`，
处于已验证可读区间。

剩余工作是两级指针追读：`0x016A3CF0+0xC` → `base` → `base+0x15C4` → `+8`。
MSC 态下这两级指针可能被清零（与 `0x0201` arena 的行为一致），静态分析无法继续，
需在录制态读取。

## 与 HD 读出方式的判定关系

HD60 实际加载的 PSF 表可以直接区分其读出形态。若走 binned 读出，输入宽度落在
3k 或 2k 档，使用 `dcv_psf_table_movie_3k.bin` 或 `_2k.bin`，并配
`spica_cetus_mov_bining_*` 系数；若走 6K 超采，应使用 `_6k.bin` 与非 binning 的
`spica_cetus_mov_*`。两组组合在文件层面可区分，无需依赖格式枚举。

读取方式为在 HD60 录制态取 CP 侧 `0x906D72` 的输出槽（0x60 字节），或读取
`0x016A3CF0+0xC → base → base+0x15C4 → +8` 的 width。限制在读取时机：
`/tmp/jiritsu` 的结果回传依赖会话保持，MSC 与 LV 互斥，因此该测量尚未取得结果。

## 未决问题

`0x920061` 与 `0x920062` 两个服务的提供方未定位，即 H/V 两张表的实际数值来源。
扫描器在 `upm.ko` 上可自校验（命中 2 处），在 `nflasha5`（248 MB）与 `appFw`（89 MB）
上为 0 命中，说明提供方不通过指令级比较，可能经 OSAL 注册表分发。

`k_upm_api_set_imager_read_mode` 的调用者未定位。`upm.ko` 内无直接调用，
该符号经 `__ksymtab` 导出，其它 `.ko` 均不导入；符号出现在 `nflasha5` 的 kallsyms 中，
调用方位于内核本体。

`k_upm_snd_cp_comm_c3p` 的实现未定位。全镜像仅 2 处字符串命中，且均为 `upm.ko`
的符号名副本，无模块定义该符号，同样位于内核本体。

WBI2 容器未解析。`nflasha5.img` 头为 `'WBI2'`，WBI1 的字段布局不适用，
按结构特征搜索节表为 0 条；已确认该分区含 195 个 AArch64 ELF 对象及
`/kmod/upm.ko` 路径串。

四项均指向内核本体，而内核由 WBI2 封装。在 WBI2 解析完成前，UPM 这条线可确认的
范围为：模块职责为电源与频率管理，read mode 对应三档带宽，链路硬件由 CP 操作。

## 复现

全过程不需要连接相机。从 `/usr` 分区镜像取出 `/kmod/upm.ko` 后，可直接读取
`.symtab` 列出全部 `k_upm_*`，按前缀分组统计字符串。复现 read mode 行为需反汇编
`0xD0B8..0xD200`，读取 `.rodata:0x2f838` 的 6 字节跳转偏移，并在 unicorn 中按
以下布局加载：

```text
.text   → 0x0
.rodata → 0x100000
.data   → 0x200000
.bss    → 0x300000
```

需手工补两处重定位：`0xD200` 指向 `.rodata+0x6A8`，`0xD208` 指向 `.bss+0x168`。
PC 跳出 `[0xD0B8, 0xD200)` 时按调用处理并直接返回，喂入合成表遍历 mode 0..5 即可。

扫描脚本需修正两处：`int('#0x..')` 的语法处理与 `movs` 误清 pending，否则漏命中。

`/sabin` 侧按文件名与 MD5 核对即可，表内门限值以 u16 计数搜索可与 CP 中的比较
常数对应。
