# Load the value we want to add to the input into a register
ldi r1, 0x1234

# Configure the VPM for writing
ldi vw_setup, 0xa00

# Add the input value (first uniform - rb32) and the register with the hard-coded
# constant into the VPM.
#add.setf vpm, ra1, unif
#add vpm, r1, unif
add r2, r1, unif
shr r2, r2, 8
mov vpm, r2

## move 16 words (1 vector) back to the host (DMA)
ldi vw_setup, 0x88010000

## initiate the DMA (the next uniform - ra32 - is the host address to write to))
or.setf vw_addr, unif, 0

# Wait for the DMA to complete
or.setf -, vw_wait, nop

# trigger a host interrupt (writing rb38) to stop the program
mov.setf irq, nop;  read rb0

nop;  thrend
nop
nop
