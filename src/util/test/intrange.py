#!/usr/bin/env python
from __future__ import absolute_import
from utility import limit_intrange, stdout

low = (-200, 0, 200)
high = (-200, 0, 200)
val = (-201, -1, 0, 201)

for v in val:
    for lo in low:
        for hi in high:
            try:
                res = limit_intrange(v, lo, hi)
            except ValueError as e:
                stdout(e)
                pass
            stdout('limit({0:d}, {1:d}, {2:d}) = {3:d}\n'.format(v,lo,hi,res))

