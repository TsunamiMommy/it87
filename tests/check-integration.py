#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path

root = Path(__file__).resolve().parents[1]
core = (root / 'it87-core.c').read_text()
make = (root / 'Makefile').read_text()

for token in (
    '#include "gigabyte/smartfan/smartfan.h"',
    'gigabyte_smartfan_channel_count(',
    'gigabyte_smartfan_vector_base(',
    'gigabyte_smartfan_tach_regs(',
    'gigabyte_smartfan_save_vector(',
    'gigabyte_smartfan_set_manual(',
    'gigabyte_smartfan_restore_vector(',
    'gigabyte_smartfan_claim(',
    'gigabyte_smartfan_release(',
    'gigabyte_smartfan_extvec_disable(',
    'gigabyte_smartfan_detect_g23(',
):
    assert token in core, f'missing integration: {token}'

manual = core.split('static void it87_h2ram_set_manual(', 1)[1]
manual = manual.split('static void it87_h2ram_restore_vector(', 1)[0]

for token in ('it87_h2ram_save_vector(data, nr);',
              'it87_h2ram_set_smartfan(data, true);',
              'gigabyte_smartfan_set_manual('):
    assert token in manual

assert manual.index('it87_h2ram_save_vector(data, nr);') < \
       manual.index('it87_h2ram_set_smartfan(data, true);') < \
       manual.index('gigabyte_smartfan_set_manual(')

for obj in ('smartfan-it86xx.o', 'smartfan-h2ram.o', 'smartfan-h2ram-g2.o',
            'smartfan-h2ram-g3.o', 'smartfan-it57xx.o'):
    assert obj in make, f'missing build object: {obj}'

print('Smart Fan integration checks passed')
