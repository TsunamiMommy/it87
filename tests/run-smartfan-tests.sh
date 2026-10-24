#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

cd "$(dirname "$0")/.."

cc=${CC:-cc}
exe=$(mktemp)
trap 'rm -f "$exe"' EXIT HUP INT TERM

"$cc" -std=gnu11 -Wall -Wextra -Werror -pedantic \
	-Itests/include -I. \
	tests/smartfan-vectors.c \
	gigabyte/smartfan/smartfan-h2ram.c \
	gigabyte/smartfan/smartfan-h2ram-g2.c \
	gigabyte/smartfan/smartfan-h2ram-g3.c \
	gigabyte/smartfan/smartfan-it57xx.c \
	-o "$exe"

"$exe"

"$cc" -std=gnu11 -Wall -Wextra -Werror -pedantic \
	-Itests/include -I. \
	tests/smartfan-ownership.c \
	gigabyte/smartfan/smartfan-h2ram.c \
	gigabyte/smartfan/smartfan-h2ram-g2.c \
	gigabyte/smartfan/smartfan-h2ram-g3.c \
	gigabyte/smartfan/smartfan-it57xx.c \
	gigabyte/smartfan/smartfan-it86xx.c \
	-o "$exe"

"$exe"
