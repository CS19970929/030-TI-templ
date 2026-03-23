ENTRY(Reset_Handler)

_estack = 0x20001B80;

MEMORY
{
    RAM_VECTOR (xrw) : ORIGIN = 0x20000000, LENGTH = 0x000000C0
    RAM        (xrw) : ORIGIN = 0x200000C0, LENGTH = 0x00001AC0
    RAM_TOP_RSVD (xrw) : ORIGIN = 0x20001B80, LENGTH = 0x00000480
    FLASH      (rx)  : ORIGIN = 0x08001C00, LENGTH = 0x0000D400
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        KEEP(*(.text.Reset_Handler))
        KEEP(*(.text.Default_Handler))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .RAMVectorTable (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.RAMVectorTable))
        . = ALIGN(4);
    } > RAM_VECTOR

    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    ._user_heap_stack (NOLOAD) :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + @APP_HEAP_SIZE@;
        . = . + @APP_STACK_SIZE@;
        . = ALIGN(8);
    } > RAM

    /DISCARD/ :
    {
        *(.ARM.extab*)
        *(.ARM.exidx*)
        *(.preinit_array*)
        *(.init_array*)
        *(.fini_array*)
        *(.note*)
        *(.comment*)
    }
}
