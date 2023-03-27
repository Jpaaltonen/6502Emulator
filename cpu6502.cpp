#include <iostream>
#include <cstdint>
#include "cpu6502.h"
#include <string>

#define CODE_LIMIT 10

//Decimal to hexadecimal conversion
std::string Hex(int n, int d)
{
	std::string str(d, '0'); //Create a string with d amount of digits (in practice 2 or 4), all zeroes
	for (int i = d - 1; i >= 0; i--) //Start looping from the last character in the result string
	{
		str[i] = "0123456789ABCDEF"[n & 0x0F]; //Change the character to one in the hex string - performing a bitwise and with 0x0F gives us the correct index
		n >>= 4; //Shift bits of value to be converted to the right by four (the amount of bits in one hexadecimal digit)
	}
	return str;
}



cpu6502::cpu6502(bool core)
{
	code.resize(CODE_LIMIT);
	mem.resize(1024 * 64); //Set memory size to 64k, all zeroes
	Reset(core);
}

cpu6502::~cpu6502()
{

}


void cpu6502::LoadProgram(std::vector<char> prg)
{
	//Copy the contents of a vector to memory starting from address given in the first two bytes
	uint16_t tmp = (prg[1]<<8)+prg[0];
	for (int i = 0; i < prg.size(); i++)
	{

		if (tmp <= 0xFFFF)
		{
			mem[tmp] = prg[i];
			tmp++;
		}
	}
	PC = (mem[0xFFFD] << 8) + mem[0xFFFC];
	addrBus = PC;
	dataBus = (uint8_t)mem[addrBus];
	opcode = dataBus;

	DisAsm(); //Update the list of disassembled code
	addrMode = "";
	opcodeAction = "";
	cycleAction = "FETCH AN OPCODE";
}

//Loader for test program stored in a vector
void cpu6502::LoadProgram(std::vector<char> prg, uint16_t start)
{
	//Copy the contents of a vector to memory starting from PC
	uint16_t tmp= start;
	for (int i = 0; i < prg.size(); i++)
	{

		if (tmp <= 0xFFFF)
		{
			mem[tmp] = prg[i];
			tmp++;
		}
	}

}

//Warm reset only resets PC and SP but leaves memory and registers intact, this allows using possibly overwritten reset/interrupt vectors
void cpu6502::Reset(bool core)
{
	opcode = -1;
	cycles = 0;
	totalCycles = 0;
	tick = 0;
	t = 0;
	//Fetch the PC from reset vector
	PC = mem[0xFFFC] | mem[0xFFFD] << 8;
	SP = 0x01FF;

	//Also reset the last memory accesses and explanation strings
	lastReadAddr = lastWriteAddr = -1;
	addrMode = "";
	opcodeAction = "";
	cycleAction = "";

	SetFlag(U, 1);
	//Reset helper flags

	jump = false;
	branch = false;
	irq = false;
	brk = false;
	nmi = false;
	exec = false;

	//Set reset and interrupt sequence execution to true if running in monitor
	if (!core)
	{
		SP = 0x100; //Set SP to bottom of the stack - Reset sequence makes three pushes to the stack, and ends up at 0x1FD
		reset = true;
		runInt = true;
	}
	//Set them to false if running the core test. Also set the SP to the top of the stack
	else
	{
		reset = false;
		runInt = false;
		SP = 0x1ff;
	}
}

//Disassembles the next 10 instructions every time an opcode (or an interrupt/reset sequence) has been completed.
//Another approach would have been to assume that the program would be unmutable, but this would leave out the possibility
//of altering opcodes on the fly or reusing the operands as opcodes by branching/jumping to them
void cpu6502::DisAsm()
{

	uint16_t tmpPC=PC;
	uint8_t offset,tmp;
	uint16_t effAddr;
	std::string s;
	for (int i = 0; i < CODE_LIMIT; i++)
	{		
		(this->*inst[opcode])();
		s ="$"+Hex(tmpPC, 4) + ":\t" + instruction;

		switch (mode)
		{
		case ACC:
			break;
		case IMM:
			++tmpPC;
			s = s + "\t#$" + Hex(mem[tmpPC], 2);
			if (mem[tmpPC] >= 0x80)
			{
				tmp = (~(mem[tmpPC]) + 1);
				s += "\t(-" + std::to_string(tmp) + ")";
			}
			else
				s += "\t(" + std::to_string(mem[tmpPC]) + ")";
			break;
		case IMP:
			break;
		case ZERO:
			++tmpPC;
			s = s + "\t$" + Hex(mem[tmpPC], 2);
			break;
		case ZEROX:
			++tmpPC;
			s = s + "\t$" + Hex(mem[tmpPC], 2)+",X";
			break;
		case ZEROY:
			++tmpPC;
			s = s + "\t$" + Hex(mem[tmpPC], 2) + ",Y";
			break;
		case ABS:
			s = s + "\t$" + Hex(mem[tmpPC+2], 2)+Hex(mem[tmpPC+1],2);
			tmpPC += 2;
			break;
		case ABSX:
			s = s + "\t$" + Hex(mem[tmpPC + 2], 2) + Hex(mem[tmpPC + 1], 2)+",X";
			tmpPC += 2;
			break;
		case ABSY:
			s = s + "\t$" + Hex(mem[tmpPC + 2], 2) + Hex(mem[tmpPC + 1], 2) + ",Y";
			tmpPC += 2;
			break;
		case IND:
			effAddr=(mem[tmpPC+2])<<8|(mem[tmpPC+1]);
			effAddr = (mem[effAddr + 1]) << 8 | (mem[effAddr]);
			s = s + "\t($" + Hex(mem[tmpPC + 2], 2) + Hex(mem[tmpPC + 1], 2)+")\t[$"+Hex(effAddr,4)+"]";
			tmpPC += 2;
			break;
		case INDX:
			++tmpPC;
			s = s + "\t($" + Hex(mem[tmpPC], 2)+",X)";
			break;
		case INDY:
			++tmpPC;
			s = s + "\t($" + Hex(mem[tmpPC], 2) + "),Y";
			break;
		case REL:
			++tmpPC;
			offset = mem[tmpPC];
			s = s + "\t$" + Hex(offset, 2);
			if (offset >= 0x80)
			{
				tmp = ((~offset) + 1);
				s += "\t[$" + Hex((tmpPC + 1) - tmp, 4)+"]";
			}
			else
				s += "\t[" + Hex(tmpPC + 1 + offset, 4)+"]";
			break;
		case XXX:			
			break;
		}
		++tmpPC;
		code[i] = s;
		t = 0;
		opcode = (uint8_t)mem[tmpPC];
	}
	opcode =(uint8_t) mem[PC];
}

//The clock function
void cpu6502::Clock()
{
	tick++;
	if (tick % 2 == 1 ) //Advance the clock every other tick, or half-cycle, i.e. only when a full clock cycle has elapsed
	{

		
		if (cycles - t == 0) //If the previous instruction has been completed
		{
			t = 0;
			if (irq || nmi)
			{
				addrMode = "";
				runInt = true;
				if (jump)
					PC--; //If interrupt happens during a control flow command, 
			}
			else
			{
				//Increase the PC only if the first opcode has been fetched, and the PC hasn't already been set by jump or Branch instructions
				if (opcode != -1 && !jump)
					PC++;
				if (exec) //For some instructions execution happens simultaneously with fetching the new opcode
				{
					cycleAction = "EXECUTE PREVIOUS INSTRUCTION\nAND FETCH A NEW OPCODE";
					exec = false;
				}
				else
					cycleAction = "FETCH AN OPCODE";

				addrMode = "";
				opcodeAction = "";

				//Fetch the opcode from the PC location
				addrBus = PC;
				dataBus = (uint8_t)mem[addrBus];
				opcode = dataBus;

				DisAsm(); //Update the list of disassembled code
				t = 0;
			}
		}

		if (runInt)
		{
			InterruptOP();
			t++;
			if (t == cycles)
			{
				runInt = false;
				//PC++;
			}
		}
		else
			(this->*inst[opcode])(); //Call the function of the current opcode
		SP = 0x100 | (SP & 0xFF); //Fail safe for ensuring the SP really IS confined to the first page - SP is updated in several instructions, so it's possible to miss one.
		if (rw)
			lastReadAddr = addrBus;
		else
			lastWriteAddr = addrBus;
		totalCycles++;
	}

}

void cpu6502::SetFlag(uint8_t flag, bool val)
{
	//Perform a bitwise OR with the designated flag if value is true
	if (val)
		P |= flag;
	//Otherwise flip the bits in the flag and perform a bitwise AND - as each flag has been predefined to be 2^n (n[0,7]),
	//the unary operation results in a value where every bit except for the flag bit is set, thus preserving the state of other bits
	else
		P &= ~flag;
}


//Definitions for opcode-functions

void cpu6502::ADC()
{
	instruction = "ADC";
	if (t == 0)
	{
		opcodeAction = "ADD DATA AND CARRY TO A";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		uint8_t tmp = A; //Store A so we can determine carry and overflow flags after the operation
		uint16_t result;
		if (!(P&D)) //Decimal flag not set, perform adding as usual
		{
			result=(uint16_t)A + (uint16_t)(P & C) + (uint16_t)dataBus;
		}
		else
		{			
			result = ((uint16_t)A & 0x0F) + ((uint16_t)dataBus & 0x0F) + (uint16_t)(P & C); //Handle the least significant digit first
			if (result > 0x09) result += 6; //If result was larger than 9, add 6
			result = ((uint16_t)A & 0xF0) + ((uint16_t)dataBus & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0F); //Handle the most significant digit last - if the result of previous operation was higher than 0x0F, add 0x10
			if (result >= 0xA0) result += 0x60;	
		}
		SetFlag(C, result > 255); //If result is larger than A, the result was larger than 8 bits, so the carry flag is set
		A = result & 0xFF;
		SetFlag(N, A & N); //If the 7th bit is set, negative flag is set
		SetFlag(Z, A == 0); //If the contents of A is 0 after the operation, zero flag is set
		SetFlag(V, (tmp ^ A) & (dataBus ^ A) & N);
	}
}

void cpu6502::AND()
{
	instruction = "AND";
	if (t == 0)
	{
		opcodeAction = "PERFORM A BITWISE AND TO A WITH DATA";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{

		A &= dataBus;
		SetFlag(N, A & N);
		SetFlag(Z, A == 0);
	}
}

void cpu6502::ASL()
{
	instruction = "ASL";
	if (t == 0)
	{

		GetAddrMode(Group1);
		if (mode == ACC)
			opcodeAction = "PERFORM AN ARITHMETIC LEFT\nSHIFT TO A";
		else
			opcodeAction = "PERFORM AN ARITHMETIC LEFT\nSHIFT TO DATA";
	}
	//If the target is accumulator, instruction takes only one byte
	if (mode == ACC)
		ByteOp();
	else
		RMWOp();
	t++;
	if (t == cycles)
	{

		if (mode == ACC)
		{
			SetFlag(C, (A & N)); //As the bits move to the left, the MSB will be shifted to carry - to do this to the original MSB, the flag must be set before the operation
			A <<= 1;
			SetFlag(N, (A & N)); //If the new MSB is set, negative flag is set
			SetFlag(Z, A == 0);
		}
		else
		{
			SetFlag(C, (dataBus & N));
			dataBus <<= 1;
			SetFlag(N, (dataBus & N));
			SetFlag(Z, dataBus == 0);
			mem[addrBus] = dataBus;
		}
	}
}

void cpu6502::BCC()
{

	instruction = "BCC";
	if (t == 0)
	{
		branch = !(P & C);
		opcodeAction = "BRANCH IF CARRY FLAG IS CLEAR";
	}

	BranchOp();
	t++;
}

void cpu6502::BCS()
{

	instruction = "BCS";
	if (t == 0)
	{
		branch = P & C;
		opcodeAction = "BRANCH IF CARRY FLAG IS SET";
	}

	BranchOp();
	t++;
}

void cpu6502::BEQ()
{

	instruction = "BEQ";
	if (t == 0)
	{
		branch = P & Z;
		opcodeAction = "BRANCH IF ZERO FLAG IS SET";
	}

	BranchOp();
	t++;
}

void cpu6502::BIT()
{
	instruction = "BIT";
	if (t == 0)
	{
		exec = true;
		//The fourth bit defines the addressing mode
		if (opcode & 0b1000)
		{
			cycles = 4;
			mode = ABS;
			addrMode = "ABSOLUTE";
		}

		else
		{
			cycles = 3;
			mode = ZERO;
			addrMode = "ZERO PAGE";
		}
	}
	MemOp();
	//BIT performs a logcial AND between operand and A without altering A
	//The 6th and 7th bit of the operand are transferred to V and N, respectively
	t++;
	if (t == cycles)
	{

		SetFlag(V, dataBus & V);
		SetFlag(N, dataBus & N);
		SetFlag(Z, (A & dataBus) == 0);
	}
}

void cpu6502::BMI()
{

	instruction = "BMI";
	if (t == 0)
	{
		branch = P & N;
		opcodeAction = "BRANCH IF NEGATIVE FLAG IS SET";
	}

	BranchOp();
	t++;
}

void cpu6502::BNE()
{

	instruction = "BNE";
	if (t == 0)
	{
		branch = !(P & Z);
		opcodeAction = "BRANCH IF ZERO FLAG IS CLEARED";
	}
	BranchOp();
	t++;
}

void cpu6502::BPL()
{

	instruction = "BPL";
	if (t == 0)
	{
		branch = !(P & N);
		opcodeAction = "BRANCH IF NEGATIVE FLAG IS CLEARED";
	}

	BranchOp();
	t++;
}

void cpu6502::BRK()
{
	mode = IMP;
	instruction = "BRK";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		brk = true;
	}
	InterruptOP();
	t++;

}

void cpu6502::BVC()
{

	instruction = "BVC";
	if (t == 0)
	{
		branch = !(P & V);
		opcodeAction = "BRANCH IF OVERFLOW FLAG IS CLEARED";
	}

	BranchOp();
	t++;
}

void cpu6502::BVS()
{
	instruction = "BVS";
	if (t == 0)
	{

		branch = P & V;
		opcodeAction = "BRANCH IF OVERFLOW FLAG IS SET";
	}

	BranchOp();
	t++;
}

void cpu6502::CLC()
{
	mode = IMP;
	instruction = "CLC";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "CLEAR CARRY FLAG";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "CLEAR CARRY FLAG";
		SetFlag(C, 0);
	}
}

void cpu6502::CLD()
{
	mode = IMP;
	instruction = "CLD";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "CLEAR DECIMAL FLAG";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "CLEAR DECIMAL FLAG";
		SetFlag(D, 0);
	}
}

void cpu6502::CLI()
{
	mode = IMP;
	instruction = "CLI";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "CLEAR INTERRUPT FLAG";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "CLEAR INTERRUPT FLAG";
		SetFlag(I, 0);
	}
}

void cpu6502::CLV()
{
	mode = IMP;
	instruction = "CLV";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "CLEAR OVERFLOW FLAG";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "CLEAR OVERFLOW FLAG";
		SetFlag(V, 0);
	}
}

void cpu6502::CMP()
{
	instruction = "CMP";

	if (t == 0)
	{
		opcodeAction = "COMPARE THE VALUES OF A\nAND FETCHED DATA";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		SetFlag(Z, A == dataBus);
		SetFlag(C, A >= dataBus);
		SetFlag(N, (A - dataBus) & N);
	}
}

void cpu6502::CPX()
{
	instruction = "CPX";
	if (t == 0)
	{
		opcodeAction = "COMPARE THE VALUES OF X\nAND FETCHED DATA";
		GetAddrMode(Group2B);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		SetFlag(Z, X == dataBus);
		SetFlag(C, X >= dataBus);
		SetFlag(N, (X - dataBus) & N);

	}
}

void cpu6502::CPY()
{
	instruction = "CPY";
	if (t == 0)
	{
		opcodeAction = "COMPARE THE VALUES OF Y\nAND FETCHED DATA";
		GetAddrMode(Group2B);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		SetFlag(Z, Y == dataBus);
		SetFlag(C, Y >= dataBus);
		SetFlag(N, (Y - dataBus) & N);
	}
}

void cpu6502::DEC()
{
	instruction = "DEC";
	if (t == 0)
	{
		GetAddrMode(Group2A);
		opcodeAction = "DECREMENT VALUE IN MEMORY BY 1";
	}
	RMWOp();
	t++;
	if (t == cycles)
	{
		dataBus--;
		mem[addrBus] = dataBus;
		SetFlag(Z, dataBus == 0);
		SetFlag(N, dataBus & N);
	}

}

void cpu6502::DEX()
{
	mode = IMP;
	instruction = "DEX";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "DECREMENT X BY 1";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		X--;
		SetFlag(Z, X == 0);
		SetFlag(N, X & N);
		cycleAction = "DECREMENT X BY 1";
	}

}

void cpu6502::DEY()
{
	mode = IMP;
	instruction = "DEY";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "DECREMENT Y BY 1";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		Y--;
		SetFlag(Z, Y == 0);
		SetFlag(N, Y & N);
		cycleAction = "DECREMENT Y BY 1";
	}
}

void cpu6502::EOR()
{
	instruction = "EOR";
	if (t == 0)
	{
		GetAddrMode(Group1);
		opcodeAction = "PERFORM EXCLUSIVE OR ON A\nWITH DATA";
	}
	MemOp();
	t++;

	if (t == cycles)
	{
		A ^= dataBus;
		SetFlag(N, A & N);
		SetFlag(Z, A == 0);
	}
}

void cpu6502::INC()
{
	instruction = "INC";
	if (t == 0)
	{
		GetAddrMode(Group2A);
		opcodeAction = "INCREMENT VALUE IN MEMORY BY 1";
	}
	RMWOp();
	t++;
	if (t == cycles)
	{
		dataBus++;
		mem[addrBus] = dataBus;
		SetFlag(Z, dataBus == 0);
		SetFlag(N, dataBus & N);
	}
}

void cpu6502::INX()
{
	mode = IMP;
	instruction = "INX";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "INCREMENT X BY 1";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		X++;
		SetFlag(Z, X == 0);
		SetFlag(N, X & N);
		cycleAction = "INCREMENT X BY 1";
	}
}

void cpu6502::INY()
{
	mode = IMP;
	instruction = "INY";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "INCREMENT Y BY 1";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		Y++;
		SetFlag(Z, Y == 0);
		SetFlag(N, Y & N);
		cycleAction = "INCREMENT Y BY 1";
	}
}

void cpu6502::JMP()
{
	instruction = "JMP";
	if (t == 0)
	{
		opcodeAction = "JUMP TO A NEW LOCATION";
		jump = true; //The jump is not conditional, and the PC for the next opcode will be fetched by the contents of address bus instead of just increasing it
		if (opcode & 0b00100000) //The 5th bit of the defines the addressing mode - If it is set, the mode is indirect, if not, the mode is absolute
		{
			addrMode = "INDIRECT";
			mode = IND;
			cycles = 5;
		}
		else
		{
			addrMode = "ABSOLUTE";
			mode = ABS;
			cycles =3;
		}
	}


	switch (mode)
	{
	case ABS:
		switch (t)
		{
		case 0:
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];
			
			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF JUMP ADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF JUMP ADDRESS";
			PC = effectiveAddr; //Set the new location
			break;
		}
		break;

	case IND:
		switch (t)
		{
		case 0:
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];
			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF INDIRECT ADDRESS";
			break;
		case 2:			
			addrBus = ++PC;		
			dataBus = mem[addrBus];			
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF INDIRECT ADDRESS";
			break;
		case 3:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];

			cycleAction = "FETCH LOW BYTE OF JUMP ADDRESS";
			break;
		case 4:			
			
			if (effectiveAddr & 0xFF == 0xFF)
			{
				std::cout << std::to_string(effectiveAddr) << std::endl;
				effectiveAddr -= 0x100; //There's a bug in indirect jump - if the low byte of jump address is fetched from the last byte of the page, the high byte is fetched from the first byte of that same page
				std::cout << std::to_string(effectiveAddr) << std::endl;
			}
			addrBus = effectiveAddr + 1;
			effectiveAddr = dataBus;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF JUMP ADDRESS";
			PC = effectiveAddr;
			break;
		}
		break;
	}
	t++;
}

void cpu6502::JSR()
{
	instruction = "JSR";
	mode = ABS;
	switch (t)
	{
	case 0:
		cycles = 6;
		rw = true;
		discarded = false;
		addrBus = PC;
		dataBus = mem[addrBus];
		jump = true;
		opcodeAction = "JUMP TO SUBROUTINE LOCATION";
		addrMode = "ABSOLUTE";
		break;
	case 1:
		addrBus = ++PC;
		dataBus = mem[addrBus];
		effectiveAddr = dataBus;
		cycleAction = "FETCH LOW BYTE OF SUBROUTINE\nADDRESS";
		break;
	case 2:
		addrBus = SP;
		discarded = true;
		dataBus = mem[addrBus];
		cycleAction = "";
		break;
	case 3:
		discarded = false;
		rw = false;
		dataBus = ((PC+1) & 0xFF00) >> 8;
		mem[addrBus] = dataBus;
		--SP;
		
		cycleAction = "PUSH HIGH BYTE OF PC TO STACK";
		break;
	case 4:
		addrBus = SP;
		dataBus = (PC+1) & 0xFF;
		mem[addrBus] = dataBus;
		--SP;
		cycleAction = "PUSH LOW BYTE OF PC TO STACK";
		break;
	case 5:
		rw = true;
		addrBus = ++PC;
		dataBus = mem[addrBus];
		effectiveAddr |= (dataBus << 8);
		cycleAction = "FETCH HIGH BYTE OF SUBROUTINE\nADDRESS";
		PC = effectiveAddr;
		break;
	}
	t++;
	if(t==cycles)
		PC = effectiveAddr;
}

void cpu6502::LDA()
{
	instruction = "LDA";
	if (t == 0)
	{
		opcodeAction = "LOAD A NEW VALUE TO A";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		A = dataBus;
		cycleAction = "LOAD A NEW VALUE TO A";
		SetFlag(N, A & N);
		SetFlag(Z, A == 0);
	}
}

void cpu6502::LDX()
{
	instruction = "LDX";
	if (t == 0)
	{
		opcodeAction = "LOAD A NEW VALUE TO X";
		GetAddrMode(Group1);
		if (mode == ZEROX)
			mode = ZEROY;
		if (mode == ABSX)
			mode = ABSY;
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		X = dataBus;
		cycleAction = "LOAD A NEW VALUE TO X";
		SetFlag(N, X & N);
		SetFlag(Z, X == 0);
	}
}

void cpu6502::LDY()
{
	instruction = "LDY";
	if (t == 0)
	{
		opcodeAction = "LOAD A NEW VALUE TO Y";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		Y = dataBus;
		cycleAction = "LOAD A NEW VALUE TO Y";
		SetFlag(N, Y & N);
		SetFlag(Z, Y == 0);
	}
}

void cpu6502::LSR()
{
	instruction = "LSR";
	if (t == 0)
	{

		GetAddrMode(Group1);
		if (mode == ACC)
			opcodeAction = "PERFORM A LOGICAL RIGHT\nSHIFT TO A";
		else
			opcodeAction = "PERFORM A LOGICAL RIGHT\nSHIFT TO DATA";
	}
	//If the target is accumulator, instruction takes only one byte
	if (mode == ACC)
		ByteOp();
	else
		RMWOp();
	t++;
	if (t == cycles)
	{

		if (mode == ACC)
		{
			SetFlag(C, (A & 1)); //As the bits move to the right, the LSB will be shifted to carry - to do this to the original LSB, the flag must be set before the operation
			A >>= 1;
			SetFlag(Z, A == 0);
		}
		else
		{
			SetFlag(C, (dataBus & 1));
			dataBus >>= 1;
			SetFlag(Z, dataBus == 0);
			mem[addrBus] = dataBus;
			lastWriteAddr = addrBus;
		}
		SetFlag(N, 0); //As 0 is shifted to the MSB, N flag is always off
	}
}

void cpu6502::NOP()
{
	instruction = "NOP";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "NO OPERATION";
	}
	ByteOp();
	t++;
}

void cpu6502::ORA()
{
	instruction = "ORA";
	if (t == 0)
	{
		opcodeAction = "ORA";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		A |= dataBus;
		SetFlag(N, A & N);
		SetFlag(Z, A == 0);
	}
}

void cpu6502::PHA()
{
	mode = IMP;
	instruction = "PHA";
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "PHA";
		cycles = 3;
	}
	PushOp();
	t++;
}

void cpu6502::PHP()
{
	instruction = "PHP";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "PHP";
		cycles = 3;
	}
	PushOp();
	t++;
}

void cpu6502::PLA()
{
	instruction = "PLA";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "PLA";
		cycles = 4;
	}
	PullOp();
	t++;
}

void cpu6502::PLP()
{
	instruction = "PLP";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "PLP";
		cycles = 4;
	}
	PullOp();
	t++;
}

void cpu6502::ROL()
{
	instruction = "ROL";
	if (t == 0)
	{

		GetAddrMode(Group1);
		if (mode == ACC)
			opcodeAction = "PERFORM A BITWISE LEFT\nROTATION TO A";
		else
			opcodeAction = "PERFORM A BITWISE LEFT\nROTATION TO DATA";
	}
	//If the target is accumulator, instruction takes only one byte
	if (mode == ACC)
		ByteOp();
	else
		RMWOp();
	t++;
	if (t == cycles)
	{

		if (mode == ACC)
		{
			uint8_t tmp = P & C; //Old carry flag will be rotated to bit 0, and it's new state will be defined by the original MSB of the value
			SetFlag(C, (A & N)); //Set the new carry before the shift
			A <<= 1;
			A |= tmp;
			SetFlag(Z, A == 0);
			SetFlag(N, A & N);
		}
		else
		{
			uint8_t tmp = P & C;
			SetFlag(C, (dataBus & N));
			dataBus <<= 1;
			dataBus |= tmp;
			SetFlag(Z, dataBus == 0);
			SetFlag(N, dataBus & N);
			mem[addrBus] = dataBus;
			lastWriteAddr = addrBus;
		}
	}
}

void cpu6502::ROR()
{
	instruction = "ROR";
	if (t == 0)
	{

		GetAddrMode(Group1);
		if (mode == ACC)
			opcodeAction = "PERFORM A BITWISE RIGHT\nROTATION TO A";
		else
			opcodeAction = "PERFORM A BITWISE RIGHT\nROTATION TO DATA";
	}
	//If the target is accumulator, instruction takes only one byte
	if (mode == ACC)
		ByteOp();
	else
		RMWOp();
	t++;
	if (t == cycles)
	{

		if (mode == ACC)
		{
			uint8_t tmp = P & C; //Old carry flag will be rotated to bit 7, and it's new state will be defined by the original LSB of the value
			SetFlag(C, (A & 1)); //Set the new carry before the shift
			A >>= 1;
			A |= (tmp << 7); //Shift the preserved carry to 7th bit
			SetFlag(Z, A == 0);
			SetFlag(N, A & N);
		}
		else
		{
			uint8_t tmp = P & C;
			SetFlag(C, (dataBus & 1));
			dataBus >>= 1;
			dataBus |= (tmp << 7);
			SetFlag(Z, dataBus == 0);
			SetFlag(N, dataBus & N);
			mem[addrBus] = dataBus;
			lastWriteAddr = addrBus;
		}
	}
}

void cpu6502::RTI()
{
	mode = IMP;
	instruction = "RTI";
	uint8_t tmp;
	switch (t)
	{
	case 0:
		rw = true;
		discarded = false;
		jump = true;
		cycles = 6;
		addrMode = "IMPLIED";
		opcodeAction = "RETURN FROM INTERRUPT";
		addrBus = PC;
		dataBus = mem[PC];
		break;
	case 1:
		addrBus = ++PC;
		dataBus = mem[PC];
		discarded = true;
		cycleAction = "";
		break;
	case 2:
		addrBus = SP;
		dataBus = mem[addrBus];
		cycleAction = "";
		break;
	case 3:
		discarded = false;
		SP = 0x100 | ((SP + 1) & 0xFF);
		addrBus = SP;
		dataBus = mem[addrBus];
		cycleAction = "PULL STATUS REGISTER FROM STACK";
		tmp = P;
		P = dataBus;
		//Set the B and U flags - U is always on, B is cleared only when pushed to stack during interrupt sequence
		SetFlag(B, 1);
		SetFlag(U, 1);
		break;
	case 4:
		SP = 0x100 | ((SP + 1) & 0xFF);
		addrBus = SP;
		dataBus = mem[addrBus];
		effectiveAddr = dataBus;
		cycleAction = "PULL LOW BYTE OF RETURN ADDRESS\nFROM STACK";
		break;
	case 5:
		SP = 0x100 | ((SP + 1) & 0xFF);
		addrBus = SP;
		dataBus = mem[addrBus];
		effectiveAddr |= (dataBus << 8);
		cycleAction = "PULL HIGH BYTE OF RETURN ADDRESS\nFROM STACK";
		PC = effectiveAddr+1;
		break;
	}
	t++;


}

void cpu6502::RTS()
{
	instruction = "RTS";
	mode = IMP;
	switch (t)
	{
	case 0:
		rw = true;
		discarded = false;
		jump = false;
		cycles = 6;
		addrMode = "IMPLIED";
		opcodeAction = "RETURN FROM SUBROUTINE";
		addrBus = PC;
		dataBus = mem[PC];
		break;
	case 1:
		addrBus = ++PC;
		dataBus = mem[PC];
		discarded = true;
		cycleAction = "";
		break;
	case 2:
		addrBus = SP;
		dataBus = mem[addrBus];
		cycleAction = "";
		break;
	case 3:
		discarded = false;
		SP = 0x100 | ((SP + 1) & 0xFF);
		addrBus = SP;
		dataBus = mem[addrBus];
		effectiveAddr = dataBus;
		cycleAction = "PULL LOW BYTE OF RETURN ADDRESS\nFROM STACK";
		break;
	case 4:
		SP = 0x100 | ((SP + 1) & 0xFF);
		addrBus = SP;
		dataBus = mem[addrBus];
		effectiveAddr |= (dataBus<<8);
		cycleAction = "PULL HIGH BYTE OF RETURN ADDRESS\nFROM STACK";
		break;
	case 5:
		addrBus = effectiveAddr;
		dataBus = mem[addrBus];
		discarded = true;
		cycleAction = "";
		PC = addrBus;
		break;
	}
	t++;
}

void cpu6502::SBC()
{
	instruction = "SBC";
	if(t==0)
	{
		opcodeAction = "SUBTRACT DATA FROM A";
		GetAddrMode(Group1);
	}
	MemOp();
	t++;
	if (t == cycles)
	{
		////Operate with 16-bit numbers to make things easier
		uint16_t data=((uint16_t)dataBus)^0x00FF; //Flip the bits, so we can perform an addition (A-B=A+(-B))
		uint16_t result;

		if(!(P&D))
			result = (uint16_t)A + data + (uint16_t)(P & C);
		else
		{
			result = (uint16_t)(A & 0x0F) + (data & 0x0F) + (uint16_t)(P & C);
			if (result <= 0x0F)
				result -= 6;
			result = (uint16_t)(A & 0xF0) + (data & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0F);
			if (result <= 0xFF)
				result -= 0x60;
		}
		SetFlag(C, result >0xFF);
		SetFlag(Z, ((result & 0xFF) == 0));
		SetFlag(N, result & 0x80);
		SetFlag(V, (result ^ (uint16_t)A) & (result ^ data) & 0x0080);
		A = result & 0xFF;







		//uint8_t tmp = A; //Store A so we can determine carry and overflow flags after the operation
		//uint8_t value = (~dataBus)+1; //Invert the databus, so we can handle SBC as an addition
		//if (!(P & D)) //Decimal flag not set, perform subtracting as usual
		//{
		//	
		//	//A -= (1-(P & C)) - dataBus; //Carry flag indicates subtract WITHOUT borrow. So, if carry is set, we will only subtract the data. If the carry is clear, we will also subtract 1
		//	A += value + (P & C);
		//}
		//else
		//{
		//	uint8_t result;
		//	
		//	result = (A & 0x0F) + (value & 0x0F) + (P & C);
		//	if (result <= 0x0F)
		//		result -= 6;
		//	result = (A & 0xF0) + (value & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0f);
		//	if (result <= 0xFF)
		//		result -= 0x60;
		//	A = result;
		//}

		//SetFlag(C, tmp < A); //If tmp is larger than A, the result was larger than 8 bits, so carry flag is set
		//SetFlag(N, A & N); //If the 7th bit is set, negative flag is set
		//SetFlag(Z, A == 0); //If the contents of A is 0 after the operation, zero flag is set
		//SetFlag(V, (tmp ^ A) & (value ^ A) & 0x80);
	}
	
}

void cpu6502::SEC()
{
	instruction = "SEC";
	mode = IMP;
	if (t == 0)
	{
		opcodeAction = "SET CARRY FLAG";
		addrMode = "IMPLIED";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "SET CARRY FLAG";
		SetFlag(C, 1);
	}
}

void cpu6502::SED()
{
	instruction = "SED";
	mode = IMP;
	if (t == 0)
	{
		opcodeAction = "SET DECIMAL FLAG";
		addrMode = "IMPLIED";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "SET DECIMAL FLAG";
		SetFlag(D, 1);
	}
}

void cpu6502::SEI()
{
	instruction = "SEI";
	mode = IMP;
	if (t == 0)
	{
		opcodeAction = "SET INTERRUPT FLAG";
		addrMode = "IMPLIED";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		cycleAction = "SET INTERRUPT FLAG";
		SetFlag(I, 1);
	}
}

void cpu6502::STA()
{
	instruction = "STA";
	if (t == 0)
	{
		opcodeAction = "STORE CONTENTS OF A INTO MEMORY";
		GetAddrMode(Group1);
	}
	StoreOp();
	t++;
	if (t == cycles)
	{
		dataBus = A;
		cycleAction = "WRITE CONTENTS OF A TO\nADDRESS $" + Hex(addrBus, 4);
		mem[addrBus] = dataBus;
		lastWriteAddr = addrBus;
	}
}

void cpu6502::STX()
{
	instruction = "STX";
	if (t == 0)
	{
		opcodeAction = "STORE CONTENTS OF X INTO MEMORY";
		GetAddrMode(Group1);
		if (mode == ZEROX)
			mode = ZEROY;
	}
	StoreOp();
	t++;
	if (t == cycles)
	{
		dataBus =X;
		cycleAction = "WRITE CONTENTS OF X TO\nADDRESS $" + Hex(addrBus, 4);
		mem[addrBus] = dataBus;
		lastWriteAddr = addrBus;
	}
}

void cpu6502::STY()
{
	instruction = "STY";
	
	if (t == 0)
	{
		opcodeAction = "STORE CONTENTS OF Y INTO MEMORY";
		GetAddrMode(Group1);
	}
	StoreOp();
	t++;
	if (t == cycles)
	{
		dataBus = Y;
		cycleAction = "WRITE CONTENTS OF Y TO\nADDRESS $" + Hex(addrBus, 4);
		mem[addrBus] = dataBus;
		lastWriteAddr = addrBus;
	}
}

void cpu6502::TAX()
{
	instruction = "TAX";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "TRANSFER THE CONTENTS OF A TO X";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		X = A;
		SetFlag(N, X & N);
		SetFlag(Z, X == 0);
	}
}

void cpu6502::TAY()
{
	instruction = "TAY";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "TRANSFER THE CONTENTS OF A TO Y";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		Y = A;
		SetFlag(N, Y & N);
		SetFlag(Z, Y == 0);
	}
}

void cpu6502::TSX()
{
	instruction = "TSX";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "TRANSFER THE CONTENTS OF STACK\nPOINTER TO X";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		X = SP;
		SetFlag(N, X & N);
		SetFlag(Z, X == 0);
	}
}

void cpu6502::TXA()
{
	instruction = "TXA";
	mode = IMP;
	if (t == 0)
	{

		addrMode = "IMPLIED";
		opcodeAction = "TRANSFER THE CONTENTS OF X TO A";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		A = X;
		SetFlag(N, A & N);
		SetFlag(Z, A == 0);
	}
}

void cpu6502::TXS()
{
	instruction = "TXS";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "TRANSFER THE CONTENTS OF X TO STACK POINTER";
	}
	ByteOp();
	t++;
	if (t == cycles)
		SP =0x0100| X;
}

void cpu6502::TYA()
{
	instruction = "TYA";
	mode = IMP;
	if (t == 0)
	{
		addrMode = "IMPLIED";
		opcodeAction = "TRANSFER THE CONTENTS OF Y TO A";
	}
	ByteOp();
	t++;
	if (t == cycles)
	{
		A = Y;
		SetFlag(N, A & N);
		SetFlag(Z, A == 0);
	}
}

void cpu6502::ILL()
{	
	mode = XXX;
	instruction = "???";
	addrMode = "???";
	opcodeAction = "UNDOCUMENTED - UNIMPLEMENTED\nEMULATION JAMMED";
	cycleAction = "???";

}


//Determine the addressing mode and base cycle count for groups 1A, 1B, 2A and 2B (Groups 3A and 3B have only one instruction per group and instructions in group 4 have only one
//addressing mode per instruction, so these can be processed in their respective functions).
//This is done by masking out the rest of the opcode and leaving only the corresponding address mode bits in each instruction group
//It has to be noted that even though groups 1A and 1B have the same bits for the address mode, 1B has only 6 modes instead of 8.
//In addition to this their designations differ, eg. mode 000 is indexed indirect mode in 1A and immediate mode in 1B
//This means we have to handle the groups separately.
//Another edge case that needs handling is when the instruction is LDX/LDY or STX, and has an indexed addressing mode
//As for cycles there are some differences, but as most are identical inside the group, the edge cases can be handled easily
void cpu6502::GetAddrMode(int mask)
{
	//Get the address mode
	mode = opcode & mask;
	int tmp = -1; //A temporary variable to distinguish between groups 1A and 1B
	if (mask == Group1)
	{
		//Shifting bits right by 2 gives us a range of [0,7] which is more convenient,
		//as the variable is used both here and in opcode functions
		mode >>= 2;
		tmp = opcode & 0b11100011; //Get the base instruction value
		//Handle only group A instructions
		if (tmp != eASL && tmp != eLDX && tmp != eLDY && tmp != eLSR && tmp != eROL && tmp != eROR)
		{
			switch (mode)
			{
			case 0:
				mode = INDX;
				addrMode = "INDIRECT X INDEXED";
				if (tmp == eSTA)
					cycles = 6;
				else
					cycles = 6;
				break;
			case 1:
				mode = ZERO;
				addrMode = "ZERO PAGE";
				if (tmp == eSTA)
					cycles = 3;
				else
					cycles = 3;
				break;
			case 2: //Not STA
				mode = IMM;
				addrMode = "IMMEDIATE";
				cycles = 2;
				break;
			case 3:
				mode = ABS;
				addrMode = "ABSOLUTE";
				if (tmp == eSTA)
					cycles = 4;
				else
					cycles = 4;
				break;
			case 4:
				mode = INDY;
				addrMode = "INDIRECT Y INDEXED";
				if (tmp == eSTA)
					cycles = 6;
				else
					cycles = 5;
				break;
			case 5:
				mode = ZEROX;
				addrMode = "ZERO PAGE, X INDEXED";
				if (tmp == eSTA)
					cycles = 4;
				else
					cycles = 4;
				break;
			case 6:
				mode = ABSY;
				addrMode = "ABSOLUTE, Y INDEXED";
				if (tmp == eSTA)
					cycles = 5;
				else
					cycles = 4;
				break;
			case 7:
				mode = ABSX;
				addrMode = "ABSOLUTE, X INDEXED";
				if (tmp == eSTA)
					cycles = 5;
				else
					cycles = 4;
				break;
			}
		}
		else //Group B
			switch (mode)
			{
			case 0:
				//Only for LDX/LDY
				mode = IMM;
				addrMode = "IMMEDIATE";
				cycles = 2;
				break;
			case 1:
				mode = ZERO;
				addrMode = "ZERO PAGE";
				if (tmp == eLDX || tmp == eLDY)
					cycles = 3;
				else
					cycles = 5;
				break;
				//Not for LDY/LDX
			case 2:
				mode = ACC;
				addrMode = "ACCUMULATOR";
				cycles = 2;
				break;
			case 3:
				mode = ABS;
				addrMode = "ABSOLUTE";
				if (tmp == eLDX || tmp == eLDY)
					cycles = 4;
				else
					cycles = 6;
				break;

				//LDX is indexed with Y register and LDY with X
			case 5:

				if (tmp == eLDX)
				{
					cycles = 4;
					mode = ZEROY;
					addrMode = "ZERO PAGE, Y INDEXED";
				}
				else
				{
					if (tmp == eLDY)
						cycles = 4;
					else
						cycles = 6;
					mode = ZEROX;
					addrMode = "ZERO PAGE, X INDEXED";
				}
				break;
			case 7:
				if (tmp == eLDX)
				{
					mode = ABSY;
					addrMode = "ABSOLUTE, Y INDEXED";
					cycles = 4;
				}
				else
				{
					if (tmp == eLDY)
						cycles = 4;
					else
						cycles = 7;
					mode = ABSX;
					addrMode = "ABSOLUTE, X INDEXED";
				}
				break;
			}
	}
	else if (mask == Group2A)
	{
		tmp = opcode & 0b11100111;

		switch (mode >> 3)
		{
		case 0:
			if (tmp == eSTX || tmp == eSTY)
				cycles = 3;
			else
				cycles = 5;
			mode = ZERO;
			addrMode = "ZERO PAGE";
			break;
		case 1:
			if (tmp == eSTX || tmp == eSTY)
				cycles = 4;
			else
				cycles = 6;
			mode = ABS;
			addrMode = "ABSOLUTE";
			break;
		case 2:
			if (tmp == eSTX)
			{
				cycles = 4;
				mode = ZEROY;
				addrMode = "ZERO PAGE, Y INDEXED";
			}
			else
			{
				if (tmp == eSTY)
					cycles = 4;
				else
					cycles = 6;
				mode = ZEROX;
				addrMode = "ZERO PAGE, X INDEXED";
			}
			break;
		case 3:
			//Neither STX or STY has this addressing mode
			cycles = 7;
			mode = ABSX;
			addrMode = "ABSOLUTE, X INDEXED";
			break;

		}
	}
	else if (mask == Group2B)
	{

		switch (mode >> 2)
		{
		case 0:
			cycles = 2;
			mode = IMM;
			addrMode = "IMMEDIATE";
			break;
		case 1:
			cycles = 3;
			mode = ZERO;
			addrMode = "ZERO PAGE";
			break;
		case 3:
			cycles = 4;
			mode = ABS;
			addrMode = "ABSOLUTE";
			break;
		}
	}

}

//Function for opcodes that take only one byte - ie. the opcode itself
void cpu6502::ByteOp()
{
	switch (t)
	{
	case 0:
		jump = false;
		cycles = 2;
		rw = true;
		discarded = false;
		addrBus = PC;
		dataBus = mem[addrBus];

		break;
	case 1:
		addrBus = PC + 1;
		dataBus = mem[addrBus];
		discarded = true;
		cycleAction = "";
		break;
	}
}

//Function for opcodes that perform operations using data contained in memory
//There's a *SLIGHT* difference with internal execution on memory data operations regarding to their real functionality
//The actual execution happens while the next opcode is fetched - i.e. on t0 of the next instruction. The cycle action info text reflects this,
//But the emulator performs the actual operation on the PREVIOUS cycle. Although the source operand is stored in the data bus, keeping track of which operation
//to perform and to which register would obfuscate the code too much and be too much of an effort compared to benefits gained
void cpu6502::MemOp()
{
	uint16_t tmp;
	switch (mode)
	{
	case IMM:
		switch (t)
		{
		case 0:
			jump = false;
			exec = true;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA";
			lastReadAddr = addrBus;
			break;
		}
		break;

	case ZERO:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH EFFECTIVE ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA FROM EFFECTIVE\nADDRESS ($" + Hex(effectiveAddr, 4) + ")";
			break;
		}
		break;

	case ABS:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			lastReadAddr = addrBus;
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8); //Combine low and high bytes to make the final 16-bit address
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			lastReadAddr = addrBus;
			break;
		case 3:
			addrBus = effectiveAddr;
			lastReadAddr = addrBus;
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA FROM EFFECTIVE\nADDRESS ($" + Hex(effectiveAddr, 4) + ")";
			break;
		}
		break;
		//The indirect address is fetched from the zero page. This means that if the address containing the final address is eg. 0xFF and the
		//contents of X is 2, indexing will wrap over and the final address will be fetched from 0x01, *not* from 0x101
	case INDX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus =  mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH BASE ADDRESS";
			lastReadAddr = addrBus;
			lastReadAddr = addrBus;
			break;
		case 2:
			addrBus = effectiveAddr;
			lastReadAddr = addrBus;
			dataBus = mem[addrBus];
			discarded = true;
			cycleAction = "";
			break;
		case 3:
			discarded = false;
			addrBus = (effectiveAddr + X) & 0xFF; //The address will stay in zero page
			lastReadAddr = addrBus;
			dataBus = mem[addrBus];
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 4:
			addrBus = (effectiveAddr + X + 1) & 0xFF;
			effectiveAddr = dataBus;
			dataBus = mem[addrBus];
			lastReadAddr = addrBus;
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			effectiveAddr |= (dataBus << 8);
			break;
		case 5:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			lastReadAddr = addrBus;
			break;
		}

		break;

	case ABSX:

		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			lastReadAddr = addrBus;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF BASE ADDRESS";
			lastReadAddr = addrBus;
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF BASE ADDRESS";
			lastReadAddr = addrBus;
			break;
		case 3:
			tmp = effectiveAddr; //Store the base address for page boundary comparison
			effectiveAddr += X; //Add X register value
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			if ((effectiveAddr & 0xFF00) != (tmp & 0xFF00)) //Check if page boundary was crossed - ie. adding the index results in an address that is on a different page than the base address
			{
				discarded = true;
				cycles++;
				cycleAction = "PAGE BOUNDARY CROSSED AFTER\nINDEXING";
			}
			else
			{
				cycleAction = "FETCH DATA FROM EFFECTIVE\nADDRESS ($" + Hex(effectiveAddr, 4) + ")";
			}
			break;
		case 4:
			discarded = false;
			cycleAction = "FETCH DATA FROM EFFECTIVE\nADDRESS ($" + Hex(effectiveAddr, 4) + ")";
			break;
		}
		break;

	case ABSY:

		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];
			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF BASE ADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF BASE ADDRESS";
			break;
		case 3:
			tmp = effectiveAddr; //Store the base address for page boundary comparison
			effectiveAddr += Y; //Add Y register value
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			if ((effectiveAddr & 0xFF00) != (tmp & 0xFF00)) //Check if page boundary was crossed - ie. adding the index results in an address that is on a different page than the base address
			{
				discarded = true;
				cycles++;
				cycleAction = "PAGE BOUNDARY CROSSED AFTER\nINDEXING";
			}
			else
			{
				cycleAction = "FETCH DATA FROM EFFECTIVE\nADDRESS ($" + Hex(effectiveAddr, 4) + ")";
			}
			break;
		case 4:
			discarded = false;
			cycleAction = "FETCH DATA FROM EFFECTIVE\nADDRESS ($" + Hex(effectiveAddr, 4) + ")";
			break;
		}
		break;

		//Indexed zero page addressing modes work similarly to indirect X and Y indexed modes - that is, the address will wrap over
	case ZEROX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			cycleAction = "FETCH BASE ZERO PAGE ADDRESS";
			break;
		case 2:
			discarded = true;
			addrBus = dataBus;
			dataBus = mem[addrBus];
			cycleAction = "";
			break;
		case 3:
			discarded = false;
			addrBus = (addrBus + X) & 0xFF; //Constrain the address to zero page
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA FROM $" + Hex(addrBus, 4);
			break;
		}

		break;

	case ZEROY:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			cycleAction = "FETCH BASE ZERO PAGE ADDRESS";
			break;
		case 2:
			discarded = true;
			addrBus = dataBus;
			dataBus = mem[addrBus];
			cycleAction = "";
			break;
		case 3:
			discarded = false;
			addrBus = (addrBus + Y) & 0xFF; //Constrain the address to zero page
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA FROM $" + Hex(addrBus, 4);
			break;
		}
		break;
	case INDY:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE INDIRECT ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH BASE LOW BYTE OF ADDRESS";
			break;
		case 3:
			addrBus = (addrBus + 1) & 0xFF; //Constrain to zero page
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH BASE HIGH BYTE OF ADDRESS";
			break;
		case 4:
			tmp = effectiveAddr; //Store effective address for page boundary crossing comparison
			effectiveAddr += Y;
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];

			if ((tmp & 0xFF00) != (effectiveAddr & 0xFF00))
			{
				cycleAction = "PAGE BOUNDARY CROSSED AFTER\nINDEXING";
				discarded = true;
				++cycles;
			}
			else
			{
				cycleAction = "FETCH DATA FROM $" + Hex(effectiveAddr, 4);
			}
			break;
		case 5:
			discarded = false;
			cycleAction = "FETCH DATA FROM $" + Hex(effectiveAddr, 4);
			break;
		}
		break;

	}


}

//Function for opcodes that store data into memory
void cpu6502::StoreOp()
{
	switch (mode)
	{
	case ZERO:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE EFFECTIVE ADDRESS";
			break;
		case 2:
			rw = false;
			addrBus = effectiveAddr;
			switch (opcode & 0b11100111) //Contents of data bus and the cycle action depend on which store instruction (STA,STX,STY) is being executed
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
			break;
		}
		break;

	case ABS:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus  = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 3:
			rw = false;
			addrBus = effectiveAddr;
			switch (opcode & 0b11100111)
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
			break;
		}
		break;

	case INDX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE BASE ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			discarded = true;
			cycleAction = "";
			break;
		case 3:
			discarded = false;
			addrBus = ((effectiveAddr + X) & 0xFF); // Indexing will wrap around
			dataBus = mem[addrBus]; 
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 4:
			++addrBus;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 5:
			rw = false;
			addrBus = effectiveAddr;
			mem[addrBus] = dataBus;
		}

		break;

	case ABSX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			effectiveAddr |= (dataBus << 8);
			break;
		case 3:
			effectiveAddr += X;
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			discarded = true;
			cycleAction = "";
			break;
		case 4:
			discarded = false;
			rw = false;
			addrBus = effectiveAddr;
			switch (opcode & 0b11100111)
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
		}
		break;

	case ABSY:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			effectiveAddr |= (dataBus << 8);
			break;
		case 3:
			effectiveAddr += Y;
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			cycleAction = "";
			discarded = true;
			break;
		case 4:
			discarded = false;
			rw = false;
			addrBus = effectiveAddr;
			switch (opcode & 0b11100111)
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
		}
		break;

	case ZEROX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:

			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE BASE ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			discarded = true;
			cycleAction = "";
			break;
		case 3:

			effectiveAddr += X;
			discarded = false;
			rw = false;
			addrBus = (effectiveAddr&0xFF);
			switch (opcode & 0b11100111)
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
			lastWriteAddr = addrBus;
		}
		break;

	case ZEROY:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:

			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE BASE ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			discarded = true;
			cycleAction = "";
			break;
		case 3:
			effectiveAddr += Y;
			discarded = false;
			rw = false;
			addrBus = (effectiveAddr&0xFF);
			switch (opcode & 0b11100111)
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
			lastWriteAddr = addrBus;
		}
		break;

	case INDY:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE INDIRECT ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF BASE ADDRESS";
			break;
		case 3:
			++addrBus;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF BASE ADDRESS";
			break;
		case 4:
			effectiveAddr += Y;
			addrBus = effectiveAddr;
			discarded = true;
			dataBus = mem[addrBus];
			cycleAction = "";
			break;
		case 5:
			rw = discarded = false;
			addrBus = effectiveAddr;
			switch (opcode & 0b11100111)
			{
			case eSTA:
				dataBus = A;
				cycleAction = "WRITE CONTENTS OF A REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTX:
				dataBus = X;
				cycleAction = "WRITE CONTENTS OF X REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			case eSTY:
				dataBus = Y;
				cycleAction = "WRITE CONTENTS OF Y REGISTER\nTO ADDRESS $" + Hex(addrBus, 4);
				break;
			}
			mem[addrBus] = dataBus;
			lastWriteAddr = addrBus;
		}
		break;

	}
}

//Function for Read-Modify-Write opcodes
//Modification of data takes place in each respective opcode function
//After the data has been read, the contents of the address bus and data bus remain the for the rest of the operation, save for the last cycle
//where data bus will contain the modified data. This will also be handled by the respective function along with the modification
void cpu6502::RMWOp()
{
	switch (mode)
	{
	case ZERO:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			cycleAction = "FETCH ZERO PAGE ADDRESS";
			break;
		case 2:
			addrBus = dataBus;
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA";
			break;

		case 3:
			rw = false;
			cycleAction = "";
			lastWriteAddr = addrBus;
			break;
		case 4:
			cycleAction = "WRITE MODIFIED DATA BACK\nINTO MEMORY";
			break;
		}
		break;

	case ABS:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH HIGH BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 3:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			cycleAction = "FETCH DATA";
			break;
		case 4:
			rw = false;
			cycleAction = "";
			lastWriteAddr = addrBus;
			break;
		case 5:
			cycleAction = "WRITE MODIFIED DATA BACK INTO MEMORY";
			break;
		}
		break;

	case ZEROX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH ZERO PAGE BASE ADDRESS";
			break;
		case 2:
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			discarded = true;
			break;
		case 3:
			effectiveAddr = (effectiveAddr + X) & 0xFF; //Constrain address to zero page
			discarded = false;
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			break;
		case 4:
			rw = false;
			cycleAction = "";
			lastWriteAddr = addrBus;
			break;
		case 5:
			cycleAction = "WRITE MODIFIED DATA BACK INTO MEMORY";
			break;
		}
		break;

	case ABSX:
		switch (t)
		{
		case 0:
			jump = false;
			rw = true;
			discarded = false;
			addrBus = PC;
			dataBus = mem[addrBus];

			break;
		case 1:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr = dataBus;
			cycleAction = "FETCH THE LOW BYTE OF EFFECTIVE\nADDRESS";
			break;
		case 2:
			addrBus = ++PC;
			dataBus = mem[addrBus];
			effectiveAddr |= (dataBus << 8);
			cycleAction = "FETCH THE HIGH BYTE OF EFFECTIVE\nADDRESS";
			break;
			//This one is a bit weird - The correct address is set in to the address bus, but the data is discarded and fetched again on the next cycle
			//The reason for this is unclear, although one plausible theory would be it being a compromise in design process
		case 3:
			effectiveAddr += X;
			addrBus = effectiveAddr;
			dataBus = mem[addrBus];
			discarded = true;
			cycleAction = "";
			break;
		case 4:
			discarded = false;
			cycleAction = "FETCH DATA";
			break;
		case 5:
			rw = false;
			cycleAction = "";
			lastWriteAddr = addrBus;
			break;
		case 6:
			cycleAction = "WRITE MODIFIED DATA BACK INTO MEMORY";
			break;
		}
		break;
	}
}

//Function for stack push opcodes
void cpu6502::PushOp()
{
	switch (t)
	{
	case 0:
		if (opcode == ePHA)
			opcodeAction = "PUSH A TO STACK";
		else if (opcode == ePHP)
			opcodeAction = "PUSH STATUS REGISTER TO STACK";
		jump = false;
		rw = true;
		discarded = false;
		addrBus = PC;
		dataBus = mem[addrBus];

		break;
	case 1:
		addrBus = PC + 1;
		dataBus = mem[addrBus];
		discarded = true;
		cycleAction = "";
		break;
	case 2:
		discarded = false;
		rw = false;
		addrBus = SP;
		if (opcode == ePHA)
		{
			dataBus = A;
			cycleAction = "WRITE CONTENTS OF REGISTER A\nTO STACK AT $" + Hex(SP, 4);
		}
		else if (opcode == ePHP)
		{
			SetFlag(B, 1);
			SetFlag(U, 1);
			dataBus = P;
			cycleAction = "WRITE CONTENTS OF STATUS REGISTER\nTO STACK AT $" + Hex(SP, 4);
		}
		mem[SP] = dataBus;
		SP = 0x100 | ((SP - 1) & 0xFF);
		lastWriteAddr = addrBus;
		break;
	}
}

//Function for stack pull opcodes
void cpu6502::PullOp()
{
	uint8_t tmp;
	switch (t)
	{
	case 0:
		if (opcode == ePLA)
			opcodeAction = "PULL A FROM STACK";
		else if (opcode == ePLP)
			opcodeAction = "PULL STATUS REGISTER FROM STACK";
		jump = false;
		rw = true;
		discarded = false;
		addrBus = PC;
		dataBus = mem[addrBus];

		break;
	case 1:
		addrBus = PC + 1;
		dataBus = mem[addrBus];
		cycleAction = "";
		discarded = true;
		break;
	case 2:
		addrBus = SP;
		dataBus = mem[SP];
		cycleAction = "";
		break;
	case 3:
		discarded = false;
		SP++;
		SP = 0x100 | (SP & 0xFF);
		addrBus = SP;
		dataBus = mem[SP];
		if (opcode == ePLA)
		{
			A = dataBus;
			SetFlag(N, A >= 0x80);
			SetFlag(Z, A == 0);

			cycleAction = "FETCH CONTENTS OF REGISTER A\nFROM STACK AT $" + Hex(SP, 4);
		}
		else if (opcode == ePLP)
		{
			tmp = P; //Store current status register
			P = dataBus;

			cycleAction = "FETCH CONTENTS OF STATUS REGISTER\nFROM STACK AT $" + Hex(SP, 4);
			SetFlag(U, true);
			SetFlag(B,(tmp & B));//B flag is not affected by the pull, use the status before the pull
		}
		break;

	}
}

//For triggering the interrupts
void cpu6502::TriggerInterrupt(uint8_t i)
{
	if(!reset)//Don't trigger interrupt during reset sequence
	switch (i)
	{
	case IRQ:
		if (!(P & I))
		{
			irq = true;
			brk = false;
			nmi = false;
			reset = false;
		}	
		break;
	case NMI:
		nmi = true;
		irq = false;
		brk = false;
		reset = false;
		break;
	}
}

//Sequence for branching operations
void cpu6502::BranchOp()
{
	uint8_t tmp;
	switch (t)
	{
	case 0:
		jump = false; //Branch won't necessarily take place, so on next t0 we will increase PC in the clock function to fetch the next opcode
		rw = true;
		discarded = false;
		addrBus = PC;
		dataBus = mem[addrBus];
		mode = REL;
		addrMode = "RELATIVE";
		cycles = 2;
		break;
	case 1:
		addrBus = ++PC;
		dataBus = mem[addrBus];
		offset = dataBus;
		cycleAction = "FETCH BRANCH OFFSET";
		//Determining whether or not branch is taken happens on the corresponding branch function
		if (branch)
		{
			cycles++;
			cycleAction += " - BRANCH TAKEN";
		}
		else
			cycleAction += " - BRANCH\nNOT TAKEN";
		break;
	case 2:
		
		
		if (dataBus >= 0x80) //if the offset is negative, perform two's complement and subtract it
		{
			tmp = ((~offset) + 1); //calculate the two's complement to a separate variable - for some reason straight calculation won't work properly
			addrBus = (PC + 1) -tmp;
		}

		else
			addrBus = PC + 1 + offset;
		dataBus = mem[addrBus];

		if ((addrBus & 0xFF00) != ((PC + 1) & 0xFF00))
		{
			cycles++;
			cycleAction = "PAGE BOUNDARY CROSSED";
		}
		else
		{
			PC = addrBus;
			jump = true;
			cycleAction = "ADD OFFSET TO PC";
		}
		break;
	case 3:
		PC = addrBus;
		jump = true;
		cycleAction = "ADD OFFSET TO PC";
		break;
	}
}

//Handles all variants of both interrupts and resets
void cpu6502::InterruptOP()
{
	switch (t)
	{
	case 0:
		if (reset)
			opcodeAction = "RESET SEQUENCE";
		else if (irq)
			opcodeAction = "HARDWARE INTERRUPT";
		else if (brk)
			opcodeAction = "SOFTWARE INTERRUPT";
		else if (nmi)
			opcodeAction = "NON-MASKABLE INTERRUPT";
		addrMode = "";
		cycleAction = "";
		cycles = 7;
		jump = true;
		rw = true;
		discarded = false;
		addrBus = PC;
		dataBus = mem[addrBus];
		break;

	case 1:
		if (brk)
			PC++; //As BRK will return to PC+2 instead of PC+1, an additional increment of PC is needed 
		addrBus = PC;
		dataBus = mem[addrBus];
		cycleAction = "";
		discarded = true;
		break;
	case 2:
		discarded = false;
		addrBus = SP;
		dataBus = (PC & 0xFF00) >> 8;		
		if (reset)
		{
			rw = true;
			discarded = true;
		}
		else
		{
			rw = false;
			cycleAction = "PUSH HIGH BYTE OF PC TO STACK";
		}
		mem[addrBus] = dataBus;
		lastWriteAddr = addrBus;
		SP--;
		break;
	case 3:
		addrBus = SP;
		dataBus = PC & 0xFF;
		mem[addrBus] = dataBus;
		lastWriteAddr = addrBus;
		if (reset)
		{
			rw = true;
			discarded = true;
		}
		else
		{
			rw = false;
			cycleAction = "PUSH LOW BYTE OF PC TO STACK";
		}
		SP--;
		break;
	case 4:
		addrBus = SP;
		//Reset the B flag, if interrupted via IRQ or NMI
		if (irq || nmi)
			SetFlag(B, 0);
	
		dataBus = P;
		//B flag is zero only on stack if executed via IRQ or NMI
		SetFlag(B, 1);
		mem[addrBus] = dataBus;
		lastWriteAddr = addrBus;
		if (reset)
		{
			rw = true;
			discarded = true;
		}
		else
		{
			rw = false;
			cycleAction = "PUSH STATUS REGISTER TO STACK";
			cycleAction+=brk ? "" : "\nWITH B CLEARED";
		}
		SP--;
		break;
	case 5:
		rw = true;
		if (reset)
		{
			cycleAction = "FETCH LOW BYTE OF RESET VECTOR";
			addrBus = 0xFFFC;
		}
		else if (irq || brk)
		{
			cycleAction = "FETCH LOW BYTE OF INTERRUPT\nVECTOR";
			addrBus = 0xFFFE;
		}
		else if (nmi)
		{
			cycleAction = "FETCH LOW BYTE OF INTERRUPT\VECTOR";
			addrBus = 0xFFFA;
		}
		dataBus = mem[addrBus];
		effectiveAddr = dataBus;
		break;
	case 6:
		if (reset)
		{
			cycleAction = "FETCH HIGH BYTE OF RESET VECTOR";
			addrBus =0xFFFD;
		}
		else if (irq || brk)
		{
			cycleAction = "FETCH HIGH BYTE OF INTERRUPT\nVECTOR";
			addrBus =0xFFFF;
		}
		else if (nmi)
		{
			cycleAction = "FETCH HIGH BYTE OF INTERRUPT\nVECTOR";
			addrBus =0xFFFB;
		}
		dataBus = mem[addrBus];
		effectiveAddr |= (dataBus << 8);
		PC = effectiveAddr;
		SetFlag(I, 1);

		
		//Clear helper flags
		reset = false;
		irq = false;
		nmi = false;
		brk = false;

		break;
	}

}





