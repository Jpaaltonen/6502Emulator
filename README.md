# A 6502 emulator

## What is it?
A cycle-level 6502 processor emulator I wrote as my Bachelor's thesis. The theoretical part can be found at https://urn.fi/URN:NBN:fi:amk-202304054800.
It emulates the action and behavior of the original 6502 processor at a cycle level, giving user information on memory and register contents and the action being taken at each cycle. Only valid opcodes are emulated, illegal opcodes will hang the processor.

A Windows executable is provided along with the sources. Linux compatibility has not been tested, but the GUI uses OneLoneCoder PixelGameEngine by David Barr which should be Linux compatible. *Note* main.cpp includes a Windows specific instruction that hides the Windows cli when starting the monitor mode. If running on linux, this has to be removed.

## Usage
You can run the emulator either as a monitor that shows you information about cycle actions, pin states and register contents. Alternatively you can run a core test, that utilizes the testsuite by Klaus Dormann's that extensively tests all valid opcodes. The test can be run within the monitor, too, but this is not recommended. The comprehensive ADC/SBC test (test nr. 41) will take orders of magnitude longer than the other tests. Even if running just the core test, the test 41 can take almost a minute to complete (compared to milliseconds or even fractions of milliseconds all previous tests take).

Two sample binaries are provided for demonstration purposes - `test.bin` is the aforementioned testsuite, and `Demo.bin` is a small looping program to quickly demonstrate the functionality of the application

| Key   | Function                                                                         |
| ----- | -------------------------------------------------------------------------------- |
| F1    | Increase speed for continuous mode                                               |
| F2    | Decrease speed for continuous mode                                               |
| ESC   | Open/close console                                                               |
| SPACE | Advance clock in step mode                                                       |
| LEFT  | Previous memory page (wraps around), not applicable if following the PC          |
| RIGHT | Next memory page (wraps around), not applicable if following the PC              |
| UP    | Increase memory page by `$10` (wraps around), not applicable if following the PC |
| DOWN  | Decrease memory page by `$10` (wraps around), not applicable if following the PC |
| C     | Toggle between continuous and step modes                                         |
| F     | Toggle the PC following mode on or off                                           |
| I     | Trigger interrupt request                                                        |
| N     | Trigger non-maskable interrupt request                                           |
| R     | Trigger reset sequence                                                           |



| Command          | Function                                                                                                                                                                                                                                              |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `cls`            | Clear the console                                                                                                                                                                                                                                     |
| `load [file]`    | Load a binary file. Both the command and the operand are case-sensitive.                                                                                                                                                                              |
| `jump [address]` | Move the PC to the address given by the user. The command is case-sensitive, and the address is given in hexadecimal, either with the prefix `$` or without it. Note that olcPGE uses a UK keyboard layout, and the combination for `$` is `Shift+4`. |

