import json
import os
import subprocess
import time
import sys

def exit_with(message, code):
    print(message)
    exit(code)

if len(sys.argv) != 2:
    exit_with("Expected test file name as argument", -1)

test_name = sys.argv[1]

try:
    test_file = open(test_name)
except IOError as e:
    exit_with("Could not open test configuration, exiting", -1)

test_data = json.load(test_file)

print("Found test configuration")

# Validate and extract command
if 'command' not in test_data:
    exit_with("No command found, exiting", -1)

command = test_data['command']

if not isinstance(command, str):
    exit_with("Command is not of type string, exiting", -1) 

print(f"Found command: \"{command}\"")

# Validate tests
if 'tests' not in test_data:
    exit_with("No tests found, exiting", -1)

tests = test_data['tests']

if not isinstance(tests, list):
    exit_with("Expected a list of tests, exiting", -1)

print(f"Found {len(tests)} test case(s)")

# Validate and extract test cases
validated_test_cases = []

idx = 0
for test_case in tests:
    if not isinstance(test_case, dict):
        exit_with(f"Test case {idx}: is not an object, exiting", -1)

    if 'args' not in test_case:
        exit_with(f"Test case {idx}: does not contain \"args\", exiting", -1)
    if 'match' not in test_case:
        exit_with(f"Test case {idx}: does not contain \"match\", exiting", -1)

    args = test_case['args']    
    match = test_case['match']

    if not isinstance(args, list):
        exit_with(f"Test case {idx}: \"args\" is not of type list, exiting", -1)
    for arg in args:
        if not isinstance(arg, str):
            exit_with(f"Test case {idx}: \"args\" must consist only of strings, exiting", -1)
    if not isinstance(match, dict):
        exit_with(f"Test case {idx}: \"match\" is not an object, exiting", -1)
    if 'exitcode' in match and not isinstance(match['exitcode'], int):
        exit_with(f"Test case {idx}: \"match.exitcode\" is not a number, exiting", -1)
    if 'stdout' in match and not isinstance(match['stdout'], str):
        exit_with(f"Test case {idx}: \"match.stdout\" is not a string, exiting", -1)

    validated_test_cases.append({
        'args': args,
        'match': match,
    })

    idx += 1

print("Validated all test cases")

# Find greates length for printing
greatest_length = 0
for test_case in validated_test_cases:
    test_command_args = " ".join(test_case['args'])
    test_command = command + " " + test_command_args
    if len(test_command) > greatest_length:
        greatest_length = len(test_command)
idx_len = len(str(len(validated_test_cases)))

# Run tests

results = [None] * len(validated_test_cases)

idx = 0
for test_case in validated_test_cases:
    test_command_args = " ".join(test_case['args'])
    test_command = command + " " + test_command_args
    quoted_command = f"\"{test_command}\""
    # Add 2 because quoted command is 2 character longer
    print(f"Running test case {idx: >{idx_len}}: {quoted_command:.<{greatest_length+2}}...", end='', flush=True)

    before = time.time()
    process = subprocess.Popen(test_command, stdout = subprocess.PIPE, text = True, shell = True)

    result = { 'result': 'success' }

    # Execute the process, the default timeout is 5, maybe change/make configurable
    try:
        out, err = process.communicate(timeout = 5)
        result['time'] = "{0:.2f}".format(time.time() - before)
    except subprocess.TimeoutExpired:
        process.kill()
        result = { 'result': 'timeout' }

    matches = test_case['match']

    # Match the exit code of the process
    if 'exitcode' in matches:
        expected = matches['exitcode']
        actual = process.returncode

        if expected != actual:
            result['result'] = 'fail'
            result['exitcode'] = { 'actual': actual, 'expected': expected }
    # Match the stdout(put) of the process
    if 'stdout' in matches:
        expected = matches['stdout']
        actual = out

        if expected != actual:
            result['result'] = 'fail'
            result['stdout'] = { 'actual': actual, 'expected': expected }

    if result['result'] == 'success':
        print("\u001b[32m", result['result'], "\u001b[0m", sep='', flush=True)
    elif result['result'] == 'fail' or result['result'] == 'timeout':
        print("\u001b[31m", result['result'], "\u001b[0m", sep='', flush=True)

    # Save the command args for later inspection
    result['args'] = test_command_args

    results[idx] = result
    idx += 1

# Report results

failure_count = sum(1 for _ in filter(lambda r: r['result'] == 'fail', results))
success_count = sum(1 for _ in filter(lambda r: r['result'] == 'success', results))
timeout_count = sum(1 for _ in filter(lambda r: r['result'] == 'timeout', results))

print(f"Failure count: {failure_count}")
print(f"Success count: {success_count}")
print(f"Timeout count: {timeout_count}")

failures = list(filter(lambda r: r['result'] == 'fail', results))
if len(failures) > 0:
    print("Failures:")
    for result in failures:
        print(f"{result}")