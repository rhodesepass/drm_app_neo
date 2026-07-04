import os
import json

with open("common_char.txt","r",encoding="utf-8") as f:
    commoncharstr = f.read()
    # split by character

with open("character_table.json","r",encoding="utf-8") as f:
    character_table = json.loads(f.read())
    for k,char in character_table["Characters"].items():
        if not k.startswith("char_"):
            continue
        opname = char["Appellation"]
        opnamezh = char["Name"]
        commoncharstr += opname + opnamezh


commonchar = [x for x in commoncharstr if x != "\n"]
commonchar = list(set(commonchar))
print(len(commonchar))
with open("result.txt","w",encoding="utf-8") as f:
    f.write("".join(commonchar))