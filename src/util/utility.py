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

"""Utility functions.

stdout and stderr are exported.
"""
from __future__ import absolute_import
import codecs, re, sys
from os import path
from datetime import date, datetime 
from calendar import monthrange
from inspect import currentframe  # for func()
from .exception import AppInternalError, AppParameterError
class InternalWriteError(AppInternalError): pass

def _write(write, obj, caller):
    """Internal use.  Called by:
        stderr(message [,caller]) 
        stdout(message [,caller]) 
    """
    if isinstance(obj, basestring):
        text = unicode(obj, 'utf8')
    elif isinstance(obj, Exception):
        text = str(obj)
    else:
        err = "_write() error: obj is type {0!s}\n".format(type(obj))
        raise InternalWriteError(err)
    if caller:
        text = "{0:s}: {1:s}".format(caller, text)
    write(text)

class _StdErr:
    """Write 'text' to standard error (sys.stderr) with optional caller."""
    def __init__(self):
        self.stderr = codecs.getwriter('utf8')(sys.stderr)

    def __call__(self, text, caller=None):
        self.write(text, caller)

    def write(self, text, caller=None):
        _write(self.stderr.write, text, caller)

stderr = _StdErr()

class _StdOut:
    """Write 'text' to standard output (sys.stdout) with optional caller."""
    def __init__(self):
        self.stdout = codecs.getwriter('utf8')(sys.stdout)

    def __call__(self, text, caller=None):
        self.write(text, caller=None)

    def write(self, text, caller=None):
        _write(self.stdout.write, text, caller)

stdout = _StdOut()

def func():
    """Return name of caller of this function.

    The intent is to provide debugging information. A function can call
      iam = func()
    which returns the name of the enclosing function definition. The usage
    is intended for passing  'iam' as 'caller' parameter to the stdout and
    stderr functions defined in this module. 
    """
    frame = currentframe()
    return frame.f_back.f_code.co_name

def limit_intrange(value, low=-sys.maxsize-1, high=sys.maxsize):
    class RangeError(AppInternalError): pass
    if not high >= low:
        e = RangeError("high range must be >= low range")
        e += "low: {0:d}, high {1:d}".format(low, high)
        raise e
    return min(max(int(value), low), high)

def strip_csv(csvstr):
    """Remove white space from a comma-seperated string of words."""
    if not isinstance(csvstr, basestring): return csvstr
    wordlist = csvstr.split(',')
    csvstr = wordlist[0].strip()
    for word in wordlist[1:]:
        csvstr += ',' + word.strip()
    return csvstr

def join_lines(block):
    """Remove excess whitespace and newline characters embedded in 'block'.

    This will replace multiple spaces with one space and join lines.
    It is typically used before calling:
        format_lines(text [,margin=''] [,col=80])
    """
    text = ''
    try:
        while True:
            n = block.index('\n')
            text += block[:n].strip() + ' '
            block = block[n+1:]
    except ValueError:
        text += block.strip()
    return text + '\n'

def format_lines(text, margin='', col=80):
    """Convert a block of text into lines of no more than 'col' characters.

    The string of text passed is converted as follows:
      - blank lines are removed
      - a tab is replaced with a single space character
      - multiple spaces are replaced with a single space

    The string returned will have lines no longer than the parameter 'col'
    specifies. 
    """
    text = join_lines(text).expandtabs(1).replace('  ', ' ')
    block = ''
    n = 0
    try:
        while True:
            n = text.rindex(' ',0,col)
            block += margin + text[:n] + '\n'
            text = text[n:].strip()
    except ValueError:
        sep = ' ' if n + len(text) < col else '\n'
        block = block[:-1].strip() + sep + text
    return block + '\n'

def data_from_file(fname):
    """Read and return the contents of a file.

    The value 'None' is returned if the file cannot be read.
    Otherwise the contents of the file are returned.
    """
    data = None
    try:
        with open(fname,'r') as f:
            data = f.read()
    except OSError as e:
        data = None
        stderr('data_from_file: unable to read {0:s}, {1!s}\n'.format(fname,e))
    return data

from .exception import AppTypeError, AppFileOpenError,\
     AppFileModeError, AppFileReadError

# various boolean tests related to files, file objects and file open mode.
def isfilepath(obj):
    """Boolean test for file on disk. Same as path.isfile(obj)."""

    if obj is not None and isinstance(obj, basestring):
        from os import path
        return path.isfile(obj)
    return False

def isfileobj(obj):
    """Boolean test for file object. Same as isinstance(obj, file)."""
    if obj is not None:
        return isinstance(obj, file) 
    return False

def isfile(obj):
    """Boolean test for disk file or file object.

    Return True if parameter 'obj' is either:

      1) A file object as determined by isinstance(obj, file)
      2) Referrs to existing file on disk as determined by os.path.isfile(obj).
    """
    return (isfileobj(obj) or isfilepath(obj))

def fopen_mode_ok(obj, mode):
    """Boolean test for file object.

    Determine if 'obj' be opened in given 'mode'.

    Intended to be called only from functions which have verified:

    1) 'obj' is a file object or referrs to existing file.
    2) 'obj' is not already open.
    """
    try:
        with open(obj, mode) as f:
            pass
    except OSError:
        return False
    return True

def _fopen_ok(obj, mode):
    """Boolean test for file object.

    Determine if 'obj' be opened in given 'mode'. This will return True if and
    only if both these conditions are True:
    1) 'obj' is a file object or referrs to a file on disk.
    2) 'obj' can be opened or is already open in the specified mode.
    """
    if isfilepath(obj) or (isfileobj(obj) and obj.closed):
        return fopen_mode_ok(obj, mode)
    elif isfileobj(obj):
        assert not obj.closed
        return obj.mode[0] == mode[0]
    return False

def fopen_wok(obj):
    return _fopen_ok(obj, "w")

def fopen_rok(obj):
    return _fopen_ok(obj, "r")

def fopen_ok(obj):
    return (fopen_rok(obj) or fopen_wok(obj))

def isopenfile(obj):
    return (isfileobj(obj) and not obj.closed)

def f_seekable(obj):
    return isfileobj(obj) and not obj.isatty()

# End of boolean tests

def _fopen(obj, mode):
    try:
        f = open(obj, mode)
    except OSError as e:
        err = "Unable to open object in mode '{0:s}'".foramt(mode)
        err += str(e)
        raise AppFileOpenError(err)
    return f

def fopen_obj(obj, mode):
    if not isfile(obj):
        raise AppTypeError("Parameter not a file or file pathname.")
    if isopenfile(obj):
        e = "Object is already open (mode '{0:s}')".format(obj.mode)
        raise AppFileOpenError(e)
    return _fopen(obj, mode)

def _filesize(f):
    assert isfileobj(f)
    from os import SEEK_CUR, SEEK_END, SEEK_SET
    pos = f.tell()
    f.seek(0,SEEK_END)
    end = f.tell()
    f.seek(pos, SEEK_SET)    # Return state to origional
    return end

def filesize(obj):
    size = -1
    f, fclose = None, False
    if not isfile(obj): return size
    if isfilepath(obj):
        f = fopen_obj(obj, 'r')
        fclose = True
    elif isfileobj(obj):
        if f_seekable(obj):
            f = obj
        else:
            return -2
    size = _filesize(f)
    if fclose: f.close()
    return size

def validate_file(obj):
    """Determing if obj if readable and non-zero length.

    Raise exception if obj is not readable or has zero length as determined
    by examining obj.seek() and obj.tell().
    If obj is open leave current file position unchanged.
    if obj is a tty-like device do nothing.
    """
    fcloseobj = False
    if not isfile(obj):
        raise AppTypeError("Parameter not a file or file object.")
    if isfilepath(obj):
        if fopen_rok(obj):
            f = fopen_obj(obj, "r")
            fcloseobj = True
        else:
            e = "Unable to open file for reading: {0:s}"
            raise AppFileOpenError(e.format(obj))
    elif isopenfile(obj):
        if obj.mode[0] != "r":
            raise AppFileModeError('file object cannot be read.')
        if f_seekable(obj):
            f = obj
        else:
            e = "File object not seekable."
            raise AppFileReadError(e.format(obj))
    else:
        fn = obj.name
        if fopen_rok(fn):
            f = fopen_obj(fn, "r")
            fcloseobj = True

    if not filesize(f) > 0:
        raise AppFileReadError("Nothing to read.") 
    if fcloseobj: f.close()

#-----------------------------------------------------------------------------
fileErrors = ('none', 'type', 'open', 'mode', 'read', 'unkn')

def test_fileobj(f):
    """Wrapper for validate_file.

    This function traps exceptions thrown by validate_file(f) and returns a
    unique value for each of the six possible processing exceptions from that
    function.
    """
    try:
        validate_file(f)
    except AppTypeError as e:
        stderr(e)
        return 1
    except AppFileOpenError as e:
        stderr(e)
        return 2
    except AppFileModeError as e:
        stderr(e)
        return 3
    except AppFileReadError as e:
        stderr(e)
        return 4
    except Exception as e:
        stderr("Unknow exception: {0:s}\n".format(e))
        #raise
        return 5
    return 0
