# A6700 卡速限制绕过：V60/V90 门限的定位与解除

V30 卡也能录 XAVC S-I。本文说明该限制的实现位置与解除方式。

## Warning
强行绕过卡速如果录制时卡速可能因为温度而降速，导致不满足最低写入速度要求，那么会导致缓冲区写满后相机直接封包写卡（丢失缓冲区数据），且CP层面死机，需拔电池重启。  

Test: 尝试在31MB/s最低速上录制 XAVC-S-I 4K 600Mbps，录制16秒后相机自动结束录制且卡死在写卡阶段。拔卡后仍然卡在写卡，然后读取文件只保留了10秒。相机拔电重启，本身对相机无危害，但封包错误对卡的影响尚且未知。

## 各规格的卡速要求

索尼官方给出的对应关系（[可使用的存储卡](https://helpguide.sony.net/ilc/2320/v1/zh-cn/contents/0002H_usable_memory_card.html)，Proxy 关）：

| 文件格式 | 最大码率 | 官方要求 | 换算写入 |
|---|---:|---|---:|
| XAVC S-I 4K | 600 Mbps | SDXC V90 或更高 | 75 MB/s |
| XAVC S-I HD | 222 Mbps | SDXC V90 或更高 | 28 MB/s |
| XAVC HS 4K | 280 Mbps | SDXC V60 或更高 | 35 MB/s |
| XAVC S 4K | 280 Mbps | SDXC V60 或更高 | 35 MB/s |
| XAVC S HD | 100 Mbps | SDHC/SDXC U3 或更高 | 12.5 MB/s |

S&Q 慢动作/快动作会抬高码率：

| 文件格式 | 最大码率 | 官方要求 | 换算写入 |
|---|---:|---|---:|
| XAVC S-I 4K | 600 Mbps | SDXC V90 或更高 | 75 MB/s |
| XAVC S-I HD | 445 Mbps | SDXC V90 或更高 | 56 MB/s |
| XAVC S 4K | 560 Mbps | SDXC V90 或更高 | 70 MB/s |
| XAVC HS 4K | 500 Mbps | SDXC V90 或更高 | 63 MB/s |
| XAVC S HD | 500 Mbps | SDXC V90 或更高 | 63 MB/s |

换算关系是 `MB/s = Mbps / 8`。V 标本身也按写入速度定：V30 = 30 MB/s（240 Mbps）、V60 = 60 MB/s（480 Mbps）、V90 = 90 MB/s（720 Mbps）。

### 固件里的实际判定

固件不看上面这张表，而是按 `VQuality + 帧率 + 10bit` 查六张速率表，得出 class 后比阈值。码率取自 `mpr::pconv::GetVideoBitrate`（`libmpr.so+0x27649C`，查表 `0x2908B8`）。

| 格式 | 规格 | 实际码率 | class | 阈值 |
|---|---|---:|---:|---:|
| XAVC S-I HD | 1080p 24/30p | 89 Mbps | 4 | 600 Mbps |
| XAVC S-I HD | 1080p 50/60p | 111 Mbps | 4 | 600 Mbps |
| XAVC S-I HD | 1080p 100/120p | 185 Mbps | 4 | 600 Mbps |
| XAVC S-I HD | 1080p 高码率 | 222 Mbps | 4 | 600 Mbps |
| XAVC S-I 4K | 4K 24/30p | 240 Mbps | 4 | 600 Mbps |
| XAVC S-I 4K | 4K 50/60p | 300 Mbps | 4 | 600 Mbps |
| XAVC S-I 4K | 4K 100/120p | 500 Mbps | 4 | 600 Mbps |
| XAVC S-I 4K | 4K 高码率 | 600 Mbps | 4 | 600 Mbps |
| XAVC HS 4K | 最低档 | 30 Mbps | 1 | 80 Mbps |
| XAVC HS 4K | 中档 | 100–200 Mbps | 2 | 120 Mbps |
| XAVC HS 4K | 高档 | 280 Mbps | 3 | 400 Mbps |

class 阈值在 `0xE3F0C` 内以立即数展开：`0x50`(80) / `0x78`(120) / `0x190`(400) / `0x258`(600) / `0x546`(1350) / `0xA8C`(2700)，单位 Mbps。

两条 Intra 表的 10bit 项全部落在 class 5，阈值 1350 Mbps（约 169 MB/s）。

### 差距在哪

官方表与固件判定不完全一致。官方要求 S-I HD 用 V90（90 MB/s），固件实际只查 class 4（600 Mbps，即 75 MB/s）——固件比官方宽松。

S-I 4K 的 600 Mbps 档则刚好卡在边界上：码率 600 Mbps 与 class 4 阈值 600 Mbps 相等，而判定是 `ok = (阈值 < 实测速率)`，严格小于，等于门槛反而不通过。

更大的差距在低档位：XAVC S-I HD 1080p 30p 实际只有 89 Mbps（约 11 MB/s），固件却要求 600 Mbps（约 75 MB/s）。索尼按整族统一设阀，不按实际码率分档。这段差距就是补丁要消掉的部分。

---

## 限制的来源

选 XAVC S-I 4K / HD，或者开 S&Q 高帧率，A6700 会先检查卡的 Video Speed Class。V30 直接拒绝，提示「由于卡速度限制，无法录制该格式」。

判定依据是卡的 V 标，不是实际写入速度。

一张合格的 V30 卡持续写入可达 60~85 MB/s。XAVC S-I HD 码率 89~185 Mbps，折合 11~23 MB/s；S-I 4K 24p 约 240~300 Mbps，30~38 MB/s。物理带宽足够。索尼按认证等级一刀切，是因为无法保证任意卡在高温下不掉速。

因此这不是编码问题，是准入判定问题。编码器与封装器都不需要改动，只需定位执行判定的位置。

---

## 判定位置

卡速判定在 `libmpr.so`，不在应用层。

```cpp
// libmpr.so + 0x0E3F0C
int RateCheck::GetMediaTransferRateInh(
    char mediaKey, mpr::movieRecParam param,
    bool& ok, InhBit& inhBit);
```

全库只有两处调用它：

1. `mpr::GetMovieRecStatus`（`0xD6F08`）—— 向上层汇报媒体状态
2. 录制主循环（`0x1092A0`）—— 维护槽位标志

一个入口，两个出口，没有旁路。

### 判定流程

函数开头先挡掉不认识的介质：

```
w4 = mediaKey - 0x41
if (w4 > 0x19) → 返回 3, InhBit = 0x400
```

然后按 `param+0x3C` 分 S&Q 和普通两条路，两个派发函数都再看 `param+0x174`（文件格式 Enum）选表：

| Enum | 格式 | 表 |
|---|---|---|
| 0, 2 | XAVC S 4K | `Normal_AVC_Long_4K`（18 项） |
| 1, 3 | XAVC S HD | `Normal_AVC_Long_FHD`（16 项） |
| 4 | XAVC HS 8K | `Normal_HEVC_Long_8K`（8 项） |
| 5, 7 | XAVC HS 4K | `Normal_HEVC_Long_4K`（20 项） |
| 9 | XAVC S-I 4K | `Normal_AVC_Intra_4K`（10 项） |
| 10 | XAVC S-I HD | `Normal_AVC_Intra_FHD`（10 项） |

六张表对应索尼六种文件格式。每条表项 12 字节：`{u32 VQuality, u32 fpsClass, u8 tenBitFlag, u8 rateClass}`。`VQuality` 对 `param+0x178`，`fpsClass` 是 `GetFrameRateClass(param+0x17C)` 的返回值。三个键命中一条，取出 `rateClass`。

`rateClass` 换算成阈值和阻断位：

| class | 阈值 | InhBit | 原因码 |
|---:|---:|---|---:|
| 1 | 0x50 (80) | 0x800 | 2 |
| 2 | 0x78 (120) | 0x1000 | 3 |
| 3 | 0x190 (400) | 0x2000 | 4 |
| 4 | 0x258 (600) | 0x4000 | 5 |
| 5 | 0x546 (1350) | 0x8000 | 6 |
| 6 | 0xA8C (2700) | 0x8000 | 6 |

判定就一行：`ok = (阈值 < 实测速率)`。

### 表项未命中的处理

派发函数返回 0 时 `w21 = -1`，`cmp w21,#5` 是无符号比较，`-1` 当 `0xFFFFFFFF` 看必然失败，落到 `0xE4148`：`InhBit = 0x400`（原因码 1）、返回 3。

VQuality / fpsClass / 10bit 的组合只要不在表里，照样 inhibit，只是原因码从「卡速不够」变成「未知档位」。XAVC HS 4K 那张表的 fpsClass 只覆盖 0、3、4 三档，其它帧率组合会直接撞上这条兜底。

### V60/V90 的触发范围

`0x4000` 和 `0x8000` 只有 `Normal_AVC_Intra_4K` 和 `Normal_AVC_Intra_FHD` 能产生。

XAVC HS 4K 那张表最高 class 3，最坏情况给 `0x2000`（原因码 4）。XAVC HS 8K 最高也是 class 3。

也就是说，V60/V90 提示只会在 XAVC S-I 上出现。

---

## 补丁

判定入口唯一，直接在该处修改。

`0x0E3F0C` 开始的 5 条指令：

```arm64
0x0E3F0C: sub  sp, sp, #0x280          ; D10A03FF
0x0E3F10: uxtb w0, w0                   ; 53001C00
0x0E3F14: sub  w4, w0, #0x41            ; 51010404
0x0E3F18: stp  x29, x30, [sp, #-0x40]!  ; A9BC7BFD
0x0E3F1C: uxtb w4, w4                   ; 53001C84
```

换成：

```arm64
0x0E3F0C: mov  w4, #1        ; 52800024
0x0E3F10: strb w4, [x2]      ; 39000044   *ok = 1
0x0E3F14: str  wzr, [x3]     ; B900007F   *inhBit = 0
0x0E3F18: mov  w0, #0        ; 52800000   return 0
0x0E3F1C: ret                ; D65F03C0
```

20 字节，原地替换。文件大小不变，段边界不变，重定位表不变，ELF 头一个字节都不动。

效果是 `0x800 / 0x1000 / 0x2000 / 0x4000 / 0x8000` 这五个位一个都不会产生。上层读到的原因码本来就是 0，弹窗逻辑不用管。

实机结果：关机正常，V30 卡录 XAVC S-I All-Intra 成功，MP4 索引和封装正常。

---

## 成片完整性的原因

卡速限制属于准入判定，不参与编码。

码率、GOP 结构、图像缓冲分配、MP4 的 `moov/tkhd/stco/stsz` 元数据，全部由 `g_videoParamComb` 和硬件编码器管线决定。SD 卡的 V 标不进这些计算。

只要卡的持续写入能力足够，`Fsys` 就会把缓冲持续刷入，封包器在录制结束时正常关闭索引。

---

## 固化到 Flash

RAM 中的修改断电即失效。要让它在冷启动后仍生效，需要写入 Flash。

目标文件 `/usr/lib/libmpr.so`，位于 `/dev/nflasha15` 分区（ext2/ext4，启动后以 `ro,sync` 挂载在 `/usr`）。不涉及 CP 侧。

写入流程：

1. `statfs("/usr")` 确认文件系统类型，保留原挂载标志 `MS_REMOUNT` 切为可写
2. `pwrite` 写入 20 字节 + `fdatasync`
3. 立即 `MS_REMOUNT|MS_RDONLY` 收回写权限
4. `O_DIRECT` 回读校验，绕过 PageCache 确认数据落到物理介质

写入前需将目标页从 file-backed 转为私有页，避免当前进程崩溃；同时在 `/tmp/jiritsu/` 写入包含前后哈希的 journal，供回滚使用。

回滚只需将原厂 20 字节写回。另有两条独立退路：USB MSC 服务由独立的 `im.elf` 提供，不依赖 `libmpr.so`；官方 `BODYDATA_a6700.DAT` 会整分区重刷。

`appFw.so` 侧内置 6 个已知基线（stock / fl23 / cineei_v3 / v12 / bb / v14），可直接叠加在 Cine EI 之上。

### WBI 快照的影响

A6700 关机时会把内存快照存到 `/dev/nflasha5`。如果开机时内核从旧快照加载原版 `.so`，Flash 中的修改不会生效，需要清除一次 WBI 头。

已固化 Cine EI 的机器不需要此步骤：`wbiclear` 已执行过，写入后拔插电池即可生效。

---

## 卡速实测

判断一张卡能不能承受目标码率，应以 Direct I/O 测得的持续写入为准，而不是卡上的 V 标。Direct I/O 绕过系统缓存，测的是硬件实际吞吐。

需注意读卡器瓶颈：USB 2.0 读卡器的写入上限约 20~23 MB/s、读取约 31 MB/s，这是协议限制而非卡的问题。插回机身走原生 UHS-II / SDR104 后，合格的 V30 卡可达 60~85 MB/s。

---

## 已知边界

- 补丁放开的是**所有格式**的卡速检查，不只是当前使用的档位。卡速实际不足时相机仍会开始录制，掉帧由使用者承担。
- 4K 120p S-I 的码率超出多数 V30 卡的持续写入能力。
