# Copyright (C) 2012 Free Software Foundation, Inc.
#
# This file is part of GNUtrition.
# 
# GNUtrition is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNUtrition is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNUtrition.  If not, see <http://www.gnu.org/licenses/>.
from __future__ import absolute_import
from __future__ import print_function
from . import config
from .util.log import LOG as log
debug = log.debug
info = log.info
warn = log.warn
error = log.error
critical = log.critical

def get_latest_version(url):
    """Fetch latest version information posted at URL provided."""
    import urllib
    import re
    try:
        obj = urllib.urlopen(url)
    except IOError as e:
        error("{0!r}".format(e))
        return "0.0" # Force update bypass
    reex = r"""
            "version"[\s+]?:   #   version":
            [\s+]?"            #  leading junk
            (?P<VER>([0-9]+[.]?)+)  # target match
            ["][\s+]?,(.+)?$   #  trailing junk until end of line
            .*"sr"[\s+]?:      #  "sr":
            [\s+]?"            #  Allow for white space
            (?P<SR>[2-9][0-9]) # target match  (database version)
            ["][\s+]?,(.+)?$   #  trailing junk until end of line
            .*"date"[\s+]?:    #  "date":
            [\s+]?"            #  Allow for white space
            (?P<DATE>[a-zA-Z]+[\s+]?20[12][0-9]) # target match  (sr release date)
            ["][\s+]?,(.+)?$   #  trailing junk until end of line
            .*"sr_url"[\s+]?:  #  "sr_url":
            [\s+]?"            #  Allow for white space
            (?P<SR_URL>http:..*) #  target match   (where we get database from)
            ["][\s+]?,(.+)?$   #  trailing junk until end of line
            .*"message"[\s+]?:    #  "message":
            [\s+]?"            #  eat white space
            (?P<message>.*\.)["]  # target match
            """
    reobj = re.compile(reex, re.X|re.M|re.S)
    m = re.search(reobj, obj.read())
    if m:
        return (m.group('VER'), m.group('SR'), m.group('DATE'),
                m.group('SR_URL'), m.group('message'))
    return ('0.0', None, None, None, None)

def cmp_version_strings(this_ver, curr_ver):
    s1 = this_ver.split('.')
    len_s1 = len(s1)
    s2 = curr_ver.split('.')
    len_s2 = len(s2)
    loop = 0
    while loop < len_s1 and loop < len_s2:
        s1_n, s2_n  = int(s1[loop]), int(s2[loop])
        if s1_n < s2_n:
            return True 
        elif s1_n > s2_n:
            return False  # Web version file needs updating
        loop += 1
    # Matched up to this point. Now need to check the case where one version
    # string is longer than the other
    if len_s2 > len_s1:
        return True
    return False

def update_version(version, message=None):
    """Present user with dialog notification of available newer version."""
    from . import gnutr
    msg = 'Version {0:s} of gnutrition is available.\n'.format(version)
    msg += 'Please visit http://www.gnu.org/software/gnutrition to download.\n'
    if message:
        msg += '\n{0:s}'.format(message)
    gnutr.Dialog('notify', msg)

def unpack(archive):
    # HERE: unfinished
    from zipfile import is_zipfile, ZipFile
    if not is_zipfile(archive):
        return False
    ZipFile.extractall(archive)

def get_database_archive(url):
    # HERE: Unfinished. Dropped working on automatic database update since it is
    #       updated once per year and updating would present permission errors.
    from . import gnutr_consts
    from urllib2 import Request, urlopen, URLError, HTTPError
    from os.path import basename, join
    success = 0
    tmp_file = join('/tmp/',basename(url))
    req = Request(url)
    try:
        f = urlopen(req)
        # HERE: put the file in tmp?
        local_file = open(tmp_file, "wb")
        local_file.write(f.read())
        local_file.close()
    except HTTPError as e:
        error("HTTP Error: {0!r} {1:s}".format(e.code, url))
        success = 1
    except URLError as e:
        error("URL Error: {0!r} {1:s}".format(e.reason, url))
        success = 1
    return success

def check_version():
    from . import gnutr_consts
    import install
    this_ver = install.gnutr_version()
    if config.get_value('check_disabled') or not config.get_value('check_version'):
        return 0
    import time
    interval = config.get_value('check_interval')
    last_check = config.get_value('last_check')
    time_now = time.time()
    if (time_now - last_check > interval):
        (curr_ver, sr, date, sr_url, mesg) = get_latest_version(gnutr_consts.LATEST_VERSION) 
        config.set_key_value('sr', sr)
        config.set_key_value('sr_date', date)
        config.set_key_value('sr_url', sr_url)
        update = False
        if this_ver == curr_ver:
            pass  # Nothing to do
        else:
            update = cmp_version_strings(this_ver,curr_ver)
        if update:
            update_version(curr_ver, mesg)
    last_check = config.set_key_value('last_check',time_now)
    return 1

if __name__ == '__main__':
    def str_cmp_test():
        print('True ==', cmp_ver_strings("0.1", "0.2"))
        print('True ==', cmp_ver_strings("0.1.1", "0.2"))
        print('False ==', cmp_ver_strings("0.2.1", "0.2"))
        print('False ==', cmp_ver_strings("0.2", "0.1"))
        print('True ==', cmp_ver_strings("0.1", "0.2.1"))
        print('True ==', cmp_ver_strings("0.1.1", "0.1.2"))
        print('False ==', cmp_ver_strings("0.2.1", "0.2"))
    from . import gnutr_consts
    url = gnutr_consts.LATEST_VERSION
    (ver,sr,rel,sr_url,msg) = get_latest_version(url)
    print('latest version available:', ver)
    print('SR:', sr)
    print('release date:', rel)
    print('SR URL:', sr_url)
    print('version message:', msg)

