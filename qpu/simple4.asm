.include "vc4.qinc"

.func vpm_wr_setup(stride, horiz, laned, size, addr)
    0x00000000 | (stride & 0x3f) << 12 | (horiz & 0x1) << 11 | (laned & 0x1) << 10 | (size & 0x3) << 8 | (addr & 0xff)
.endf

.func dma_wr_setup(units, depth, laned, horiz, vpmbase, modew)
    0x80000000 | (units & 0x7f) << 23 | (depth & 0x7f) << 16 | (laned & 0x1) << 15 | (horiz & 0x1) << 14 | (vpmbase & 0x7ff) << 3 | (modew & 0x7)
.endf

mov r1, unif
mov r3, unif

# initiate the DMA to copy from addr to VPM
mov vr_setup, vdr_setup_0(0, 16, 2, dma_h32(0, 0))
mov vr_addr, r3
or.setf -, vr_wait, nop

.rep i, 1

# Configure the VPM for reading
ldi  vr_setup, vpm_setup(16, 1, h32(0, 0))
nop
nop
nop
mov r2, vpm
nop
nop
nop

#add r2, r2, r2

# Configure the VPM for writing                      
#ldi vw_setup, vpm_setup(16, 1, h32(0, 0))
ldi vw_setup, vpm_wr_setup(0, 1, 0, 3, 0)
nop
nop
nop
mov vpm, 5

#ldi vw_setup, vpm_setup(16, 1, h32(1, 0))
#mov r2, 3
#mov vpm, r2
#nop
#nop
#nop

.endr


## move 16 words (1 vector) back to the host (DMA)
#ldi vw_setup, vdw_setup_0(1, 16, dma_h32(0, 0))
ldi vw_setup, dma_wr_setup(1, 2, 0, 1, 0, 0)

## initiate the DMA (the next uniform - ra32 - is the host address to write to))
mov vw_addr, unif

# Wait for the DMA to complete
or.setf -, vw_wait, nop

# trigger a host interrupt (writing rb38) to stop the program
mov.setf irq, nop;  read rb0

nop;  thrend
nop
nop
