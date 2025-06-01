# Stick all c scripts together
from os import listdir

C = filter(lambda x: x.endswith(".c"), listdir())
code = ""
for i in C:
    with open(i) as f:
        code += f"\n// {i}\n"
        code += f.read()
with open("libtl.c", "w") as f:
    f.write(code)
