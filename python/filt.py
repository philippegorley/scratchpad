#!/usr/bin/env python

# Demangles a c++ symbol coming from backtrace(3) and backtrace_symbols(3)
# Splits on + as the symbols from those functions contain their offset, which trips up c++filt

import sys
import subprocess as sp

if len(sys.argv) < 2:
    sys.exit("Usage: {} <mangled c++ names>", sys.argv[0])

for mangled in sys.argv[1:]:
    sp.Popen(["c++filt", mangled.split('+')[0]])
