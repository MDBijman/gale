from pathlib import Path

Path("tests/benchmarks/").mkdir(exist_ok=True)
many_vars = open("tests/benchmarks/many_vars.gale", "w")

many_vars.write("mod many_vars;\n")
many_vars.write("main: [str] -> ui64\n")

many_vars.write("main s = {\n")
many_vars.write("\tlet v0: ui64 = 1;\n")
n = 40
for i in range(1, n):
    many_vars.write(f"\tlet v{i}: ui64 = v{i - 1} + 1;\n")

many_vars.write(f"\tv{n-1}\n")

many_vars.write("};")

many_vars.close()
