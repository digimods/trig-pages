| SPDX-License-Identifier: GPL-2.0-or-later
| trig-pages: the stock fonts a generator draws with, OS 1.53 (each the firmware's font record: its width,
| rows, then the widths, offsets and bitmaps tables).
|   tp_font_label   0x401fe09c  the parameter pages' knob labels and values: 5 rows, capitals
|   tp_font_pop     0x401fd1cc  the header's pop-up while a knob turns: 6 rows, any case
|   tp_font_big     0x401f9160  9 rows: the track box's digit

        .globl  tp_font_label
        .set    tp_font_label, 0x401fe09c
        .globl  tp_font_pop
        .set    tp_font_pop, 0x401fd1cc
        .globl  tp_font_big
        .set    tp_font_big, 0x401f9160
