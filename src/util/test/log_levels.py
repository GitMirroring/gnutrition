#!/usr/bin/env python3
from __future__ import absolute_import
from __future__ import print_function
import sys
from os import path
import logging
from utility import stdout, stderr, test_fileobj
from log import LOG as log, str2loglevel, loglevel2str, toLogLevel,\
    initLogger, setLogLevel
from exception import AppException

(pgm, ext) = path.splitext(path.basename(sys.argv[0]))

# Test conversions to Python logger levels first
user_levels = ('off', 'debug', 'info', 'warning', 'error', 'critical', 'nosuch')
log_levels = (logging.NOTSET, logging.DEBUG, logging.INFO,
              logging.WARNING, logging.ERROR, logging.CRITICAL)

stdout("initializing logging, level debug, console on, log file off.\n")
initLogger(logFile=pgm, logLevel=1, logConsole=True, logDisk=False)
debug = log.debug
info = log.info
warn = log.warn
error = log.error
critical = log.critical

errors = 0
tests = 0

for n in range(1,6):
    if n == 1:
        debug('log.debug() called with this string')
    elif n == 2:
        info('log.info() called with this string')
    elif n == 3:
        warn('log.warn() called with this string')
    elif n == 4:
        error('log.error() called with this string')
    elif n == 5:
        critical('log.critical() called with this string')

for n in range(6):
    # convert string log levels to logger values
    pylevel = str2loglevel(user_levels[n])
    if pylevel != log_levels[n]:
        stderr("str2loglevel({0:s}) ({1:d}) failed, got {2:d} ({3:s})\n".format(
            user_levels[n], n, pylevel, user_levels[pylevel/10]))
        errors += 1
    tests += 1

    # convert logger levers to strings
    strlevel = loglevel2str(log_levels[n])
    if strlevel.lower() != user_levels[n]:
        stderr("loglevel2str({0:d}) failed, got {1:s}\n".format(
            log_levels[n], strlevel))
        errors += 1
    tests += 1

    # convert strings to logger levels 
    pylevel = toLogLevel(user_levels[n])
    if pylevel != log_levels[n]:
        stderr("toLogLevel({0:s}) ({1:d}) failed, got {2:d} ({3:s})\n".format(
            user_levels[n], n, pylevel, user_levels[pylevel/10])) 
        errors += 1
    tests += 1

    # convert 0..5 to logger levels
    pylevel = toLogLevel(n)
    if pylevel != log_levels[n]:
        stderr("toLogLevel({0:s}) ({1:d}) failed, got {2:d} ({3:s})\n".format(
            user_levels[n], n, pylevel, user_levels[pylevel/10])) 
        errors += 1
    tests += 1

stdout("tests converting back and forth between strings and integer log levels:\n")
stdout("{0:d} tests, {1:d} failures\n\n".format(tests, errors))

stdout("should see debug message test 1\n")
log.debug("log test 1")
if test_fileobj('{0:s}'.format(pgm)) != 1:
    stdout('log file \"{0:s}\" should not have been created\n'.format(pgm), pgm)
    errors += 1
else:
    stdout('Should see "Parameter not a file or file object."\n')
tests += 1
    
print()

stdout("testing setLogLevel('warn'), should see only WARN: log test 2\n")
setLogLevel("warn")
log.debug("log test 2")
log.warn("log test 2")
print()

stdout("shutting down logger, reinitializing with log level 0\n")
stdout("NullHander should prevent logger calls from generating warnings/errors.\n")
logging.shutdown()

initLogger(logLevel=0, logConsole=True, logDisk=False)
log.debug("this is an error.")
stdout('you should not see "this is an error." above\n')

stdout('after installing null handler, setting log level to debug\n')
setLogLevel('debug')
log.debug("this is not an error.")

stdout('testing logging AppException instance with appended strings\n')
excp = AppException('creating AppException instance. This string will appended to.')
excp += 'this line was added to the exception with "excp += " syntax'
log.debug(excp)

logging.shutdown()
stdout('\nTotal of {0:d} tests, {1:d} errors\n'.format(tests, errors))
if errors == 0:
    stdout('Passed.\n')
else:
    stdout('Failed.\n')
