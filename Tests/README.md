# End to End Tests

This directory contains a set of e2e tests for the compiler + vm.
The tests are described in `config.json`. Each test describes the compiler/vm command line args and expected stdout. These are passed to a simple script that invokes the compiler and vm with the appropriate args and checks whether the stdout matches the expectations.
