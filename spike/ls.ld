OUTPUT_ARCH("riscv")

ENTRY(_start)

SECTIONS
{
    . = 0x80000000;

    .stack : ALIGN(1K)
    {
        _estack = .;
        . += 128K;
        _sstack = .;
    }

    .data : ALIGN(1K)
    {
        _sdata = .;
        . += 128K;
        _edata = .;
    }
}
