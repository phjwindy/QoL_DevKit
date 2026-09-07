#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
embed_hash.py — DLL 篡改自校验：编译后哈希嵌入工具

原理：
  1. MOD 源码中链接 selfverify.cpp，内含 g_selfHash 签名段（magic + 32 字节零占位）
  2. MSVC 编译产出 DLL 后，运行本脚本
  3. 脚本定位 DLL 中的 magic bytes（Q O L 0xDE）
  4. 将签名段（64 字节）置零后计算整个 DLL 的 SHA-256
  5. 把哈希写回签名段的 sha256 字段

  运行时 SelfVerifyInit() 重复同样的流程验证 DLL 未被篡改。

用法：
  python embed_hash.py <DLL路径>
  python embed_hash.py <DLL目录>          # 批量处理目录下所有 .dll
  python embed_hash.py <DLL路径> --verify  # 仅验证不写入

作者：PHJ&消失的清风
"""

import sys
import os
import hashlib
import struct
from pathlib import Path

MAGIC = b'QOL\xde'
SIG_SIZE = 64  # magic(4) + sha256(32) + reserved(28) = 64 bytes


def find_signature(data: bytes) -> int:
    """在 DLL 二进制中搜索 magic bytes，返回偏移。"""
    offset = data.find(MAGIC)
    if offset == -1:
        return -1
    # 确保后面有足够空间
    if offset + SIG_SIZE > len(data):
        return -1
    return offset


def compute_dll_hash(data: bytes, sig_offset: int) -> bytes:
    """将签名段置零后计算完整 DLL 的 SHA-256。"""
    masked = bytearray(data)
    # 整个 64 字节签名段（含 magic）置零
    for i in range(SIG_SIZE):
        masked[sig_offset + i] = 0
    return hashlib.sha256(bytes(masked)).digest()


def embed_hash(dll_path: str, verify_only: bool = False) -> bool:
    """对单个 DLL 嵌入或验证哈希。"""
    path = Path(dll_path)
    if not path.exists():
        print(f"  [ERROR] 文件不存在: {dll_path}")
        return False

    data = path.read_bytes()
    sig_offset = find_signature(data)
    if sig_offset == -1:
        print(f"  [SKIP]  未找到签名段 (magic not found): {path.name}")
        return False

    # 计算哈希（签名段置零后）
    file_hash = compute_dll_hash(data, sig_offset)
    hash_hex = file_hash.hex().upper()

    if verify_only:
        # 验证模式：读出已嵌入的哈希，与重新计算的比对
        embedded = data[sig_offset + 4: sig_offset + 36]
        embedded_hex = embedded.hex().upper()
        if embedded == b'\x00' * 32:
            print(f"  [DEV]   未嵌入哈希（开发版）: {path.name}")
            return True
        if embedded == file_hash:
            print(f"  [OK]    校验通过: {path.name}")
            print(f"          SHA-256: {hash_hex}")
            return True
        else:
            print(f"  [FAIL]  校验失败! DLL 已被篡改: {path.name}")
            print(f"          嵌入值: {embedded_hex}")
            print(f"          实际值: {hash_hex}")
            return False

    # 嵌入模式：写入哈希
    masked = bytearray(data)
    # 先确保签名段全零（清除上次嵌入的哈希）
    for i in range(SIG_SIZE):
        masked[sig_offset + i] = 0
    # 重新计算（因为此时签名段已全零，与最终运行时一致）
    file_hash = hashlib.sha256(bytes(masked)).digest()

    # 写入：magic + sha256 + zeros(reserved)
    result = bytearray(data)
    result[sig_offset: sig_offset + 4] = MAGIC
    result[sig_offset + 4: sig_offset + 36] = file_hash
    # reserved 区域保持零

    path.write_bytes(bytes(result))
    print(f"  [DONE]  哈希已嵌入: {path.name}")
    print(f"          SHA-256: {file_hash.hex().upper()}")
    return True


def main():
    if len(sys.argv) < 2:
        print("用法:")
        print("  python embed_hash.py <DLL路径>           # 嵌入哈希")
        print("  python embed_hash.py <DLL目录>           # 批量嵌入目录下所有 .dll")
        print("  python embed_hash.py <DLL路径> --verify   # 仅验证")
        sys.exit(1)

    target = sys.argv[1]
    verify_only = '--verify' in sys.argv

    print(f"{'=== 验证模式 ===' if verify_only else '=== 哈希嵌入 ==='}")
    print()

    if os.path.isdir(target):
        # 批量处理目录下所有 DLL
        dlls = sorted(Path(target).glob('*.dll'))
        if not dlls:
            print(f"  目录下无 .dll 文件: {target}")
            sys.exit(0)
        print(f"  发现 {len(dlls)} 个 DLL 文件:")
        print()
        success = 0
        for dll in dlls:
            if embed_hash(str(dll), verify_only):
                success += 1
        print()
        print(f"  完成: {success}/{len(dlls)} 成功")
    else:
        embed_hash(target, verify_only)


if __name__ == '__main__':
    main()
