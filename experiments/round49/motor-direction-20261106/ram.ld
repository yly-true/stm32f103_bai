ENTRY(ram_start)
MEMORY { RAM (rwx) : ORIGIN = 0x20001000, LENGTH = 16K }
SECTIONS {
  .vectors : { KEEP(*(.vectors)) } > RAM
  .text : { *(.text*) *(.rodata*) } > RAM
  .data : { *(.data*) } > RAM
  .bss (NOLOAD) : {
    . = ALIGN(4); __bss_start = .; *(.bss*) *(COMMON); . = ALIGN(4); __bss_end = .;
  } > RAM
  __stack_top = ORIGIN(RAM) + LENGTH(RAM);
  ASSERT(__bss_end < __stack_top - 1024, "insufficient stack")
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) }
}
