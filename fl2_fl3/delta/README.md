# CP delta 工具

把 33 窗口的 delta 原地写进 `/cp/cpapp-b.bin`，不传 17.9 MB 整文件。

载荷是同目录的 [`cp.delta`](cp.delta)，9,536 B：原厂 → 42 项候选。

## 构建

```bash
python build.py
```

产出 `build/rampatch_cpdelta.so`，约 6 KB，AArch64、freestanding、入口 `_init`。

## 使用

```bash
python cpdelta.py --mode apply
python cpdelta.py --mode restore
python cpdelta.py --clean
```

`apply` 要求每个窗口都还是 before；`restore` 允许部分应用。传输层在
[`../../wbi/host/jiritsu.py`](../../wbi/host/jiritsu.py)，相机侧模块是
[`cpdelta.c`](cpdelta.c)。

## 判据

`windows=33`、`written=4456`、`RESULT=OK`。

selector0 那 2 B（`0x135F0A`，`00 20 → 00 BF`）不在这个 delta 里，它打在候选之上。
