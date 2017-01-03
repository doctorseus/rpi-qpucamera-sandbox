.include "../qpu/vc4.qinc"

.func gvpm_wr_setup(stride, addr)
    # Ignored, Horizontal, Laned (ignored), 32-bit
    gvpm_setup(0, stride, 1, 0, 2, addr)
.endf

.func gvpm_rd_setup(num, stride, addr)
    # Horizontal, Laned (ignored), 32-bit
    gvpm_setup(num, stride, 1, 0, 2, addr)
.endf

.func gvpm_setup(num, stride, horiz, laned, size, addr) 
    0x00000000 | (num & 0xf) << 20 |(stride & 0x3f) << 12 | (horiz & 0x1) << 11 | (laned & 0x1) << 10 | (size & 0x3) << 8 | (addr & 0xff)
    # Table 32: VPM Generic Block Write Setup Format
.endf

.func dma_wr_setup(units, depth, laned, horiz, vpmbase, modew)
    0x80000000 | (units & 0x7f) << 23 | (depth & 0x7f) << 16 | (laned & 0x1) << 15 | (horiz & 0x1) << 14 | (vpmbase & 0x7ff) << 3 | (modew & 0x7)
    # Table 34: VCD DMA Store (VDW) Basic Setup Format
.endf

.func dma_rd_setup(modew, mpitch, rowlen, nrows, vpitch, vert, addrxy)
    0x80000000 | (modew & 0x7) << 28 | (mpitch & 0xf) << 24 | (rowlen & 0xf) << 20 | (nrows & 0xf) << 16 | (vpitch & 0xf) << 12 | (vert & 0x1) << 11 | (addrxy & 0x7ff)
    # Table 36: VCD DMA Load (VDR) Basic Setup Format
.endf

mov ra2, unif # Frame buffer size
read unif
read unif
read unif
mov ra3, unif # Frame buffer ptr
mov ra4, unif # Output buffer ptr

.macro threshold, offset
    # Read one vector
    ldi vr_setup, gvpm_rd_setup(16, 1, offset)
    nop
    nop
    nop
    
    # Write one vector back
    ldi vw_setup, gvpm_wr_setup(1, offset)
    nop
    nop
    nop
    
    .set j, ra5
    mov j, 16
    :1
    mov r3, vpm

    mov r2, 0x00000000

    ldi r0, 0xff000000
    and r1, r3, r0
    ldi r0, 0x60000000
    sub.setf -, r0, r1
    ldi r0, 0xff000000
    add.ifc r2, r2, r0

    ldi r0, 0x00ff0000
    and r1, r3, r0
    ldi r0, 0x00600000
    sub.setf -, r0, r1
    ldi r0, 0x00ff0000
    add.ifc r2, r2, r0

    ldi r0, 0x0000ff00
    and r1, r3, r0
    ldi r0, 0x00006000
    sub.setf -, r0, r1
    ldi r0, 0x0000ff00
    add.ifc r2, r2, r0

    ldi r0, 0x000000ff
    and r1, r3, r0
    ldi r0, 0x00000060
    sub.setf -, r0, r1
    ldi r0, 0x000000ff
    add.ifc r2, r2, r0
    
    mov vpm, r2

    sub.setf j, j, 1
    brr.anynz -, :1
    nop # Branch delay
    nop
    nop
.endm

.rep i, 225
    # Load whole block of memory into VPM with DMA
    ldi r0, (i * 16*4*64)
    ldi r1, 16*4*16

    ldi vr_setup, dma_rd_setup(0, 3, 16, 16, 1, 0, 0x100 * 0) # 64 times 16 words, horizontal
    add vr_addr, ra3, r0        # initiate the DMA    
    read vr_wait                # Wait for the DMA to complete

    ldi vr_setup, dma_rd_setup(0, 3, 16, 16, 1, 0, 0x100 * 1)
    add r0, r0, r1
    add vr_addr, ra3, r0
    read vr_wait

    ldi vr_setup, dma_rd_setup(0, 3, 16, 16, 1, 0, 0x100 * 2)
    add r0, r0, r1
    add vr_addr, ra3, r0
    read vr_wait
    
    ldi vr_setup, dma_rd_setup(0, 3, 16, 16, 1, 0, 0x100 * 3)
    add r0, r0, r1
    add vr_addr, ra3, r0
    read vr_wait
    
    threshold 0
    threshold 16
    threshold 32
    threshold 48
    
    
    # Move VPM back to host with DMA
    ldi vw_setup, dma_wr_setup(64, 16, 0, 1, 0, 0) # 64 times 16 words, horizontal
    mov r0, (i * 16*4*64)
    add vw_addr, ra4, r0
    read vw_wait
.endr


# trigger a host interrupt (writing rb38) to stop the program
mov.setf irq, nop;  read rb0

nop;  thrend
nop
nop
