# openvela 公共仓适配补丁（GD32F407VKT6）

本目录保存火眼项目为跑通 GD32F407 板卡所做的**公共仓改动**，以补丁形式留档，
避免这些改动只存在于本地构建树、`repo sync` 之后丢失。

导出时间：2026-09-11
基线：

- `nuttx`：`dd92bcf425738734d1b8aed09c2bd4dbe3f2e438`（2026-07-10）
- `vendor/gigadevice`：`838aae8ecb474675e1f350528909ddedc7ac741a`（2026-06-09）

## 文件

| 文件 | 对应仓库 | 内容 |
|---|---|---|
| `nuttx-gd32f407-support.patch` | `nuttx` | chip.h / memorymap.h / pinmap.h / progmem.c 的 F407 分发，以及新增的 `arch/arm/src/gd32f4/CMakeLists.txt` |
| `vendor-gigadevice-gd32f407v-start.patch` | `vendor/gigadevice` | `boards/gd32f4/gd32f470v_start/` 的时钟注释修正、USART0 改 PB6/PB7、I2C1 引脚别名、nsh defconfig（168MHz/25MHz 晶振/串口控制台/I2C1/火眼应用） |

## 应用方法

```bash
# nuttx
cd <workspace>/nuttx
git apply /path/to/patches/nuttx-gd32f407-support.patch

# vendor/gigadevice
cd <workspace>/vendor/gigadevice
git apply /path/to/patches/vendor-gigadevice-gd32f407v-start.patch
```

> `nuttx` 补丁里包含一个**新增文件** `arch/arm/src/gd32f4/CMakeLists.txt`，
> 若用 `git apply` 失败，可先 `git apply -p1 --directory=. <patch>` 或直接用 `patch -p1`。

## 重要提醒

这两个仓库属于**公共仓**。按大赛规则，公共仓改动不能随专属仓 PR 一起提交，需要：

1. fork 公共仓（nuttx / vendor）；
2. 在 `dev-ai-contest-2026` 基础上建分支并提交本补丁内容；
3. 向组委会仓库发 PR 并说明用途（GD32F407 芯片分发 + gd32f470v_start 板级适配）。

在公共仓 PR 合入之前，本仓的构建依赖上面这两处本地改动。
