import subprocess
import argparse


def truncate(string, length):
    return string[:length] + "..." if len(string) > length else string


def run_one(filename, args, expected, debug, skip_check, interp):
    print("Running test " + filename +
          "(\"" + truncate(' '.join(args), 30) + "\") .. ", end="", flush=True)

    debug_flag = "-d" if debug else ""

    # Run Gale Compiler
    compile_completed = subprocess.run(
        f".\\target\\release\\gale.exe -i .\\tests\\{filename}.gale -o .\\tests\\{filename}.gbc {debug_flag}",
        shell=True, text=True, capture_output=True)

    if compile_completed.returncode != 0:
        print("fail: compiler failed")
        return

    if debug:
        print(compile_completed.stdout)
        print(compile_completed.stderr)

    # Run Gale VM
    vm_args = (" -a " + " ".join(args) if len(args) > 0 else "") + (" -v " if debug else "") + (" -j " if not interp else "")
    vm_completed = result = subprocess.run(
        f".\\target\\release\\galevm.exe -i .\\tests\\{filename}.gbc " + vm_args,
        shell=True, text=True, capture_output=True)

    if vm_completed.returncode != 0:
        print("fail: vm failed")
        return

    if debug:
        print(vm_completed.stdout)
        print(vm_completed.stderr)

    if skip_check:
        return

    if result.stdout == expected:
        print("ok")
    else:
        print("fail, got " + repr(result.stdout) +
              ", expected " + repr(expected))


def cleanup(filename):
    subprocess.run(f"rm .\\tests\\{filename}.gbc")


parser = argparse.ArgumentParser(
    description="Test runner for Gale infrastructure")
parser.add_argument('-t', '--test', help="Specify test name to run")
parser.add_argument('-d', dest="debug",
                    help="Pass debug flag to Gale compiler and vm", action='store_true')
parser.add_argument('-e', dest="exec_only",
                    help="Execute without checking results", action="store_true")
parser.add_argument('-i', dest="interp",
                    help="Run in interpreter mode", action="store_true")
args = parser.parse_args()

if args.debug and not args.exec_only:
    print("Warning: debug flag enabled without -e option")

# Build Gale Compiler
print("Compiling .. ", end="")
build_completed = subprocess.run(
    "cargo build --release -q", shell=True, capture_output=True)

if build_completed.returncode != 0:
    print("fail: cargo build failed")
    print(build_completed.stderr.decode("utf-8"))
    exit(-1)

print("ok")

tests = {
    "vars": ([], "14\n"),
    "simple_fn": ([], "4\n"),
    "rec_fn": (["5"], "15\n"),
    "fib": (["30"], "1346269\n"),
    "parse_ui64": (["11"], "11\n"),
    "let": ([], "3\n"),
    "assign": ([], "3\n"),
    "aoc\\2020_1_a": (open(".\\tests\\aoc\\2020_1.txt").read().split(sep=" "), "987339\n"),
    "aoc\\2020_1_b": (open(".\\tests\\aoc\\2020_1.txt").read().split(sep=" "), "259521570\n"),
    "loop": (["10", "9", "8"], "27\n"),
    "nested_loop": (["1", "2", "3", "4"], "112342123431234412341\n")
}

if args.test != None:
    if not args.test in tests:
        print("No such test")
    else:
        test_args, out = tests[args.test]
        run_one(args.test, test_args, out, args.debug, args.exec_only, args.interp)
        #cleanup(args.test)
else:
    for test in tests:
        test_args, out = tests[test]
        run_one(test, test_args, out, args.debug, args.exec_only, args.interp)
        cleanup(test)
