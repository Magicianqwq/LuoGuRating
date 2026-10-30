#!/usr/bin/env bash
# 拉一份 MinGW-w64 到 tools/mingw64（Git Bash 里跑）
# 版本号过期导致 404 时，去镜像目录里找同名的新版本替换即可。
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
REPO="https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/mingw64"
DEST="$HERE/tools/mingw64"
CACHE="$HERE/tmp/pkgs"
mkdir -p "$DEST" "$CACHE"
PKGS="mingw-w64-x86_64-gcc-16.2.0-6-any.pkg.tar.zst
mingw-w64-x86_64-gcc-libs-16.2.0-6-any.pkg.tar.zst
mingw-w64-x86_64-binutils-2.47-4-any.pkg.tar.zst
mingw-w64-x86_64-crt-git-13.0.0.r91.gfc0b67305-1-any.pkg.tar.zst
mingw-w64-x86_64-headers-git-13.0.0.r91.gfc0b67305-1-any.pkg.tar.zst
mingw-w64-x86_64-libwinpthread-git-12.0.0.r747.g1a99f8514-1-any.pkg.tar.zst
mingw-w64-x86_64-windows-default-manifest-6.4-4-any.pkg.tar.zst
mingw-w64-x86_64-zlib-1.3.2-3-any.pkg.tar.zst
mingw-w64-x86_64-zstd-1.5.7-2-any.pkg.tar.zst
mingw-w64-x86_64-gmp-6.3.0-2-any.pkg.tar.zst
mingw-w64-x86_64-mpfr-4.2.2-3-any.pkg.tar.zst
mingw-w64-x86_64-mpc-1.4.1-1-any.pkg.tar.zst
mingw-w64-x86_64-isl-0.28-1-any.pkg.tar.zst
mingw-w64-x86_64-libiconv-1.19-1-any.pkg.tar.zst
mingw-w64-x86_64-gettext-runtime-0.26-2-any.pkg.tar.zst"

TAR=/c/Windows/System32/tar.exe
for p in $PKGS; do
  echo "Downloading $p ..."
  curl -sSL --connect-timeout 20 --retry 3 -o "$CACHE/$p" "$REPO/$p"
  echo "Extracting $p ..."
  "$TAR" -xf "$CACHE/$p" -C "$DEST" --exclude '.PKGINFO' --exclude '.BUILDINFO' --exclude '.MTREE' 2>/dev/null \
    || "$TAR" -xf "$CACHE/$p" -C "$DEST"
done

# 说明：libintl-8.dll 来自 gettext-runtime（binutils 依赖它，缺了 ld 会静默失败）；
#      libpthread.a / libwinpthread.a 来自 libwinpthread（注意 winpthreads-stub 里的是空壳）。
echo DONE
ls "$DEST"
