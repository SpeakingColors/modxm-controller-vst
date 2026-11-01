#!/bin/sh
# Runs the pure C++ core tests (MIDI map and SysEx) against this repo.
cd "$(dirname "$0")/.." && c++ -std=c++17 -Wall -Wextra -o /tmp/core_test tests/test_core.cpp && /tmp/core_test map/params.csv && c++ -std=c++17 -Wall -Wextra -o /tmp/sysex_test tests/test_sysex.cpp && /tmp/sysex_test
