# Cine EI 固化包（V3）

Sony ILCE-6700，固件 2.00。把 Cine EI 永久写入 `/usr/lib/appFw.so`，冷启动即生效。
**状态：已在实机验证**（2026-10-25，冷启动后 Cine EI 正常启用）。

具体补丁、模块、刷机与回退见 `docs/FIRMWARE_AND_FLASH_REPORT_2026-10-25.md`，落点细节见
`docs/CINEEI_PATCH_DETAILS_2026-10-25.md`。

> 下表中标「本地」的条目不在仓库里。清头工具见 [`wbi/`](../../wbi/)。

## 目录

```
cine_ei/
├─ README.md                         分层启用说明（概念）
├─ docs/
│  ├─ PACKAGE_OVERVIEW.md            本文件：包结构 + 快速开始
│  ├─ CINEEI_PATCH_DETAILS_2026-10-25.md        落点细节（段数、字节、hook、程序头）
│  └─ FIRMWARE_AND_FLASH_REPORT_2026-10-25.md   固件构成、刷机流程、回退
├─ images/（本地）
│  └─ CINEEI_V3_OFFLINE_ONLY.bin     89,027,992 B
├─ tables/（本地）
│  ├─ PATCH_TABLE_V3_VS_FL23_2026-10-25.json    211 段 / 827 B（刷机用）
│  ├─ PATCH_TABLE_VS_CURRENT_2026-10-25.json    相对“FL23+扩展头”的 209 段 / 823 B
│  └─ PATCH_TABLE_VS_CURRENT_2026-10-25.md      人读表
├─ modules/（本地）
│  ├─ rampatch_apv3check.so          只读预检（211 段）
│  ├─ rampatch_apv3write.so          FL23 → V3 写入
│  ├─ rampatch_apv3restore.so        V3 → FL23 回退
│  ├─ rampatch_apfl23.so             AP 头 0x4489230 → 0x4488EED
│  ├─ rampatch_wbihdr.so             只读：WBI 头 + 分区扫描
│  ├─ rampatch_wbirewrite.so         WBI 写通道自检（原样写回）
│  ├─ rampatch_wbiclear.so           WBI 头备份 + 清零
│  └─ rampatch_wbirestore.so         从备份写回 WBI 头
├─ tools/（本地）
│  ├─ apv3_run.py                    --mode check|write|restore
│  ├─ apfl23_run.py                  AP 头回退到 FL23
│  ├─ wbihdr_run.py                  只读 WBI 头检查
│  ├─ wbirewrite_run.py              WBI 写通道自检
│  ├─ wbiclear_run.py                WBI 头备份 + 清零
│  └─ wbirestore_run.py              WBI 头写回
└─ backup/（本地）
   └─ wbihdr_backup.bin              512 B 原始 WBI 头
```

## 快速开始

相机进入 USB 服务模式后，在仓库根目录执行：

```powershell
# 1) 首次：让旧快照失效（已做过可跳过）
python FireHydrant/cine_ei/tools/wbihdr_run.py --force
python FireHydrant/cine_ei/tools/wbirewrite_run.py --force
python FireHydrant/cine_ei/tools/wbiclear_run.py --force
# 断电冷启动

# 2) 写 V3
python FireHydrant/cine_ei/tools/apv3_run.py --mode check
python FireHydrant/cine_ei/tools/apv3_run.py --mode write
python FireHydrant/test_scripts/cineei_ap_direct_hash_status.py --capture --usb-wait 180
# 断电冷启动 → 菜单确认 Cine EI

# 3) 回退
python FireHydrant/cine_ei/tools/apv3_run.py --mode restore
```

## 判据

| 检查 | 期望 |
|---|---|
| `apv3_run.py --mode check` | `preflight=211/211`、`RESULT=CHECK_OK` |
| `apv3_run.py --mode write` | `written=827`、`verified=827`、`RESULT=OK` |
| O_DIRECT | 与 V3 记录一致；header 与文件头一致 |
| 冷启动后 | `ssboot state=idle`；`ram_elf_segment_sizes=30924804…`；Cine EI 出现在菜单 |

## 回退阶梯

1. `apv3_run.py --mode restore` → V3 写回 FL23（827 B，预检 + 读回校验）；
2. `wbirestore_run.py --force` → 写回 512 B WBI 头 → 断电 → 回到旧快照启动；
3. 官方 `BODYDATA_a6700.DAT` 更新（不依赖 AP）。

所有回退都需要断电冷启动才生效。
