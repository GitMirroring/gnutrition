#!/usr/bin/env python3
from __future__ import absolute_import
from __future__ import print_function
from exception import AppException, format_buffer, string_truncate
from utility import stdout, stderr

class AppInitError(AppException): pass

def show_result(r):
    stdout('{0!r}\n---------------\n'.format(r))
    n = 0
    for l in r:
        n += 1
        stdout("line {0:02d}({1:02d}): {2:s}\n".format(n,len(l),l))

e = AppInitError('Testing appending text to AppException class instance.')
explain = """This class allows for appending strings via '+' or '+='. This
        tests the formatting when appending a multi-line string to the
             class instance."""
e += explain

stdout('string_truncate(80)\n')
show_result(string_truncate(explain, 80))

print('----------------------------------------------------------------')

stdout('string_truncate(60)\n')
show_result(string_truncate(explain, 60))

print('-------------stdout(Exception) ----------------------')
stdout(e)

print('-------------raise Exception ----------------------')
raise e
