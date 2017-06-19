
XMBurner ALU Emulator
==============================================================================

:Author:    Sandor Zsuga (Jubatian)
:License:   GNU GPLv3 (version 3 of the GNU General Public License)




Overview
------------------------------------------------------------------------------


This is an 8 bit AVR compatible ALU emulator derived from the CUzeBox project
(https://github.com/Jubatian/cuzebox), designed for supporting the development
of various ALU testing applications.

The emulator covers the ALU and the SRAM and ROM of the microcontroller (based
on an ATMega644) along with some basic peripherals designed to support
interaction with the emulator itself and the console for integration in
automated testing frameworks.

The main capability of it is support to alter the internal behaviour of the
emulator itself allowing to simulate faults within the ALU.



Calling
------------------------------------------------------------------------------


The emulator accepts one parameter: the binary to run.

If a binary is provided proper, it only produces output on the standard output
channel according to the code within the binary. This can be used to generate
test results.



Output features
------------------------------------------------------------------------------


The following ports are used to generate text onto the standard output
channel:

- 0xE0: Single character output.
- 0xE1: Output a decimal number ("0" - "255").
- 0xE2: Output a hexadecimal number ("00" - "FF").
- 0xE3: Output a binary number ("00000000" - "11111111").



Port reset and locks
------------------------------------------------------------------------------


The emulator's behaviour can be modified from within the running program by
writing to certain ports. The ports below are used to control the overall
process.

- 0xE8: Guard port. A second access to this terminates the program.
- 0xE9: Port lock: Having 0xA5 here enables accessing ports beyond this one.
- 0xEA: Reset sequentially accessed ports.



Setting emulation goals
------------------------------------------------------------------------------


By default the emulator attempts to emulate one million instructions before
returning. This can be modified by setting an emulation target using the
following ports:

- 0xEB: 4 byte target instruction count to emulate.

The target instruction count port accepts low byte first. It can be used to
set the total count of instructions to emulate. Note that the instruction
counter can not be reset, so at worst case the emulator will return after
approximately 4 billion emulated instructions.



Altering internal behaviour
------------------------------------------------------------------------------


Internal behaviour of the ALU can be altered by writing to certain ports:

- 0xF0: Behaviour modifications are enabled if this is set 0x5A.
- 0xF1: Register / RAM Memory stuck bits.
- 0xF2: ROM stuck or altered bits.
- 0xF3: Instruction related flag behaviour anomalies.
- 0xF4: Instruction destination anomalies.
- 0xF5: Addition anomalies.


0xF1: Register / RAM Memory stuck bits.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Stuck set or cleared bits can be emulated by this feature. The port accepts
the following sequence:

- Byte 0: OR mask of the byte to modify.
- Byte 1: AND mask of the byte to modify.
- Byte 2: Address low byte.
- Byte 3: Address high byte.

The memory map accords with the layout of an ATMega644. CPU registers are in
the 0x0000 - 0x001F range.

Limitations: In RAM the stuck bits are fully emulated. In IO space the
peripherals and interrupts would keep operating according to the state without
the forced stuck bits. Stuck bits in CPU registers, Status register and Stack
as concerned by the ALU are fully emulated.


0xF2: ROM stuck or altered bits.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Stuck set or cleared bits can be emulated by this feature. The port accepts
the following sequence:

- Byte 0: OR mask of the byte to modify.
- Byte 1: AND mask of the byte to modify.
- Byte 2: Address low byte
- Byte 3: Address high byte.
- Byte 4: Must be provided, zero (reserved for larger than 64K address space).

Limitations: Only emulated for LPM instructions. Code execution always happens
in accordance with the original ROM contents (so this feature is mostly useful
only for testing various checksum algorithms used to verify the code space).


0xF3: Instruction logic flag behaviour anomalies.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Flags can be made stuck cleared or set for certain instruction. Upon the
execution of the affected instruction, the flags (SREG) will be modified
according to the OR and AND masks defined for it.

- Byte 0: OR mask for the flags.
- Byte 1: AND mask for the flags.
- Byte 2: Opcode to be affected.

The opcode accords with the translated instruction set, see cu_avrc.h.


0xF4: Instruction destination anomalies.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bits can be made stuck set or cleared in the destination of instructions
having one. These will be applied after the execution of the instruction.

- Byte 0: OR mask for the destination.
- Byte 1: AND mask for the destination.
- Byte 2: Opcode to be affected.

The opcode accords with the translated instruction set, see cu_avrc.h.


0xF5: Addition anomalies.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Instructions having an addition or subtraction component can be affected by
this feature. This includes post-increments or pre-decrements in loads and
stores and relative jumps along with the normal add, subtract and compare
instructions.

- Byte 0: Add / Subtract failure at bits.
- Byte 1: Carry failure at bits.
- Byte 2: Opcode to be affected.

Add / Subtract failure causes the corresponding source bit to not add or
subtract to the destination.

Carry failure causes the corresponding bit to not receive carry for the
bit level add / subtract operation.

The opcode accords with the translated instruction set, see cu_avrc.h.
