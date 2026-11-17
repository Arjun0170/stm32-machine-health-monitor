ENTRY(Reset_Handler)
MEMORY { FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1M
         RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K }
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
 .vectors : { KEEP(*(.vectors)) } > FLASH
 .text : { *(.text*) *(.rodata*) } > FLASH
 _sidata = LOADADDR(.data);
 .data : { _sdata = .; *(.data*) _edata = .; } > RAM AT > FLASH
 .bss (NOLOAD) : { _sbss = .; *(.bss*) *(COMMON) _ebss = .; } > RAM
 /DISCARD/ : { *(.comment) *(.note*) }
}
