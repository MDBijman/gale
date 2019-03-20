# End to End Tests

This directory contains a set of e2e tests for the compiler + vm.
The tests are described in `tests.json`. Each test describes the arguments and success conditions. The arguments are passed to `compile_and_run.ps1`, a script that invokes the compiler and vm with the appropriate args. The test runner then checks the success conditions.
