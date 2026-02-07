#  Copyright(C) 2010 - 2012 Thomas Sinclair (thomas.a.sinclair@gmail.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Logging facility. Well tested. Works well. If no logging is desired by the
user a null-handler is installed.

This is intended to be used like so:

    from util.log import LOG as log

Then, for example, log.info('whatever').

This also will log Python exceptions.

"""
from __future__ import absolute_import
import logging, logging.handlers as handlers
from os import path
from .exception import AppException

class LogLevelError(AppException): pass
class FileSystemAccessError(AppException): pass

_LOG_MODULE = 'api'
_DEBUG_MODULE = 'debug'

def str2loglevel(level):
    """Get the integer log level associated with the text token 'level'.
    
    The parameter 'level'  must have three leading characters matching:
     'off', 'not', 'deb', 'inf', 'war', 'err', or 'cri'

    If a match is not found logging.NOTSET is returned.
    """
    strlevels= {
        'off': logging.NOTSET,
        'not': logging.NOTSET,     # 'notset'
        'non': logging.NOTSET,     # 'none'
        'deb': logging.DEBUG,
        'inf': logging.INFO,
        'war': logging.WARNING,
        'err': logging.ERROR,
        'cri': logging.CRITICAL}

    nlevel = strlevels['off']
    level = level.lower()
    pfx = level[0:3]
    if pfx in strlevels: nlevel = strlevels[pfx]
    return nlevel

def loglevel2str(level):
    levels = ('OFF', 'DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL', 'INVALID')
    level = toLogLevel(level)
    if level == -1: level = logging.CRITICAL + 1           
    return levels[level/10]

def toLogLevel(val):
    """Conver val to Python logger level.

    Parameter val represents a log level which may be given by:
    a) 0..5
    b) 0..50  
    c) case insensitive versions of:
        'off', 'debug', 'info', 'warning', 'error', 'critical'

    If 'val' does not make sense -1 is returned.
    """
    level = logging.NOTSET
    levelerr = False
    if isinstance(val, str):
        if val.isdigit():
            level = int(val)
        else:
            level = str2loglevel(val)
    elif isinstance(val, int):
        level = val
    else:
        levelerr = True
    if not levelerr:
        if level >= 0 and level <= 5: # allows for specifying level as 0 .. 5
            level *= 10
        # This allows for user-specified log levels
        tmplevel = level/10
        if not (tmplevel >= logging.NOTSET/10 and tmplevel <= logging.CRITICAL/10):
            # We'll not error out on this. User wanted logging but specified level
            # incorrectly.
            level = logging.WARNING
    else:
        # Error out here; no idea what was passed for logging level
        level = -1
    return level

def log_error_message(caller, levelspec):
    def StdError(caller, mesg):
        from sys import stderr
        stderr.write("{0:s}: {1:s}".format(caller, mesg))
    StdError(caller, "log level must be specified in one of the following ways:\n")
    StdError(caller, "  > numerically by levels 0..5 inclusive\n")
    StdError(caller, "  > symbolically by logging.{NOTSET,DEBUG,INFO,WARNING,ERROR,CRITICAL}\n")
    StdError(caller, "  > string token 'off','notset','debug','info','warning','error', or 'critical'\n")
    e = "Invalid logging level specification \"{0!r}\"".format(levelspec)
    raise LogLevelError(e)

def initLogger(logFile='my.log', logLevel='warn', logMaxSize=50000,
                             logRotate=5, logDisk=True, logConsole=False):
    """Initialize and return a logger object at given log level or warn by
    default.

    If logging is off (log-level 0) no log file is started.  The logging sub-
    system is initialized regardless with a null-handler so that no error
    messages are generated.

    If logging is on, a file is opened but may be empty depending on logging
    level requested and error conditions encountered.

    Default behavior will be to log any message sent with a log level of equal to or
    greater that logging.WARNING.
    """
    global _LOG_MODULE
    # Figure out what format was used to specify log level; convert to integer log level
    level = toLogLevel(logLevel)
    if level == -1:
        log_error_message('initialize_logging', logLevel)
    if level == 0:
        h = logging.NullHandler()
        logging.getLogger(_LOG_MODULE).addHandler(h)
        return  
    disk = True if level > 0 and not (logDisk or logConsole) else logDisk
    (dname, bname) = path.split(logFile)
    if dname and not path.isdir(dname): makedirs(dname)
    logger = logging.getLogger(_LOG_MODULE)
    logger.setLevel(level)
    if disk:
        # disk file log handler
        dfh = handlers.RotatingFileHandler(logFile,
                                           maxBytes=logMaxSize,
                                           backupCount=logRotate)
        dfh.setLevel(level)
        fmt = "%(asctime)s:%(funcName)s():%(levelname)s: %(message)s"
        datefmt='%Y-%m-%d %H:%M:%S'
        dfh.setFormatter(logging.Formatter(fmt,datefmt))
        logger.addHandler(dfh)
    if logConsole:
        # console logging handler
        clh = logging.StreamHandler()
        clh.setLevel(level)
        fmt = "%(funcName)s(): %(levelname)s: %(message)s"
        clh.setFormatter(logging.Formatter(fmt))
        logger.addHandler(clh)

def init_logging(logfile, **kwargs):
    """Initialize application logging facility.

    The logger is initialized regardless of settings- if logging is 'off' a
    null-handler needs to be installed so that calls to the logger will not
    generate warnings.

    The keyword args recognised are:
      'logto'
      'level'
      'maxsize'
      'rotate'
    """
    from os.path import dirname, isdir
    from .utility import fopen_mode_ok, limit_intrange
    ld, ll, lr, lms, lc = True, 'warn', 3, 50000, False
    lf = logfile
    if not isdir(dirname(lf)):
        from os import mkdir
        try:
            mkdir(dirname(lf))
        except OSError as e:
            raise FileSystemAccessError(e)
    if not fopen_mode_ok(lf, 'w'):
        emsg = 'This process does not have write permission in {0:s}'
        raise FileSystemAccessError(emsg.format(dirname(lf)))       
    for keyword in kwargs.keys():
        value = kwargs[keyword]
        if keyword == 'logto':
            if value == 'console':
                ld, lc = False, True
            elif value == 'both':
                lc = True
        elif keyword == 'level':
            ll = value
        elif keyword == 'maxsize':
            lms = limit_intrange(value, 0)
        elif keyword == 'rotate':
            lr = limit_intrange(value, 1, 10)
    lf = logfile

    initLogger(logFile=lf, logLevel=ll, logMaxSize=lms,
                                   logRotate=lr, logDisk=ld, logConsole=lc)

def setLogLevel(loglevel):
    level = toLogLevel(loglevel)
    if level == -1:
        log_error_message('set_logging_level', loglevel)
    global LOG
    LOG.setLevel(level)

LOG = logging.getLogger(_LOG_MODULE)
