import subprocess
import sys

filename = sys.argv[1]
rest = sys.argv[2:]

subprocess.run(
    "cargo build --release -q", shell=True, check=True, capture_output=True)

subprocess.run(
    f".\\target\\release\\gale.exe -i {filename}.gale -o {filename}.gbc",
    shell=True, text=True)

subprocess.run(
    f".\\target\\release\\galevm.exe --input {filename}.gbc " + " ".join(rest),
    shell=True, text=True)
