
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


The emulator accepts one parameter: the binary to run as a .hex file.

If a binary is provided proper, it only produces output on the standard output
channel according to the code within the binary. This can be used to generate
test results.

It adds a single new line (an ASCII '\n' character) to the end of the output,
useful for normal test report generation if the code can not be relied upon to
finish text lines. This is in main.c should it be necessary to remove it.



Output features
------------------------------------------------------------------------------


The following ports are used to generate text onto the standard output
channel:

- 0xE0: Single character output.
- 0xE1: Output a decimal number ("0" - "255").
- 0xE2: Output a hexadecimal number ("00" - "FF").
- 0xE3: Output a binary number ("00000000" - "11111111").



Port reset, locks and program termination
------------------------------------------------------------------------------


The emulator's behaviour can be modified from within the running program by
writing to certain ports. The ports below are used to control the overall
process.

- 0xE7: Terminate program.
- 0xE8: Guard port. A second access to this terminates the program.
- 0xEA: Reset sequentially accessed ports.



Setting emulation goals
------------------------------------------------------------------------------


By default the emulator attempts to emulate eight million cycles before
returning. This can be modified by setting an emulation target using the
following ports:

- 0xEB: 4 byte target cycle count to emulate.

The target cycle count port accepts low byte first. It can be used to set the
total count of cycles to emulate. Note that the cycle counter can not be
reset, so at worst case the emulator will return after approximately 4 billion
emulated cycles.



Altering internal behaviour
------------------------------------------------------------------------------


Internal behaviour of the ALU can be altered by writing to certain ports:

- 0xF0: Behaviour modifications can be enabled if this is set 0x5A.
- 0xF1: Register / RAM Memory stuck bits.
- 0xF2: ROM stuck or altered bits.
- 0xF3: Instruction related flag behaviour anomalies.
- 0xF4: Instruction destination anomalies.
- 0xF5: Addition anomalies.
- 0xF6: Instruction skipping.
- 0xF7: Condition disable.


0xF0: Behaviour modifications enable.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Writing this port to 0x5A enables behaviour modifications, but only after the
execution of an "ijmp" instruction. Writing the port to any other value than
0x5A disables all behaviour modifications.

Recommended usage is so calling the function to test with a processor having
anomalies enabled using an "ijmp" instruction. The "ijmp" then is the last
instruction executing with the modifications disabled.

When behaviour modifications are enabled, all ports in the 0xE9 - 0xFF range
become unaccessible except for 0xF0 for disabling behaviour modifications.


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

Stuck bits only affects reads, not writes. So by disabling ALU modifications,
it is possible to reveal the last written value to these locations (this is
useful if the tested code outputs result data into an area affected by this
feature).


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

Flags can be made stuck cleared or set after instructions. Upon the
execution of the affected instruction, the flags (SREG) will be modified
according to the OR and AND masks defined for it.

- Byte 0: Instruction mask, low
- Byte 1: Instruction mask, high
- Byte 2: Compare value, low
- Byte 3: Compare value, high
- Byte 4: OR mask for the flags.
- Byte 5: AND mask for the flags.

The feature applies the Instruction mask (AND) on the opcode word to process,
then if the result matches the Compare value, after the processing of the
instruction, the flags are modified according to the OR & AND masks.


0xF4: Instruction destination anomalies.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

(Not implemented yet)

Bits can be made stuck set or cleared in the destination of instructions
having one. These will be applied after the execution of the instruction.

- Byte 0: OR mask for the destination.
- Byte 1: AND mask for the destination.
- Byte 2: Opcode to be affected.

The opcode accords with the translated instruction set, see cu_avrc.h.


0xF5: Increment / decrement anomalies.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Instructions which could be used or have an increment or decrement component
(such as addition, subtraction, comparison, loads and stores) can be affected
by this feature (instructions in the add / subtract group only when the source
operand is one).

- Byte 0: Failing value, low byte.
- Byte 1: Failing value, high byte.
- Byte 2: Opcode to be affected.

If the destination or the value which should be affected by the increment or
decrement matches the value provided, the increment or decrement is cancelled,
if any flags would be affected, they wouldn't be modified.

The opcode accords with the translated instruction set, see cu_avrc.h. If the
opcode is invalid or have no increment / decrement component, then this feature
will have no effect.


0xF6: Instruction skipping
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

An instruction or a group of instructions can be skipped (turning them into
NOPs) by this feature.

- Byte 0: Skip mask, low
- Byte 1: Skip mask, high
- Byte 2: Compare value, low
- Byte 3: Compare value, high

The feature applies the Skip mask (AND) on the opcode word to process, then
if the result matches the Compare value, the instruction is executed as a
NOP.

If the Skip mask is zero, the feature is turned off. By default it is turned
off.


0xF7: Condition disable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A conditional branch or skip instruction can be made always taken by this
feature (similar to the instruction skipping feature above).

- Byte 0: Instruction mask, low
- Byte 1: Instruction mask, high
- Byte 2: Compare value, low
- Byte 3: Compare value, high

The feature applies the Instruction mask (AND) on the opcode word to process,
then if the result matches the Compare value, the branch or skip is always
taken.

If the Instruction mask is zero, the feature is turned off. By default it is
turned off.

