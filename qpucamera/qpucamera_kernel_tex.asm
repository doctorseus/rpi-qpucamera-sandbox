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

# Create vector 0-15
.macro store_vector_seq_in, reg
    mov r0, [0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x3]
    shl r0, r0, 2
    mov r1, [0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3]
    add reg, r1, r0
.endm

# Init width and height steps
.macro store_vector_seq_in, reg
    mov r0, [0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x3]
    shl r0, r0, 2
    mov r1, [0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3]
    add reg, r1, r0
.endm

.set frame_width,   ra2
.set frame_height,  ra8
.set frame_ptr,     ra3
.set obuf_ptr,      ra4
.set obuf_off,      rb5
.set vec_seq,       rb6
.set fstep_w,       ra7
.set fstep_w16,     rb8
.set fstep_h,       ra9

# Store uniforms
mov frame_width,    unif # Frame width
mov frame_height,   unif # Frame height
mov fstep_w,        unif # 1.0f / frame_width
mov fstep_h,        unif # 1.0f / frame_height
mov frame_ptr,      unif # Frame buffer ptr
mov obuf_ptr,       unif # Output buffer ptr

# Init variables
store_vector_seq_in vec_seq
mov     obuf_off,   0x0
fmul    fstep_w16,  fstep_w, 16.0



.set j, ra20 # column counter
.set i, ra21 # row counter
.set column_off,    rb11
.set row_off,       rb12

mov row_off, 1.0
mov i, frame_height
:1
    # Setup the VPM for writing
    ldi vw_setup, gvpm_setup(0, 1, 1, 0, 0, 0) # Increase addr by 1 byte, start from 0

    mov column_off, 0.0

    shr j, frame_width, 4
    :2
        itof r0, vec_seq                    # load [0-16]
        fmul r0, fstep_w, r0                # step * [0-16]
        fadd r1, column_off, r0             # add element offset
        
        #mov r1, row_off
        #mov r0, 255.0
        #fmul r1, r1, r0
        #ftoi r1, r1
        #mov vpm, r1
        
        mov t0t, r1
        mov t0s, row_off
        mov unif_addr_rel, -2
        ldtmu0
        nop
        nop
        nop
        mov vpm, r4
        
        mov r0, fstep_w16
        fadd column_off, column_off, r0     # add vector column offset

    sub.setf j, j, 1
    brr.anynz -, :2
    nop # Branch delay
    nop
    nop

    fsub row_off, row_off, fstep_h          # sub row offset

    ldi vw_setup, dma_wr_setup(20, 16, 0, 1, 0, 0)
    add vw_addr, obuf_ptr, obuf_off    
    ldi r0, (16*4*20)
    add obuf_off, obuf_off, r0
    read vw_wait
    
sub.setf i, i, 1
brr.anynz -, :1
nop
nop
nop

# trigger a host interrupt (writing rb38) to stop the program
mov.setf irq, nop;  read rb0

nop;  thrend
nop
nop
