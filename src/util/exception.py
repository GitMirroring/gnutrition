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

"""Exception class.
"""
from __future__ import absolute_import
from os import environ
import string


def _pr(s):
    from sys import stdout
    stdout.write("{0:s}\n".format(s))

def cvtstr(s, col=60):
    if not s or s == "":
        s = None
    elif not isinstance(s, basestring):
        ts = str(s)
    return s

def _combine(x):
    y = ""
    for s in x:
        y += "{0:s} ".format(s.strip())
    return y.rstrip()

# Remember to move this into utility module
def string_truncate(line, maxlen):
    """Break text string on a word boundary.
    
    Parameter 'line' is interpreted as a text string.
    Return 2-tuple of strings. The first is no more than maxlen characters. 
    The second is the remainder which may be any length including zero.
    """
    tlist = []
    line = line.strip(string.whitespace)
    if len(line): 
        tlist2 = line.splitlines() 
        #_pr('len(tlist2) is {0:d}'.format(len(tlist2)))
        #n = 1
        for line in tlist2:
            #_pr('line {0:02d}({1:d}): {2:s}'.format(n,len(line),line))
            #n += 1
            if len(line) > maxlen:
                maxsplit = 1
                tmp = line.rsplit(None, maxsplit)
                #_pr('maxsplit {0:2d}(tmp1){1!r}'.format(maxsplit,tmp))
                while len(tmp[0]) > maxlen: 
                    maxsplit += 1
                    tmp = line.rsplit(None, maxsplit)
                    #_pr('maxsplit {0:2d}(tmp2){1!r}'.format(maxsplit,tmp))
                tlist.append(tmp[0])
                #tlist.extend(string_truncate(_combine(tmp[1:]), maxlen))
                tlist.append(_combine(tmp[1:]))
            else:
                tlist.append(line)
    return (tlist[0], _combine(tlist[1:]))

def format_buffer(ebuf, col=60):
    """Format the text buffer 'ebuf'.

    Parameter 'ebuf' is as a list of strings. The text displayed will use a
    column width no more than the value stored in the environment variable
    COLUMNS if available. Otherwise a conservative width of no more than 60
    characters will be used.
    """
    if 'COLUMNS' in environ:
        txtwid = int(environ['COLUMNS']) - 2
    else:
        txtwid = col - 2
    if len(ebuf) == 0: return ""
    final = []
    while len(ebuf) > 0:
        (line, rem) = string_truncate(ebuf[0], txtwid)
        ebuf = ebuf[1:]
        final.append("{0:s}".format(line))
        while len(rem) > txtwid:
            (line, rem) = string_truncate(rem, txtwid)
            final.append("{0:s}".format(line))
        final.append("{0:s}".format(rem))
    text = final[0]
    if not text.endswith('\n'): text += '\n'
    for line in final[1:]:
        if not line.endswith('\n'): line += '\n'
        text += '> ' + line
    return text

class AppException(Exception):
    def __init__(self, e=None):
        self.ebuf = []
        if e:
            self.ebuf.append(cvtstr(e))
    def __add__(self, e): 
        e = cvtstr(e)
        if e:
            self.ebuf.append(e)
        return self
    def __radd__(self, e): 
        return self.__add__(e)
    def __iadd__(self, e): 
        return self.__add__(e)
    def __str__(self):
        return format_buffer(self.ebuf)
    def emesg(self):
        return self.__str__() 

class AppTestError(AppException): pass
class AppInternalError(AppException): pass
class AppInitError(AppException): pass
class AppOptionError(AppException): pass
class AppIOError(AppException): pass
class AppFileOpenError(AppIOError): pass
class AppFileReadError(AppIOError): pass
class AppFileWriteError(AppIOError): pass
class AppFileModeError(AppIOError): pass
class AppValidateFileError(AppException): pass
class AppKeyError(AppException): pass
class AppTypeError(AppException): pass
class AppParameterError(AppException): pass
class AppParseError(AppException): pass
class UknInternalError(AppInternalError): pass
class OpenerError(AppInitError): pass
class CfgFileError(AppParseError): pass
class CfgParseError(CfgFileError): pass
class CfgFileIOError(CfgFileError): pass
class RequestError(AppException): pass
class ParamFmtError(AppException): pass
