# 解除「从网络下载」的锁定标记（Mark-of-the-Web）
# 用法：在本文件所在目录（tools）右键 →「使用 PowerShell 运行」，或在仓库根目录执行：
#   powershell -ExecutionPolicy Bypass -File tools\unblock.ps1
# 作用：去掉 zip / exe 上的 Zone.Identifier，双击时就不会再弹 SmartScreen 的「已保护你的电脑」。
# 注意：这不是杀毒软件白名单，只是让 Windows 不再把它当成「网络下载的文件」。

$root = Split-Path -Parent $PSScriptRoot
Write-Host "正在解除锁定：" $root
Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue | Unblock-File -ErrorAction SilentlyContinue
Write-Host "完成。现在可以直接双击 LuoguScorer.exe（如果杀毒软件仍然拦截，请把它加入信任，或自己编译）。"
