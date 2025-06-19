# Stick all c scripts together
from os import listdir
from sys import argv

if len(argc) == 1:
    C = filter(lambda x: x.endswith(".c"), listdir())
    code = ""
    for i in C:
        with open(i) as f:
            code += f"\n// {i}\n"
            code += f.read()
    with open("libtl.c", "w") as f:
        f.write(code)
elif len(argc) == 2:
    from subprocess import Popen, PIPE
    print(f"Making custom executable for toylang script {repr(argc)}.")
    
