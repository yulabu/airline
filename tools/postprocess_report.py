#!/usr/bin/env python3
r"""报告 docx 后处理：页码域修补 + 目录去重。
用法: python3 postprocess_report.py <docx路径>
步骤:
  1. footer 的 PAGE 域补 \* arabic \* MERGEFORMAT（WPS 兼容）
  2. 删除封面节残留的空 <w:pgNumType/>
  3. 目录去重：占位脚本会在域内多注入一条超链接条目、且首条标题
     出现两个静态段——删除域内超链接条目与第一个重复静态段，
     保留 43 条带页码的静态条目；Word 打开更新域后由真实标题重建。
"""
import re
import shutil
import sys
import zipfile

path = sys.argv[1]
tmp = path + ".tmp"

with zipfile.ZipFile(path) as zin:
    items = {n: zin.read(n) for n in zin.namelist()}

# 1. footer PAGE 域格式
footers = []
for n in list(items):
    if re.match(r"word/footer\d+\.xml", n):
        xml = items[n].decode("utf8")
        new = re.sub(r"(<w:instrText[^>]*>)\s*PAGE\s*(</w:instrText>)",
                     r"\1 PAGE \\* arabic \\* MERGEFORMAT \2", xml)
        if new != xml:
            items[n] = new.encode("utf8")
            footers.append(n)

doc = items["word/document.xml"].decode("utf8")

# 2. 空 pgNumType
n_pg = doc.count("<w:pgNumType/>")
doc = doc.replace("<w:pgNumType/>", "")

# 3. 目录去重：占位脚本会把第一条条目以超链接形式注入目录域段落内
#    （无页码），其后 43 条静态条目完整且带页码。删除该超链接条目，
#    保留静态条目；Word 打开更新域后由真实标题重建。
n_link = 0
m = re.search(r'<w:hyperlink w:history="1" w:anchor="_Toc\d+">.*?</w:hyperlink>', doc, flags=re.S)
if m:
    doc = doc[:m.start()] + doc[m.end():]
    n_link = 1

items["word/document.xml"] = doc.encode("utf8")

with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as zout:
    for n, d in items.items():
        zout.writestr(n, d)
shutil.move(tmp, path)
print(f"footer={footers} pgNumType删除={n_pg} 目录超链接条目删除={n_link}")
