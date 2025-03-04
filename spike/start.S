# Initialization code
.text
.global _start
_start:

# Initialize section pointers
la sp, _sstack
la gp, _sdata

# Set errno address. This is a hack and probably shouldn't work, but gp doesn't
# seem to be used for anything so it's fine for now.
sd gp,1192(gp)

# Enable FP unit
li t0, 1 << 13
csrs mstatus, t0

jal main

# Write the value 1 to tohost, telling Spike to quit with an exit code of 0.
li t0, 1
la t1, tohost
sw t0, 0(t1)

# Spin until Spike terminates the run.
1: j 1b

# Expose tohost and fromhost to Spike so we can communicate with it.
.data

.align 3
.global tohost
tohost:
.dword 0

.align 3
.global fromhost
fromhost:
.dword 0
