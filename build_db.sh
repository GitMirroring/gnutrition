#!/bin/bash

# build_db.sh - Download USDA food data and build a SQLite database
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

set -e

dir="$HOME/.local/share/gnutrition"
mkdir -p "$dir"
cd "$dir"

# Check for required commands
missing=()
for cmd in wget sha512sum libreoffice sqlite3; do
    if ! command -v "$cmd" > /dev/null 2>&1; then
        missing+=("$cmd")
    fi
done

if [ ${#missing[@]} -ne 0 ]; then
    echo "Error: The following required commands are not available:" >&2
    for cmd in "${missing[@]}"; do
        echo "  - $cmd (please install it)" >&2
    done
    exit 1
fi

# Download the Excel files
BASE_URL="https://www.ars.usda.gov/ARSUserFiles/80400530/apps"

wget -O "2019-2020 FNDDS At A Glance - Foods and Beverages.xlsx" \
    "${BASE_URL}/2019-2020%20FNDDS%20At%20A%20Glance%20-%20Foods%20and%20Beverages.xlsx"

wget -O "2019-2020 FNDDS At A Glance - Portions and Weights.xlsx" \
    "${BASE_URL}/2019-2020%20FNDDS%20At%20A%20Glance%20-%20Portions%20and%20Weights.xlsx"

wget -O "2019-2020 FNDDS At A Glance - FNDDS Nutrient Values.xlsx" \
    "${BASE_URL}/2019-2020%20FNDDS%20At%20A%20Glance%20-%20FNDDS%20Nutrient%20Values.xlsx"

wget -O "FPED_1720.xls" \
    "${BASE_URL}/FPED_1720.xls"

# Verify SHA-512 hashes
echo "Verifying file integrity..."

if ! sha512sum -c <<'EOF'
a2d99aa0e4761e5fed5dd9e5b2003ece4c8f61c954ddabbdacef816abb15c2647d2f1b1cfef8bed98af5a20d4af6d2a34c3e80fd271e33bdf728797f72c18e0c  2019-2020 FNDDS At A Glance - Foods and Beverages.xlsx
7da5e96923f92bf94d0420d899b86004fd86c69a5a7bb2273b02ee21147e827b63af37b8e80525c91b680dd6ed035cfad677cf27dd80f1ae8c10ed0e8e49a0f8  2019-2020 FNDDS At A Glance - Portions and Weights.xlsx
43ee5a5661499b41e621622a4fdffd071805f86839e0577654316a8f21952ba69d56594e0437b77aac32391d90df053a2c036fd7067e2e6b2cad5c5e0576edc1  2019-2020 FNDDS At A Glance - FNDDS Nutrient Values.xlsx
38b46ba1972d700de4d1625acfd0260c518b64850fa0c8251dc7e960ad09f008915ed4fa935074b19fc83cc09abee756dc7dd62a4c487b2a2ec7b4bf196bd683  FPED_1720.xls
EOF
then
    echo "Error: SHA-512 hash verification failed. Downloaded files may be corrupted." >&2
    exit 1
fi

echo "All hashes verified successfully."

# Convert Excel files to CSV using LibreOffice
echo "Converting Excel files to CSV..."

for file in *.xls *.xlsx; do
    libreoffice --headless --convert-to csv "$file"
done

# The USDA "At A Glance" Excel files contain 1-2 rows of report title
# and date information before the actual column headers (e.g., Food code,
# Main food description). These extra rows must be removed so that the
# first row of each CSV contains the column names; otherwise SQLite will
# import the title text as a data row or fail on type mismatches.
for csv in "2019-2020 FNDDS At A Glance - Foods and Beverages.csv" \
           "2019-2020 FNDDS At A Glance - Portions and Weights.csv" \
           "2019-2020 FNDDS At A Glance - FNDDS Nutrient Values.csv"; do
    sed -i '1,2d' "$csv"
done

echo "Conversion complete."

# Build the SQLite database
echo "Building SQLite database..."

rm -f food.db

sqlite3 food.db <<'SQL'
.mode csv

-- Import foods table
.import '2019-2020 FNDDS At A Glance - Foods and Beverages.csv' foods

-- Import portions table
.import '2019-2020 FNDDS At A Glance - Portions and Weights.csv' portions

-- Import nutrients table
.import '2019-2020 FNDDS At A Glance - FNDDS Nutrient Values.csv' nutrients

-- Import points table
.import 'FPED_1720.csv' points
SQL

echo "SQLite database food.db created successfully."
