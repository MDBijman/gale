import subprocess

def run_one(filename, args, expected):
    # Build Gale Compiler
    subprocess.run("cargo build -q", shell=True, check=True, capture_output=True)

    # Run Gale Compiler
    subprocess.run(f".\\target\\release\\gale.exe -i .\\tests\\{filename}.gale -o .\\tests\\{filename}.gbc", shell=True, check=True, capture_output=True)

    # Run Gale VM
    print("Running " + filename + "(" + ''.join(map(str, args)) + ") .. ", end="")

    vm_args = " -a " + " ".join(args) if len(args) > 0 else ""
    result = subprocess.run(f".\\target\\release\\galevm.exe -i .\\tests\\{filename}.gbc -j" + vm_args, shell=True, check=True, text=True, capture_output=True)

    if result.stdout == expected:
        print("ok")
    else:
        print("fail, got " + repr(result.stdout) + ", expected " + repr(expected))
    
    subprocess.run(f"rm .\\tests\\{filename}.gbc")

run_one("simple", ["30"], "1346269\n")
run_one("id", ["11"], "11\n")

