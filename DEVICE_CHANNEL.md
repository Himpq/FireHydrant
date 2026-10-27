# Jiritsu 通道与固件内存访问

主机与 ILCE-6700 之间通信通道的构成，以及相机应用进程内的模块如何定位并读写固件内存。

## 1. 背景

索尼相机固件内建一个名为 **Jiritsu** 的诊断组件，提供一组厂商扩展的文件服务命令，工作目录为 `/tmp/jiritsu`。该目录位于一块专用分区，属 flash 而非内存盘，因此其中的文件在断电后仍然保留。

除文件读写外，已在运行中的系统上确认：通过该服务写入固定路径 `/tmp/jiritsu/libJit.so`，会触发共享对象加载并执行入口函数。这构成了向应用进程注入代码的入口。

2026-10-24 复核：用户完整断电后，boot_id 改变、单调运行时间重置，但留置的有效探针没有自动执行。本文原先「初始化阶段会加载」的说法撤回；目前确认的是运行期写入触发。详见[冷启动探针实机结论](scripts/CINEEI_BOOT_CONTROL_RESULT_2026-10-24.md)。

与之配合的是厂商扩展的 USB 命令，用于在常规大容量存储模式之外建立带认证的会话。二者结合，可以在不修改任何固件文件的前提下，于运行时观察和调整固件行为。

## 2. 通信建立

```
USB 枚举 → 识别相机（大容量存储类身份）
  → 握手与认证（两轮）
  → 相机切换为服务身份，再次认证
  → 进入 Jiritsu 命令循环
```

- 服务身份在 Windows 下通常需要单独绑定驱动（如 Zadig），否则不会出现；
- 可通过只读方式读取 `/usr/lib/appFw.so` 并计算哈希，用于确认机身固件版本；
- 部分「查询相机信息」的扩展命令在 A6700 上无响应，调用会使端点进入异常状态。

## 3. 模块加载

### 3.1 固件侧实现

入口在 `/lib/libJiritsu.so`（A6700 `nflasha15` 中 inode 24547，233,416 字节；其中的字符串按 `VA = 文件偏移` 排布）：

```text
0x33d01  [JIT] jit_thread B.
0x33e67  [JIT] ramdisk_mount B. / E.
0x33e7f  [JIT] libJit_so copy B.
0x33e98  /tmp/jiritsu/ramdsk_libJit/libJit.so      ← 实际被 dlopen 的副本
0x33f05  [JIT] libJit_so dll open B.
0x33f22  dlopen err pErr
0x33f4f  [JIT] libJit_so method load CreateScriptI B.
0x33f7d  CreateScriptI
0x33f8b  sym create err pErr
0x33fcd  [JIT] libJit_so method run CreateScriptI B.
0x34027  [JIT] libJit_so method load LoadScriptI B.
0x3409d  [JIT] jmode open B.
```

流程：

```text
1) 收包，挂载 RAM_DISK
2) 把 /tmp/jiritsu/libJit.so 复制到 /tmp/jiritsu/ramdsk_libJit/libJit.so
3) dlopen 该副本
4) dlsym("CreateScriptI") 并调用；随后可选 dlsym("LoadScriptI")
5) jmode open
```

模块为 AArch64 共享对象，以 `-shared -nostdlib -fPIC -Wl,-init,_init` 构建，入口为 `_init`，不链接运行库，读写通过裸系统调用完成。主机接口单次上传上限 65,536 字节。

- 没有 `CreateScriptI` 时加载器记录 `sym create err pErr` 后继续，入口 `_init` 已经执行完毕，属正常情况；
- `libJit.so` 是服务的固定触发路径，写入前归档已有文件，使用后清理本次模块；
- 残留文件会跨完整断电保留；文件保留与自动执行是两件事，冷启动不会自动执行留置的模块。

### 3.2 传输：一次会话完成

推荐在一次认证会话内完成全部步骤：

```text
with Session() as s:
    prior = s.read("libJit.so", missing=True)      # 归档旧触发
    s.temp("stage.so", blob)                       # 暂存
    assert s.read("stage.so") == blob              # 回读校验
    s.temp("libJit.so", blob)                      # 触发（写调用会返回）
    for _ in range(120):                           # 同一会话内轮询
        raw = s.read("result", missing=True)
        if raw: break
        sleep(0.25)
    s.temp("libJit.so", remove=True)               # 清理本次文件
    s.temp("stage.so", remove=True)
```

实测：`apply`、`status`、`reset`、`forceundo` 四个操作连续执行，appFw 的 `pid` 与 `appfw_base` 全程不变，说明触发写不会重启进程，同一会话可以继续读结果。写触发后相机离开 MSC、显示回放界面、再回到 MSC，期间个别读请求会返回认证错误，重试即可。

若把触发写放在独立子进程并在超时后终止该进程，主机会在服务处理期间断开 USB 句柄，相机可能停在回放界面并需要手动断电。

### 3.3 结果文件

结果写在 `/tmp/jiritsu/<name>.result`，主机用 `s.read("<name>.result")` 读取。

若把结果写到 `/tmp` 根目录再用 `/tmp/jiritsu/../../tmp/...` 回退读取，路径本身仍然可读，但文件离开了服务的工作目录，归档、清理与路径前缀检查需要单独处理。

同一路径被重复打开写入（例如每个阶段写一次进度标记）时，每次都会新建一次服务调用；单次收尾写入更简单。

### 3.4 清理

只删除本次写入的文件，删除前比对内容，避免误删其他实验留下的模块。`/tmp/jiritsu` 位于 flash 分区，断电不会清除，需要显式删除。

## 4. 文件读取

Jiritsu 的路径前缀检查在主机侧客户端完成，相机端不做路径规范化，因此以绝对路径可以访问实际根目录下的文件。

通道不支持目录枚举，只能按已知文件名读取。

## 5. 固件内存访问

模块不加载固件文件。固件本身已运行于同一进程，模块只需定位其映射位置。

**定位基址**：读取 `/proc/self/maps`，解析映射区间，取 `/usr/lib/appFw.so` 对应映射项的起始地址作为 `base`。

**偏移换算**：该库为 ELF，含两个 LOAD 段。只读段（代码与常量）满足 `runtime = base + file_offset`；可写段（数据与重定位数据）另有固定段间差值。反汇编工具中显示的地址使用第三套基准，三者不可混用。

**读写**：读取时逐字节拼装，避免非对齐访问；写入前为目标页增加写权限，写入后立即恢复；修改代码段时需清理数据缓存并失效指令缓存，否则处理器可能继续执行旧指令。

## 6. 可恢复性

`appFw.so` 以私有映射方式加载。写入操作触发 copy-on-write，修改仅作用于当前进程的私有副本，固件文件与块缓存均不改变；断电后副本消失，下次启动重新读取文件。

因此基于内存的修改不可持久，这一点由映射语义决定，不是额外的保护机制。

## 7. 两类修改的差异

| | 运行时修改 | 文件修改 |
|---|---|---|
| 作用位置 | 进程私有副本 | 固件文件本身 |
| 断电后 | 消失 | 保留 |
| 失效后果 | 当前进程可能异常，重启清除私有 RAM 改动 | 可能导致主程序无法启动；届时 Jiritsu 是否仍可用尚未验证 |
| 恢复手段 | 清理触发文件后重启；撤销快照须匹配当前启动身份 | 需验证独立恢复入口；正常菜单内 SD 更新尚不能充当该证据 |

常规顺序是先在内存中验证行为，确认符合预期后再考虑写入文件；持久化写入放在最后，并预先准备恢复手段。

## 8. 参考资料

### 8.1 固件解包

- **`ma1co/fwtool.py`** —— 索尼相机固件镜像解包工具，最初由 nex-hack 的 fwtool 移植而来。
  支持 FDAT / Msfirm / DSLR / ASH 等多代格式，处理 DAT 解密、分区镜像与文件系统提取。
  本文涉及的二进制分析，其对象（解包后的 `lib_appFw.so`、`/usr` 分区文件、ACD 数据）均由该项目产出。

### 8.2 设备通信

- **`pmca-re`** —— 索尼相机 USB/PTP 协议逆向实现。提供大容量存储类之外的会话建立、
  两轮认证与服务身份切换，是第 2 节通信流程的上游依据。

### 8.3 服务通道与模块加载

- **`YuuLuo/a7c2-service-preload`**（CC BY-SA 4.0）—— 针对 A7C II 2.01 的服务模式研究，
  包含 Jiritsu 通道、`libJit.so` 加载入口与路径解析的上游工作。本文第 1、3 节在此基础上完成 A6700 侧确认。

### 8.4 相关机型的方法记录

- **`MDHan/a7c2-fl23-macos`** —— A7C II FL2/FL3 工具链的 macOS 前端与流程状态机。
  其方法是工具包封装而非独立研究；对本项目的主要价值在于记录了流程顺序与几处已知失败模式
  （例如设置写入阶段中断、仅切电源开关不生效、必须完整断电）。
