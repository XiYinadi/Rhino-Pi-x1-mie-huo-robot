#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# tools/comment_coverage.py
# 作用：统计 Python 源码的注释覆盖率。
# 说明：由 tools 目录维护。
# 文档：docs/代码结构说明.md

"""统计 Python 源码的注释覆盖率。

口径（与《开发者案例征集规范》一致）：
  覆盖率 =（注释行 + 文档字符串行）/ 非空行
  - 注释行：tokenize 出的 COMMENT（`# ...`）
  - 文档字符串行：模块/类/函数首条三引号字符串的行数
  - 非空行：去掉空行后的总行数（含代码、注释、docstring）

用法：
    python3 tools/comment_coverage.py [根目录] [--min 30]
退出码：0 = 达到门限；1 = 未达到。
"""
import argparse
import io
import os
import sys
import tokenize

SKIP_DIRS = {".git", "deprecated", "__pycache__", "backups", "node_modules"}


def measure(root):
    """返回 (文件数, 非空行, 注释行, 文档字符串行, 明细列表)。"""
    total = comments = doclines = files = 0
    detail = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in sorted(filenames):
            if not name.endswith(".py"):
                continue
            path = os.path.join(dirpath, name)
            try:
                src = io.open(path, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            nonblank = sum(1 for line in src.splitlines() if line.strip())
            if nonblank == 0:
                continue
            c = d = 0
            try:
                for token in tokenize.generate_tokens(io.StringIO(src).readline):
                    if token.type == tokenize.COMMENT:
                        c += 1
                    elif token.type == tokenize.STRING and token.string.lstrip().startswith(('"""', "'''")):
                        d += token.string.count("\n") + 1
            except (tokenize.TokenError, IndentationError):
                continue
            files += 1
            total += nonblank
            comments += c
            doclines += d
            detail.append((path, nonblank, c, d))
    return files, total, comments, doclines, detail


def main():
    """入口。
    返回：计算结果。
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default=".")
    parser.add_argument("--min", type=float, default=30.0, help="门限百分比")
    parser.add_argument("--detail", action="store_true", help="按文件打印明细")
    args = parser.parse_args()

    files, total, comments, doclines, detail = measure(args.root)
    if total == 0:
        print("没有找到 Python 文件")
        return 1
    covered = comments + doclines
    ratio = 100.0 * covered / total
    print("文件数        %d" % files)
    print("非空行        %d" % total)
    print("注释行        %d (%.1f%%)" % (comments, 100.0 * comments / total))
    print("文档字符串行  %d (%.1f%%)" % (doclines, 100.0 * doclines / total))
    print("合计覆盖      %.1f%% (门限 %.0f%%)" % (ratio, args.min))
    if args.detail:
        print("\n%-52s %6s %6s %6s %7s" % ("文件", "行数", "注释", "文档", "覆盖率"))
        for path, n, c, d, in sorted(detail, key=lambda x: x[1], reverse=True):
            print("%-52s %6d %6d %6d %6.1f%%" % (path[-52:], n, c, d, 100.0 * (c + d) / n))
    ok = ratio >= args.min
    print("\n结论：%s" % ("达到门限" if ok else "未达到门限"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
