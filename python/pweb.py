#!/usr/bin/env python3

import requests
import sys

for page in sys.argv[1:]:
    r = requests.get(page)
    print(r.text)

