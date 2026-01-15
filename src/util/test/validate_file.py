#!/usr/bin/env python3
from __future__ import absolute_import
from __future__ import print_function
import sys
from utility import stdout, stderr, test_fileobj, fileErrors, filesize
#-----------------------------------------------------------------------------
testfile = "/tmp/testfile"
#fileErrors = ('none', 'type', 'open', 'mode', 'read', 'unkn')
    
failed = 0
ntests = 0

# Expect type error
res = test_fileobj("bogus")
sres = fileErrors[res] 
if sres != 'type':
    print('failed "bogus" test, got', sres)
    failed += 1
ntests += 1

# Expect type error
res = test_fileobj(None)
sres = fileErrors[res] 
if sres!= 'type':
    print("failed None type test, got", sres)
    failed += 1
ntests += 1

# Expect type error
res = test_fileobj([])
sres = fileErrors[res]
if sres != 'type':
    print("failed [] type test, got", sres)
    failed += 1
ntests += 1

f = open(testfile, "w")
f.write("byte")
sz = filesize(f)
if sz != 4:
    print("failed file size test(1), expected 4 got", sz)
    failed += 1
ntests += 1
    
# File is open for writing, not reading. Expect mode error.
res = test_fileobj(f)
sres = fileErrors[res]
if sres != 'mode':
    print('failed "mode" test, got', sres)
    failed += 1
ntests += 1

f.close()
sz = filesize(testfile)
if sz != 4:
    print("failed file size test(2), expected 4 got", sz)
    failed += 1
ntests += 1

f = open(testfile, "r")

sz = filesize(f)
if sz != 4:
    print("failed file size test(3), expected 4 got", sz)
    failed += 1
ntests += 1

# Expect no error
res = test_fileobj(f)
sres = fileErrors[res]
if sres != 'none':
    print('failed no error test(1), got', sres)
    failed += 1
ntests += 1

f.close()
# Expect no error when passing valid file pathname
res = test_fileobj(testfile)
sres = fileErrors[res]
if sres != 'none':
    print('failed no error test(1), got', sres)
    failed += 1
ntests += 1

f = open(testfile, "w")
# Test zero length file with closed opject
f.truncate()
f.close()
res = test_fileobj(f)
sres = fileErrors[res]
if sres != 'read':
    print('failed closed file object  test (zero length file): got', sres)
    failed += 1
ntests += 1


f = open(testfile, "r")
res = test_fileobj(f)
sres = fileErrors[res]
if sres != 'read':
    print('failed zero-length test: got', sres)
    failed += 1
ntests += 1

f.close()
stdout("failed {0:d} of {1:d} tests\n".format(failed, ntests))
exit(failed)
