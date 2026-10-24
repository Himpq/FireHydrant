# 固件与刷机流程报告：A6700 Cine EI 固化（V3）

日期：2026-10-25
机型：Sony ILCE-6700（A6700），固件 2.00
内核：`4.9.337-scel+`（build 2025-05-24）
结果：**Cine EI 在冷启动后正常启用（实机确认）**

---

## 1. 固件构成（本次涉及的）

| 部件 | 路径/设备 | 大小 | 说明 |
|---|---|---|---|
| 升级包 | `F:\Temp\Downloads\BODYDATA_a6700.DAT` | — | FDAT tar，含 `0700_part_image/dev/nflasha*` |
| AP | `/usr/lib/appFw.so`（`/dev/nflasha15`，ext2） | 89,027,992 B | 应用层主程序，Cine EI 补丁目标 |
| WBI 分区 | `/dev/nflasha5` | 248,637,568 B | 热启动镜像；首 512 B 为 `struct wb_header` |
| CP | `cpapp-b.bin` | 17,892,024 B | 相机处理层，本次未改 |
| 内核 | `vmlinux.bin` | 6,610,952 B | 平铺 AArch64 Image |

WBI 头（88 字节，`include/linux/warmboot.h`）：

```
fVersion[4]  "WBI2" 有效 / "WBI0" 中间态 / "invl" 作废 / "prof"
padding      u32
sections     1314
flag         0（未启用 CKSUM）
resume_vector 0x4004A67F0
dVersion     0x20060224
sector_size  512
r_data_size  248,291,328
kern_start   0
kern_size    0
kern_cksum   0x0F0F0F0F（填充值，非校验）
o_data_size  273,801,216
```

首扇区 88 字节之后全零（`nonzero_after88=0`）。有效性只由魔数决定，没有启用的校验和。

## 2. 关键结论（刷机依据）

1. **零 WBI 头 → 正常冷启动**：在 A6700 2.00 上实测成立（`bootid` 变化、`ssboot state=idle`、内存加载头 = 文件头）。
2. **快照会盖住新 AP**：旧快照恢复的是旧段布局（`0x4488EED`），文件里的扩展头（`0x4489230`）不生效，直到清头冷启动。
3. **AP 写入需要 remount**：`/usr` 默认 `ro,sync`；写入前 `MS_REMOUNT` 改 rw，写完改回 ro。
4. **写后必须 O_DIRECT 复核**：普通读取会命中旧页缓存（表现为“文件还是旧头”），O_DIRECT 才是闪存真实内容。

## 3. 刷机流程（本次实际执行）

前置：相机进入 USB 服务模式（MSC + 服务身份），电池充足，写入过程中不要断电。
工具都在 `FireHydrant/cine_ei/tools/`，模块在 `../modules/`。

### 3.1 第一次：让旧快照失效

```powershell
# 只读确认 WBI 设备与头（可选，但建议先跑一次留档）
python tools/wbihdr_run.py --force

# 写通道自检：把当前 512 B 原样写回并校验（内容不变）
python tools/wbirewrite_run.py --force

# 备份 + 清零 512 B 头（相机侧与本地各留一份备份）
python tools/wbiclear_run.py --force
```

期望 `wbiclear` 输出：`pre=512 magic=57424932`、`backup=ok`、`write=512`、`fsync=0`、`zero=1`、`RESULT=OK`。

然后**断电冷启动**。验证：

```powershell
python FireHydrant/test_scripts/cineei_boot_snapshot_status.py --capture --usb-wait 180
python FireHydrant/test_scripts/cineei_ap_direct_hash_status.py --capture --usb-wait 180
python FireHydrant/test_scripts/cineei_ap_boot_view.py --capture --phase baseline --usb-wait 180
```

判据：`ssboot state=idle`、O_DIRECT 头 = 文件头、AP view 的 `ram_elf_segment_sizes` 等于文件头。

### 3.2 可选：回到 FL23 基线确认底座

```powershell
python tools/apfl23_run.py --force
```

把 `0x60/0x68`（`p_filesz/p_memsz`）从 `0x4489230` 改回 `0x4488EED`，写后读回校验。
断电冷启动后应得到与 FL23 基线记录一致的哈希、AP view `PASS`（disk=BASE, loaded=BASE）。

### 3.3 写入 V3（Cine EI）

```powershell
python tools/apv3_run.py --mode check     # 只读预检 211 段
python tools/apv3_run.py --mode write     # 写入 827 字节
python FireHydrant/test_scripts/cineei_ap_direct_hash_status.py --capture --usb-wait 180
```

期望：

```
check : preflight=211/211  RESULT=CHECK_OK
write : preflight=211/211  written=827  fsync=0  verified=827  remount_ro=0  RESULT=OK
O_DIRECT: 与 V3 记录一致（profile CINEEI_V3_OFFLINE）
```

然后**断电冷启动**，进菜单确认 Cine EI。

## 4. 补丁内容（V3 相对 FL23）

211 段 / 827 字节，全部落在 profile 窗口或 RX 尾岛 `0x4488EED–0x4489230` 内，无越界：

```
native_code      17 段   65 B
ui_source        63 段   63 B
support_source    7 段    7 B
compiled_default  4 段    4 B
static_island   114 段  652 B
rx_tail_island    2 段   30 B
footer            2 段    2 B
```

- 新代码位于扩展出来的 RX 尾段（`0x4489190` 起），入口 hook 在 `0x3304D0C` / `0x3300D44`；
- v1 的崩溃原因正是尾段全零（执行到即取指故障），V2 仍有空洞，V3 补齐；
- 离线 Unicorn：`verify_layout_regression`（NEXT 布局两条 hook 正常返回）+ `verify_reloads`（420 例，118 保留 / 240 纠正）通过。

## 5. 版本锚点

| 版本 | 大小 | 结构性识别 |
|---|---:|---|
| 原厂 stock AP | 89,027,992 B | 未改动；程序头 `0x4488EED` |
| FL23 基线 AP | 89,027,992 B | 原厂 + 112 B 转换窗口；程序头 `0x4488EED` |
| V3（本次刷入） | 89,027,992 B | FL23 基线 + 211 段；程序头 `0x4489230` |

跨版本比对用「程序头 + 段数」这两个结构判据；三者大小完全相同，不要靠文件名或文件大小区分。

WBI 头备份是 512 B 原始扇区（88 B 头 + 424 B 零），同样只在本地留存。

## 6. 回退

| 目的 | 命令 |
|---|---|
| V3 → FL23 | `python tools/apv3_run.py --mode restore` |
| FL23 扩展头 → 旧头 | `python tools/apfl23_run.py --force`（反向：需要另行写入） |
| 恢复 WBI 头（回到旧快照启动） | `python tools/wbirestore_run.py --force` |
| 全部复原 | 官方 `BODYDATA_a6700.DAT` 更新 |

回退后都要**断电冷启动**才生效。

## 7. 风险与注意

- 清头后不再有快照保护，回退依赖 USB 服务可用；USB 也不可用时只能走官方更新。
- 正常关机是 `mem`（不建像）；**BIS/更新流程会走 `warm` 挂起并可能重建 WBI**，不要用它关机。
- 固件升级会覆盖 AP，需要按新版本重新生成补丁表。
- 每次写入前必须 `check`（逐段 before 校验），任何不符即中止，不写半截。

## 8. 未完成验证

- 第二次冷启动后 Cine EI 是否仍启用；
- ISO / Log 面板切换（v1 崩溃点）是否已被 V3 修复；
- BIS/更新路径对 WBI 的实际影响。
