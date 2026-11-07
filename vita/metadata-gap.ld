/* vita-elf-create appends Sony module/import metadata to the RX segment.
 * Keep an explicit unmapped gap before RW: ordinary 4 KiB page alignment can
 * leave <4.4 KiB and the converter otherwise fails with overlapping segments. */
SECTIONS { . = ALIGN(0x1000) + 0x4000; }
INSERT BEFORE .init_array;
