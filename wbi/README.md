# Jiritsu 模块工具

相机侧的 AArch64 模块，加一套主机 CLI。模块共用一份骨架编译，用 `PATCH_APPLY` 选功能：

- **快照头（WBI）**：读、备份、清零、写回 `/dev/nflasha5` 的前 512 B；
- **AP 程序头**：把 `/usr/lib/appFw.so` 偏移 `0x60` 的 16 字节从扩展值改回基线；
- **CP 模式数据**：把 33 窗口的 delta 原地写进 `/cp/cpapp-b.bin`，不传整文件。

## 目录

```
wbi/
├─ native/
│  ├─ jiritsu_module.c        骨架：syscall、输出缓冲、_init 入口
│  ├─ wbi_hdr_block.c         211 只读：88 B 头 + 分区采样
│  ├─ wbi_rewrite_block.c     212 原样写回 512 B（写通道自检）
│  ├─ wbi_clear_block.c       213 备份 + 清零 512 B
│  ├─ wbi_restore_block.c     214 从备份写回
│  ├─ ap_fl23_revert_block.c  215 程序头 0x4489230 → 0x4488EED
│  ├─ cpdelta.c               独立入口：逐窗口校验后原地写 /cp
│  └─ build.py                clang 构建六份模块
├─ host/
│  ├─ jiritsu.py              Jiritsu 文件通道（15/16/17/18）
│  ├─ wbi.py                  CLI：hdr / rewrite / clear / restore / clean
│  ├─ apfl23.py               CLI：程序头回退 / clean
│  └─ cpdelta.py              CLI：apply / restore / clean
├─ data/
│  └─ cp.delta                9,536 B，33 窗口的 before/after
└─ build/                     编译产物（不提交）
```

## 构建

需要 clang（带 aarch64 target 与 lld）：

```bash
python native/build.py
```

模块是 freestanding 的：`-nostdlib -shared`，无 libc，入口 `_init`（`DT_INIT`），
所有系统调用直接走 `svc #0`。每个模块 9–14 KB，低于通道单文件 65,536 B 的上限。

## 使用

快照头：

```bash
python host/wbi.py hdr        # 只读
python host/wbi.py rewrite    # 原样写回，验证写通道
python host/wbi.py clear      # 备份 + 清零
python host/wbi.py restore    # 写回备份
python host/wbi.py clean      # 清掉相机侧残留
```

AP 程序头：

```bash
python host/apfl23.py         # 写入 16 字节并读回
python host/apfl23.py --clean
```

`apfl23` 只动 `p_filesz` / `p_memsz`；当前值不是 `0x4489230` 时中止，不猜、不覆盖。

CP 模式数据：

```bash
python host/cpdelta.py --mode apply     # 原厂 → 候选
python host/cpdelta.py --mode restore   # 候选 → 原厂（允许部分应用）
python host/cpdelta.py --clean
```

载荷固定为 `data/cp.delta`；`--mode` 只决定写 before 还是 after。

前提：相机进 MSC 模式（`054C:0E76`），认证后出现服务身份 `054C:0336`；
主机侧需要 PMCA-RE 的 `pmca` 包（放在 `host/pmca-re/`、`../pmca-re/`、
`PMCA_PATH` 或已安装）。

## 判据

| 命令 | 期望输出 |
|---|---|
| `hdr` | `magic=57424932`（`WBI2`）、`sections=1314`、`sector=512` |
| `rewrite` | `write=512`、`verify=OK` |
| `clear` | `pre=512`、`backup=ok`、`write=512`、`fsync=0`、`zero=1`、`RESULT=OK` |
| `restore` | `write=512`、`RESULT=OK` |
| `apfl23` | `pre=16`、`write=16`、`read2=16 now=ed8e4804…`、`RESULT=OK` |
| `cpdelta` | `windows=33`、`written=4456`、`RESULT=OK` |

`clear` 之后必须**完整断电**（拔电池），下一次启动才按文件里的段布局加载；`apfl23` 之后同样要断电。

## 风险

1. 清头后不再有快照保护，回退只能靠 USB 通道或官方恢复包；
2. 只在完整断电后才生效，热重启看到的仍是旧状态；
3. 不要在 BIS / 固件更新流程里用它关机——那条路径会走 `warm` 并可能重建快照；
4. `clear` 前确认备份已经落到主机（脚本会打印 `orig512=`，同时相机侧留一份）；
5. `apfl23` 改的是加载器读的程序头，写坏会让 AP 起不来，同样只有恢复包兜底；
6. `cpdelta` 写的是 `/cp/cpapp-b.bin`，`/cp` 只剩约 3 MB，不要在里面另存整文件。

## 实现备注

- 结果通过 `/tmp/jiritsu/rampatch.result` 回传，主机侧轮询读取；
- 已存在的残留文件（`libJit.so`、`*-staged.so`、`rampatch.result`）默认阻止运行，需 `--clean` 或 `--force`；
- 模块里的路径都是写死的常量，不接受参数注入。
