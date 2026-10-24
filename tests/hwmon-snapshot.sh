#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

printf 'kernel\t%s\n' "$(uname -r)"
printf 'board_vendor\t%s\n' "$(cat /sys/class/dmi/id/board_vendor 2>/dev/null || true)"
printf 'board_name\t%s\n' "$(cat /sys/class/dmi/id/board_name 2>/dev/null || true)"

for path in /sys/class/gigabyte/*/gigabyte_siv \
	    /sys/class/gigabyte/*/gigabyte_lid; do
	[ -r "$path" ] || continue
	printf '%s\t%s\n' "$path" "$(cat "$path" 2>/dev/null || echo unavailable)"
done

for hwmon in /sys/class/hwmon/hwmon*; do
	[ -r "$hwmon/name" ] || continue
	name=$(cat "$hwmon/name")
	case "$name" in
	it86*|it87*|it879*) ;;
	*) continue ;;
	esac
	printf '%s/name\t%s\n' "$hwmon" "$name"
	for file in "$hwmon"/fan*_input "$hwmon"/temp*_input \
		    "$hwmon"/pwm* "$hwmon"/in*_input; do
		[ -r "$file" ] || continue
		[ -f "$file" ] || continue
		printf '%s\t%s\n' "$file" \
			"$(cat "$file" 2>/dev/null || echo unavailable)"
	done
done
