import json
import os
import subprocess

def exit_with(message, code):
    print(message)
    exit(code)

try:
    test_file = open("./tests.json")
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

# Run tests

results = [None] * len(validated_test_cases)

idx = 0
for test_case in validated_test_cases:
    test_command_args = " ".join(test_case['args'])
    test_command = command + " " + test_command_args
    print(f"Running test case {idx}: \"{test_command}\"")

    process = subprocess.Popen(test_command, stdout = subprocess.PIPE, text = True, shell = True)

    result = { 'result': 'success' }

    # Execute the process, the default timeout is 15, maybe change/make configurable
    try:
        out, err = process.communicate(timeout = 15)
    except TimeoutError:
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

    # Save the command args for later inspection
    result['args'] = test_command_args

    results[idx] = result

    idx += 1

# Report results

for idx, result in enumerate(results):
    print(f"{idx}: {result}")