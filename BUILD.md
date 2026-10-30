# 编译说明

## 环境

- Windows 10 / 11 x64
- MinGW-w64（g++ 13 以上），需要 `wininet`、`comctl32`
- 不需要 Visual Studio，也不需要任何第三方库

## 一键准备工具链

仓库里带了 `tools/setup_mingw.sh`（在 Git Bash / MSYS2 里跑），会从清华源拉 MSYS2 的 MinGW-w64 包到 `tools/mingw64/`：

```bash
bash tools/setup_mingw.sh
```

如果链接时报 `cannot find -lpthread`，说明缺 winpthreads 的静态库；如果 `ld` 静默退出（返回码 5 / 127），通常是 bin 目录缺 `libintl-8.dll`。两个包分别是：

- `mingw-w64-x86_64-gettext-runtime`（提供 `libintl-8.dll`）
- `mingw-w64-x86_64-winpthreads-git`（提供 `libpthread.a`，注意 `winpthreads-stub` 里的是空壳，不能用）

## 编译

```bash
# 必须给临时目录，否则 ld 可能建不了临时文件
export PATH="$PWD/tools/mingw64/mingw64/bin:$PATH"
export TMPDIR="$PWD/tmp/ltmp" TMP="$PWD/tmp/ltmp" TEMP="$PWD/tmp/ltmp"
mkdir -p "$TMPDIR"

# 1) 先编译资源（图标 + VERSIONINFO + manifest）
windres -I. luogu_scorer.rc -O coff -o tmp/app.res

# 2) 再编译程序，把 .res 一起链进去
g++ -O2 -municode -mwindows -static src/luogu_scorer.cpp tmp/app.res \
    -o LuoguScorer.exe -lwininet -lcomctl32 \
    -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va

# 3) 去掉调试符号（体积从 1.0 MB 降到约 420 KB）
strip LuoguScorer.exe
```

`luogu_scorer.rc` 引用了根目录的 `app.ico` 和 `luogu_scorer.manifest`，所以要在仓库根目录下执行 `windres`（`-I.` 是必须的）。

产物直接放仓库根目录（和 `data\` 同级）即可运行。

> 如果链接报 `cannot open output file LuoguScorer.exe: Permission denied`，说明程序正在运行，先 `taskkill /F /IM LuoguScorer.exe`。

## 参数说明

| 参数 | 作用 |
| --- | --- |
| `-municode` | 使用 `wWinMain`（源码是 Unicode 入口） |
| `-mwindows` | 不弹控制台窗口 |
| `-static` | 静态链接 libgcc / libstdc++ / winpthread，免装运行库 |
| `-lwininet` | HTTP 请求 |
| `-lcomctl32` | 通用控件（`INITCOMMONCONTROLSEX`） |
| `tmp/app.res` | 资源：图标、版本信息、manifest |
| `--nxcompat` / `--dynamicbase` / `--high-entropy-va` | DEP / ASLR，让二进制看起来更正常，也减少杀软误报 |

## 体积与签名

静态链接 + `strip` 之后约 420 KB。

exe 没有代码签名，从网络下载后第一次运行会被 SmartScreen 拦（「Windows 已保护你的电脑」）。这不是代码能解决的问题：SmartScreen 看的是「受信任 CA 的签名 + 下载量声誉」。想消掉弹窗有两条路：

- 让使用者自己解锁：右键 exe → 属性 → 勾「解除锁定」，或在解压目录里 `Get-ChildItem -Recurse -File | Unblock-File`（仓库里的 `tools/unblock.ps1` 就是干这个的）。
- 申请免费的 OSS 代码签名：[SignPath Foundation](https://signpath.org/)（开源项目免费）或 [Azure Trusted Signing](https://learn.microsoft.com/azure/trusted-signing/)。自签名证书对 SmartScreen 无效。

已嵌入的、能让它「看起来更正常」的东西：图标、`VERSIONINFO`（属性页显示「洛谷计分器 1.0.0.0」）、manifest（`asInvoker`，不触发 UAC）、DEP/ASLR 标志，以及不含加壳 / 自解压 / 下载器行为。
