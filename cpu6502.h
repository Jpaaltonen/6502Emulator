#pragma once
#include <iostream>
#include <vector>

class cpu6502
{
public:
	//Constructor and destructor
	cpu6502(bool core);
	~cpu6502();


	uint16_t PC, SP; //Program counter and the stack pointer
	uint8_t A, X, Y, P; //A,X,Y and status registers
	uint16_t addrBus; //Contents of the adress bus
	uint8_t dataBus; //Contents of the data bus
	int lastReadAddr, lastWriteAddr; //Last R/W accesses for visualization purposes

	//Vector for memory - Using a static array would result in a warning regarding a large stack, and if we have to use dynamic allocation anyway, why not choose vector class?
	std::vector<uint16_t> mem;
	uint8_t tick; //Clock tick - On/Off signal, half-cycle
	uint8_t cycles; //OPcode cycles 
	uint8_t t; //Tn for opcde cycle action - This is a workaround solution to cycle count changing eg. because of branching or crossing page boundaries
	uint32_t totalCycles; //Total cycles since reset
	std::vector<std::string> code; //Vector for disassembled instruction to be shown
	std::string opcodeAction; //String to hold the explanation of the opcode currently executing
	std::string cycleAction; //String to hold the explanation of what happens on each cycle
	std::string addrMode; //String to hold the addressing mode of the opcode
	void LoadProgram(std::vector<char> prg, uint16_t start); //Mainly for the Dormann's test
	void LoadProgram(std::vector<char> prg); //For loading custom test programs
	bool rw = false; //Status of R/W pin
	bool discarded = false; //Status of the data present on data bus at each cycle - some of it is discarded
	bool reset = false; //Whether or not a reset sequence is being executed
	bool runInt = false; //Set when the hardware interrupt/reset sequence has to be run - overrides the execution of the next opcode in line
	
	void Clock();

	void Reset(bool core);

	void TriggerInterrupt(uint8_t i); //Trigger the interrupt

private:

	//Status register flags - Using and 8 bit integer with enumerations allows us to set or clear the flags and
	//draw the visualization more conveniently compared to a struct with bitfields, for example
	enum
	{
		C = 1 << 0,
		Z = 1 << 1,
		I = 1 << 2,
		D = 1 << 3,
		B = 1 << 4,
		U = 1 << 5,
		V = 1 << 6,
		N = 1 << 7,
	};

	//Distinguish between IRQ and NMI
	enum
	{
		IRQ=0,
		NMI=1
	};

	//Enumerations for the base format of each instruction - Not all of them are used in the version, but as they don't do any harm, why leave them out?
	enum
	{
		eADC = 0b01100001,
		eAND = 0b00100001,
		eASL = 0b00000010,
		eBCC = 0b10010000,
		eBCS = 0b10110000,
		eBEQ = 0b11110000,
		eBIT = 0b00100100,
		eBMI = 0b00110000,
		eBNE = 0b11010000,
		eBPL = 0b00010000,
		eBRK = 0b00000000,
		eBVC = 0b01010000,
		eBVS = 0b01110000,
		eCLC = 0b00011000,
		eCLD = 0b11011000,
		eCLI = 0b01011000,
		eCLV = 0b10111000,
		eCMP = 0b11000001,
		eCPX = 0b11100000,
		eCPY = 0b11000000,
		eDEC = 0b11000110,
		eDEX = 0b11001010,
		eDEY = 0b10001000,
		eEOR = 0b01000001,
		eINC = 0b11100110,
		eINX = 0b11101000,
		eINY = 0b11001000,
		eJMP = 0b01001100,
		eJSR = 0b00100000,
		eLDA = 0b10100001,
		eLDX = 0b10100010,
		eLDY = 0b10100000,
		eLSR = 0b01000010,
		eNOP = 0b11101010,
		eORA = 0b00000001,
		ePHA = 0b01001000,
		ePHP = 0b00001000,
		ePLA = 0b01101000,
		ePLP = 0b00101000,
		eROL = 0b00100010,
		eROR = 0b01100010,
		eRTI = 0b01000000,
		eRTS = 0b01100000,
		eSBC = 0b11100001,
		eSEC = 0b00111000,
		eSED = 0b11111000,
		eSEI = 0b01111000,
		eSTA = 0b10000001,
		eSTX = 0b10000110,
		eSTY = 0b10000100,
		eTAX = 0b10101010,
		eTAY = 0b10101000,
		eTSX = 0b10111010,
		eTXA = 0b10001010,
		eTXS = 0b10011010,
		eTYA = 0b10011000
	};

	//Addressing mode masks
	enum
	{
		Group1 = 0b00011100,
		Group2A = 0b00011000,
		Group2B = 0b00001100,
		Group3A = 0b00001000,
		Group3B = 0b00100000
	};

	//Addressing modes
	enum
	{
		ACC =  0,
		IMM =  1,
		IMP =  2,
		ZERO =  3,
		ZEROX =  4,
		ZEROY =  5,
		ABS =  6,
		ABSX =  7,
		ABSY =  8,
		IND =  9,
		INDX =  10,
		INDY =  11,
		REL =  12,
		XXX =  255
	};





	std::string instruction; //String to hold the mnemonic of the instruction
	uint8_t relativeAddr; //Relative address for branching
	uint16_t effectiveAddr; //Effective address for fetching data
	uint8_t offset; //Branching offset
	int opcode; //OPCode to be executed
	int mode; //The address mode of the opcode
	bool jump = false; //Keep track whether or not executing a branch or jump instruction
	bool branch = false; //Determine whether or not branch takes place
	
	bool irq = false; //Flag for hardware interrupt request
	bool brk = false; //Flag for software interrupt request
	bool nmi = false; //flag for non-maskable interrupt
	bool exec = false; //As the  execution of a certain group of instruction happens while fetching the next opcode, we'll need a flag to denote the correct cycle action on t0
	 

	void SetFlag(uint8_t flag, bool val); //Helper function to set the flags

	void DisAsm(); //Disassemble a part of the code

	//Declarations for opcode-functions
	void ADC();
	void AND();
	void ASL();
	void BCC();
	void BCS();
	void BEQ();
	void BIT();
	void BMI();
	void BNE();
	void BPL();
	void BRK();
	void BVC();
	void BVS();
	void CLC();
	void CLD();
	void CLI();
	void CLV();
	void CMP();
	void CPX();
	void CPY();
	void DEC();
	void DEX();
	void DEY();
	void EOR();
	void INC();
	void INX();
	void INY();
	void JMP();
	void JSR();
	void LDA();
	void LDX();
	void LDY();
	void LSR();
	void NOP();
	void ORA();
	void PHA();
	void PHP();
	void PLA();
	void PLP();
	void ROL();
	void ROR();
	void RTI();
	void RTS();
	void SBC();
	void SEC();
	void SED();
	void SEI();
	void STA();
	void STX();
	void STY();
	void TAX();
	void TAY();
	void TSX();
	void TXA();
	void TXS();
	void TYA();
	void ILL();

	//An array of function pointers to opcode functions
	void (cpu6502::* inst[256])(void) =
	{
		//			0				1				2				3				4			5				6				7			 8			 	9			   A				B			 C				D				E				F
				&cpu6502::BRK, &cpu6502::ORA, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ORA, &cpu6502::ASL, &cpu6502::ILL, &cpu6502::PHP, &cpu6502::ORA, &cpu6502::ASL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ORA, &cpu6502::ASL, &cpu6502::ILL,
				&cpu6502::BPL, &cpu6502::ORA, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ORA, &cpu6502::ASL, &cpu6502::ILL, &cpu6502::CLC, &cpu6502::ORA, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ORA, &cpu6502::ASL, &cpu6502::ILL,
				&cpu6502::JSR, &cpu6502::AND, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::BIT, &cpu6502::AND, &cpu6502::ROL, &cpu6502::ILL, &cpu6502::PLP, &cpu6502::AND, &cpu6502::ROL, &cpu6502::ILL, &cpu6502::BIT, &cpu6502::AND, &cpu6502::ROL, &cpu6502::ILL,
				&cpu6502::BMI, &cpu6502::AND, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::AND, &cpu6502::ROL, &cpu6502::ILL, &cpu6502::SEC, &cpu6502::AND, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::AND, &cpu6502::ROL, &cpu6502::ILL,
				&cpu6502::RTI, &cpu6502::EOR, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::EOR, &cpu6502::LSR, &cpu6502::ILL, &cpu6502::PHA, &cpu6502::EOR, &cpu6502::LSR, &cpu6502::ILL, &cpu6502::JMP, &cpu6502::EOR, &cpu6502::LSR, &cpu6502::ILL,
				&cpu6502::BVC, &cpu6502::EOR, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::EOR, &cpu6502::LSR, &cpu6502::ILL, &cpu6502::CLI, &cpu6502::EOR, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::EOR, &cpu6502::LSR, &cpu6502::ILL,
				&cpu6502::RTS, &cpu6502::ADC, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ADC, &cpu6502::ROR, &cpu6502::ILL, &cpu6502::PLA, &cpu6502::ADC, &cpu6502::ROR, &cpu6502::ILL, &cpu6502::JMP, &cpu6502::ADC, &cpu6502::ROR, &cpu6502::ILL,
				&cpu6502::BVS, &cpu6502::ADC, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ADC, &cpu6502::ROR, &cpu6502::ILL, &cpu6502::SEI, &cpu6502::ADC, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ADC, &cpu6502::ROR, &cpu6502::ILL,
				&cpu6502::ILL, &cpu6502::STA, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::STY, &cpu6502::STA, &cpu6502::STX, &cpu6502::ILL, &cpu6502::DEY, &cpu6502::ILL, &cpu6502::TXA, &cpu6502::ILL, &cpu6502::STY, &cpu6502::STA, &cpu6502::STX, &cpu6502::ILL,
				&cpu6502::BCC, &cpu6502::STA, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::STY, &cpu6502::STA, &cpu6502::STX, &cpu6502::ILL, &cpu6502::TYA, &cpu6502::STA, &cpu6502::TXS, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::STA, &cpu6502::ILL, &cpu6502::ILL,
				&cpu6502::LDY, &cpu6502::LDA, &cpu6502::LDX, &cpu6502::ILL, &cpu6502::LDY, &cpu6502::LDA, &cpu6502::LDX, &cpu6502::ILL, &cpu6502::TAY, &cpu6502::LDA, &cpu6502::TAX, &cpu6502::ILL, &cpu6502::LDY, &cpu6502::LDA, &cpu6502::LDX, &cpu6502::ILL,
				&cpu6502::BCS, &cpu6502::LDA, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::LDY, &cpu6502::LDA, &cpu6502::LDX, &cpu6502::ILL, &cpu6502::CLV, &cpu6502::LDA, &cpu6502::TSX, &cpu6502::ILL, &cpu6502::LDY, &cpu6502::LDA, &cpu6502::LDX, &cpu6502::ILL,
				&cpu6502::CPY, &cpu6502::CMP, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::CPY, &cpu6502::CMP, &cpu6502::DEC, &cpu6502::ILL, &cpu6502::INY, &cpu6502::CMP, &cpu6502::DEX, &cpu6502::ILL, &cpu6502::CPY, &cpu6502::CMP, &cpu6502::DEC, &cpu6502::ILL,
				&cpu6502::BNE, &cpu6502::CMP, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::CMP, &cpu6502::DEC, &cpu6502::ILL, &cpu6502::CLD, &cpu6502::CMP, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::CMP, &cpu6502::DEC, &cpu6502::ILL,
				&cpu6502::CPX, &cpu6502::SBC, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::CPX, &cpu6502::SBC, &cpu6502::INC, &cpu6502::ILL, &cpu6502::INX, &cpu6502::SBC, &cpu6502::NOP, &cpu6502::ILL, &cpu6502::CPX, &cpu6502::SBC, &cpu6502::INC, &cpu6502::ILL,
				&cpu6502::BEQ, &cpu6502::SBC, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::SBC, &cpu6502::INC, &cpu6502::ILL, &cpu6502::SED, &cpu6502::SBC, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::ILL, &cpu6502::SBC, &cpu6502::INC, &cpu6502::ILL,

	};


	//Function for determining the address mode
	void GetAddrMode(int mask);

	//Functions for single cycle execution
	void ByteOp();
	void MemOp();
	void StoreOp();
	void RMWOp();
	void PushOp();
	void PullOp();
	void BranchOp();

	//This will handle software and hardware interrupts, as well as cold and warm resets
	void InterruptOP();
};