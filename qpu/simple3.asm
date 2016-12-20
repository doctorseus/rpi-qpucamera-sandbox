.include "vc4.qinc"

#.set vpm_setup(num, stride, dma) (num & 0xf) << 20 | (stride & 0x3f) << 12 | (dma & 0xfff)
#.set v32(y, x) 0x200 | (y & 0x30) | (x & 0xf)

mov r1, unif
mov r3, unif

# Read element i from VPM

#ldi  vr_setup, vpm_setup(16, 0, h32(0, 0))
#vdr_setup_0(0, 0, 1, dma_h32(0, 0))
# 1 000 0000 0001 0000 0001 1 00000000000
#ldi vr_setup, 0x80101800
#mov vr_addr, r3
#nop
#nop
#nop

mov vr_setup, vdr_setup_0(0, 16, 2, dma_h32(0, 0))
mov vr_addr, r3
or.setf -, vr_wait, nop

.rep i, 1

ldi  vr_setup, vpm_setup(16, 2, h32(0, 0))
nop
nop
nop
mov r2, vpm
add r2, r2, r2

# Configure the VPM for writing                      
#ldi vw_setup, 0xa00         # 00000000000000000000 1 0 10 00000000
ldi vw_setup, vpm_setup(16, 2, h32(0, 0))
mov vpm, r2
nop
nop
nop
mov vpm, r2

.endr


## move 16 words (1 vector) back to the host (DMA)
#ldi vw_setup, 0x88010000    # 10 0010000 0000001 0 0 00000000000 000
ldi vw_setup, vdw_setup_0(2, 16, dma_h32(0, 0))


## initiate the DMA (the next uniform - ra32 - is the host address to write to))
mov vw_addr, unif

# Wait for the DMA to complete
or.setf -, vw_wait, nop

# trigger a host interrupt (writing rb38) to stop the program
mov.setf irq, nop;  read rb0

nop;  thrend
nop
nop
