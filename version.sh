#!/bin/sh
# Generate a version string from git metadata.
# Called by configure.ac via m4_esyscmd_s.
#
# Copyright (C) 2026 Free Software Foundation, Inc.
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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# 1. If a baked-in version file exists, use it (Release Tarball mode)
if [ -f .tarball-version ]; then
    cat .tarball-version
    exit 0
fi

# 2. Otherwise, assume we are in a dev environment (Git mode)
branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
if [ "$branch" != "main" ]
then
    # Try to describe from tags; fall back to dev version if no tags
    version=$(git describe --tags 2>/dev/null)
    if [ -n "$version" ]
    then
        echo "$version"
    else
        commit_count=$(git rev-list HEAD --count 2>/dev/null || echo "0")
        short_hash=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
        echo "dev-$commit_count-$short_hash"
    fi
else
    commit_count=$(git rev-list HEAD --count 2>/dev/null || echo "0")
    short_hash=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
    echo "dev-$commit_count-$short_hash"
fi
