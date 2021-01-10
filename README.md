# Summary
This project contains a 6502 CPU emulator called fake6502 from 
http://rubbermallet.org/fake6502.c wrapped in a very simple 
Virtual Machine to aid learning to program the 6502 processor. 
This is a very hacky and quickly put together project and is
certainly not even close to production quality but you may find
it useful. 

I started with DASM to generate 6502 object code but it quickly
became apparent that this was a slow way to learn as it required
having to include the DASM assembler in the development cycle. I
therefore implemented support for simple hex files which quickly
evolved into a very basic mnemonic syntax with limited support
for labels (they have to be defined before used so are only really 
useful for jumping backwards but that stops you having to calculate
the two's complement relative jumps yourself). You can control 
some aspects of the VM's operation too from the hex files including
setting breakpoints.

To interrupt program execution, just use CTRL-C and you can then
step the processor, place watches and breakpoints. I have posted
this on GitHub as I wrote it to help me learn 6502 machine code
as I couldn't find anything else I particularly liked. Some of
the design of this tool is rather limited and there are clearly
inconsistencies but please feel free to fork and take it your
own way or suggest pull requests.

I will add documentation over time but the best way to learn the
operation of the tool is via the comments in main.c.
