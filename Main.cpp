
#define OLC_PGE_APPLICATION

#include <OLC/olcPixelGameEngine.h>
#include "cpu6502.h"
#include <string>
#include <fstream>
#include <conio.h>
#include <chrono>

#define MAX_SPEED 8


//Stopwatch class to time the tests
class watch
{
public:
	std::chrono::time_point<std::chrono::steady_clock> startTime, endTime;

	void start()
	{
		startTime = std::chrono::steady_clock::now();
	}

	double stop()
	{
		endTime = std::chrono::steady_clock::now();
		std::chrono::duration<double> d = endTime - startTime;
		return d.count();
	}
};


class Monitor6502 : public olc::PixelGameEngine
{
public:
	cpu6502* cpu; //Pointer to the instance of the cpu

	uint8_t userPage = 0x02; //Set the user defined page to 0x0200 as a default
	int UI_line, UI_leftColumn = 20, UI_rightColumn = 500, UI_top = 2;
	int UI_lineHeight = 12, UI_charWidth = 10;
	bool showBus = true; //Show data and address bus only if the user wants to see them
	bool stepMode = true; //Switch between continuous and step mode
	bool follow = true; //Follow the PC in memory - ie. user defined page is always the one where PC is at the moment
	bool console = false; //Helper flag to disable the keyboard commands while using the console
	int delayCnt; //Delay variables for timing the delay of continuous mode
	float delay, t;
	olc::Sprite skin, skin2;

	//Differentiate between IRQ and NMI
	enum
	{
		IRQ = 0,
		NMI = 1
	};

	Monitor6502(cpu6502* c)
	{
		//Set the pointer of cpu to the one created in main
		cpu = c;
		sAppName = "6502 emulator";
	}


	void loadBin(std::string name)
	{
		char* block;
		std::streampos size;
		std::ifstream file;
		file.open(name, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = file.tellg();
			block = new char[size];
			file.seekg(0, std::ios::beg);
			file.read(block, size);
			file.close();
			std::vector<char> prg(block, block + size);
			cpu->LoadProgram(prg);
			delete[] block;
			std::cout << "Loaded file " << name << "\n";
		}
		else
			std::cout << "could not load file\n";
	}


	bool OnConsoleCommand(const std::string& s) override
	{
		std::stringstream stream;
		stream << s;
		std::string cmd;
		stream >> cmd;
		if (cmd == "load")
		{
			std::string file;
			stream >> file;
			loadBin(file);
			if (file == "test.bin")
			{
				cpu->Reset(true); //Reset as you would with the core test
				cpu->PC = 0x400;
			}
			else
				cpu->Reset(false);

		}
		else if (cmd == "jump")
		{
			std::string hex;
			stream >> hex;
			if (hex[0] == '$')
				hex.erase(0, 1);
			uint16_t loc = stoi(hex, 0, 16);
			uint16_t tmp = cpu->PC;
			cpu->PC = loc;
			std::cout << "PC moved to $" << Hex(loc, 4) << " from $" << Hex(tmp, 4) << "\n";
		}
		else if (cmd == "cls")
			ConsoleClear();
		else
			std::cout << "Unknown command \"" << cmd << "\"\n";
		return true;
	}



	bool OnUserCreate() override
	{
		olc::rcode SetWindoWTitle();
		skin.LoadFromFile("Skin3.png");

		loadBin("Demo.bin");
		delayCnt = 2;
		delay = 0.5 / (delayCnt + 1), t = 0; //Set a delay for continuous mode

		//As console window is not shown on the background, print the messages on OLC console - Only if it's open, but nothing gets printed to std out, if it's not
		ConsoleCaptureStdOut(true);
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{

		Clear(olc::BLACK);
		if (follow)
			userPage = (cpu->PC & 0xFF00) >> 8;
		Clear(olc::BLACK);
		DrawSprite(0, 0, &skin);
		UI_line = UI_top;
		DrawString(UI_leftColumn, UI_line * UI_lineHeight, "MEMORY CONTENTS", olc::DARK_YELLOW);
		UI_line++;
		DumpMem(0x00, "ZERO PAGE ($0000-$00FF):");
		UI_line += 2;
		DumpMem(0x01, "STACK ($0100-$01FF):");
		UI_line += 2;
		DumpMem(userPage, "USER DEFINED ($" + Hex(userPage << 8, 4) + "-$" + Hex((userPage << 8) + 0xFF, 4) + "):");
		UI_line = UI_top;
		DrawClock();
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "REGISTERS", olc::DARK_YELLOW);
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "     ADDRESS      DATA", olc::DARK_YELLOW);
		UI_line++;
		DumpReg("A");
		DumpReg("X");
		DumpReg("Y");
		DumpReg("SP");
		DumpReg("PC");
		DrawStatus();
		UI_line += 3;
		int y = UI_line * UI_lineHeight;
		int x = UI_rightColumn;
		DrawString(x, y, "R/W:", olc::DARK_YELLOW);
		UI_line++;
		y = UI_line * UI_lineHeight;
		if (showBus)
			if (cpu->rw)
			{
				FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::GREEN);
			}
			else
			{
				FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::RED);
			}
		else
			FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::VERY_DARK_RED);
		UI_line++;
		y = UI_line * UI_lineHeight;
		DrawString(x, y + 1, "DATA BUS:", olc::DARK_YELLOW);
		UI_line++;
		DrawBus(cpu->dataBus);
		UI_line++;
		y = UI_line * UI_lineHeight;
		DrawString(x, y + 1, "ADDRESS BUS:", olc::DARK_YELLOW);
		UI_line++;
		DrawBus(cpu->addrBus);

		UI_line += 3;
		DumpCode();
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "CURRENT OPCODE ADDRESSING MODE:", olc::DARK_YELLOW);
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, cpu->addrMode);
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "OPCODE ACTION:", olc::DARK_YELLOW);
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, cpu->opcodeAction);
		UI_line += 2;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "CYCLE ACTION:", olc::DARK_YELLOW);
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, cpu->cycleAction);
		UI_line += 5;
		Info();





		//Keyboard controls
		if (GetKey(olc::S).bPressed && !console)
		{
			showBus = !showBus;
		}

		if (GetKey(olc::I).bPressed && !console)
		{
			cpu->TriggerInterrupt(IRQ);
		}

		if (GetKey(olc::N).bPressed && !console)
		{
			cpu->TriggerInterrupt(NMI);
		}

		if (GetKey(olc::W).bPressed && !console)
		{
			cpu->Reset(false);
			userPage = cpu->mem[0xFFFD]; //Fetch the page from the upper byte of reset vector			
		}

		if (GetKey(olc::C).bPressed && !console)
		{
			stepMode = !stepMode;
		}

		if (GetKey(olc::F).bPressed && !console)
		{
			follow = !follow; //Flip follow flag
		}

		if (GetKey(olc::F1).bPressed && delayCnt < MAX_SPEED)
		{
			delay /= 2;
			delayCnt++;
		}

		if (GetKey(olc::F2).bPressed && delayCnt > 0)
		{
			delay *= 2;
			delayCnt--;
		}

		if (GetKey(olc::ESCAPE).bPressed)
		{
			if (!console)
				ConsoleShow(olc::Key::ESCAPE);
			console = !console;
		}


		//Increase user defined page number by one
		if (GetKey(olc::RIGHT).bPressed && !console)
		{
			userPage = (userPage + 0x1) & 0xFF;
		}

		//Decrease user defined page number by one
		if (GetKey(olc::LEFT).bPressed && !console)
		{
			userPage = (userPage - 0x1) & 0xFF;

		}

		//Increase user defined page number by 0x10
		if (GetKey(olc::UP).bPressed && !console)
		{
			userPage = (userPage + 0x10) & 0xFF;
		}

		//Decrease user defined page number by 0x10
		if (GetKey(olc::DOWN).bPressed && !console)
		{
			userPage = (userPage - 0x10) & 0xFF;

		}

		if (stepMode)
		{
			if (GetKey(olc::SPACE).bPressed && !console)
				cpu->Clock();
		}
		else
		{
			t += fElapsedTime; //Advance t with the time it took between frames
			if (t > delay)
			{
				//If delay limit was passed, advance the clock and reset the timer
				cpu->Clock();
				t = 0;
			}
		}
		return true;
	}

	void DrawStatus()
	{
		int y = UI_line * UI_lineHeight;
		int x = UI_rightColumn;
		std::string str = "", tmp = "NVXBDIZC";

		for (int i = 0; i < tmp.size(); i++)
		{
			str = "";
			str += "NVXBDIZC"[i];
			DrawString(x, y, str, olc::DARK_YELLOW);
			x += UI_charWidth;
		}
		UI_line++; y = UI_line * UI_lineHeight;

		int chrCnt = 0;
		for (int i = 7; i >= 0; i--, chrCnt++)
		{
			x = UI_rightColumn + UI_charWidth * chrCnt;
			if (cpu->P & (1 << i))
			{
				FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::GREEN);
			}
			else
			{
				FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::RED);
			}
		}

	}

	template <typename T>
	void DrawBus(T n)
	{
		int y = UI_line * UI_lineHeight;
		int x = UI_rightColumn;
		float chrCnt = 0;
		for (int i = sizeof(n) * 8 - 1; i >= 0; i--, chrCnt++)
		{
			//When processing the address bus, add one empty character in the middle to separate it into two 8-bit displays
			if (i == 7 && sizeof(n) == 2)
				chrCnt++;
			x = UI_rightColumn + UI_charWidth * chrCnt;

			if (showBus)
				if (n & (1 << i))
				{
					FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::GREEN);
				}
				else
				{
					FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::RED);
				}
			else
				FillRect(x, y, UI_charWidth - 2, UI_charWidth - 2, olc::VERY_DARK_RED);
		}
		if (showBus)
		{
			//Print the contents of each bus beside the displayed bits
			chrCnt += 2;
			x = UI_rightColumn + UI_charWidth * chrCnt;
			if (sizeof(n) == 2)
				DrawString(x, y, "$" + Hex(cpu->addrBus, 4));
			else //If drawing data bus, also show whether or not the data is discarded
			{
				DrawString(x, y, "$" + Hex(cpu->dataBus, 2));
				chrCnt += 4;
				x = UI_rightColumn + UI_charWidth * chrCnt;
				if (cpu->discarded)
				{
					FillRect(x, y, UI_charWidth - 1, UI_charWidth - 2, olc::GREEN);
				}
				else
				{
					FillRect(x, y, UI_charWidth - 1, UI_charWidth - 2, olc::RED);
				}
				chrCnt += 1.5;
				x = UI_rightColumn + UI_charWidth * chrCnt;
				DrawString(x, y, "DATA DISCARDED", olc::DARK_YELLOW);
			}
		}

	}

	void DrawClock()
	{
		if (cpu->tick % 2 == 1)
			FillCircle(749, 51, 15, olc::GREEN);
		else
			FillCircle(749, 51, 15, olc::RED);
	}

	void DumpCode()
	{
		int y = UI_line * UI_lineHeight;
		int x = UI_rightColumn;
		DrawString(x, y, "CODE:", olc::DARK_YELLOW);
		UI_line++;
		y = UI_line * UI_lineHeight;
		DrawString(x, y, cpu->code[0]);
		UI_line++;
		for (int i = 1; i < cpu->code.size(); i++)
		{
			y = UI_line * UI_lineHeight;
			DrawString(x, y, cpu->code[i], olc::DARK_GREY);
			UI_line++;
		}
	}

	void DumpReg(std::string reg)
	{
		uint8_t tmp; //For two's complement conversion
		std::string dec = ""; //Decimal representation string 
		if (reg == "A")
		{
			tmp = cpu->A;
			if (tmp >= 0x80)
			{
				tmp = (~(cpu->A) + 1);
				dec += "  (-";
			}
			else
				dec = "  (";
			dec += std::to_string(tmp) + ")";
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "A:", olc::DARK_YELLOW);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "       ---      $" + Hex(cpu->A, 2) + dec);
			UI_line++;
		}
		if (reg == "X")
		{
			tmp = cpu->X;
			if (tmp >= 0x80)
			{
				tmp = (~(cpu->X) + 1);
				dec += "  (-";
			}
			else
				dec = "  (";
			dec += std::to_string(tmp) + ")";
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "X:", olc::DARK_YELLOW);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "       ---      $" + Hex(cpu->X, 2) + dec);
			UI_line++;
		}
		if (reg == "Y")
		{
			tmp = cpu->Y;
			if (tmp >= 0x80)
			{
				tmp = (~(cpu->Y) + 1);
				dec += "  (-";
			}
			else
				dec = "  (";
			dec += std::to_string(tmp) + ")";
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "Y:", olc::DARK_YELLOW);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "       ---      $" + Hex(cpu->Y, 2) + dec);
			UI_line++;
		}
		if (reg == "SP")
		{
			tmp = cpu->mem[cpu->SP];
			if (tmp >= 0x80)
			{
				tmp = (~(cpu->mem[cpu->SP]) + 1);
				dec += "  (-";
			}
			else
				dec = "  (";
			dec += std::to_string(tmp) + ")";

			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "SP:", olc::DARK_YELLOW);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "      $" + Hex(cpu->SP, 4), olc::GREEN);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "                $" + Hex(cpu->mem[cpu->SP], 2) + dec);
			UI_line++;
		}
		if (reg == "PC")
		{

			tmp = cpu->mem[cpu->PC];
			if (tmp > 0x80)
			{
				tmp = (~(cpu->mem[cpu->PC]) + 1);
				dec += "  (-";
			}
			else
				dec = "  (";
			dec += std::to_string(tmp) + ")";
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "PC:", olc::DARK_YELLOW);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "      $" + Hex(cpu->PC, 4), olc::RED);
			DrawString(UI_rightColumn, UI_line * UI_lineHeight, "                $" + Hex(cpu->mem[cpu->PC], 2) + dec);
			UI_line++;
		}

	}

	void DumpMem(uint8_t page, std::string header)
	{
		DrawString(UI_leftColumn, UI_line * UI_lineHeight, header, olc::DARK_YELLOW);
		int charCnt = 0, offset = 0;
		for (int i = page << 8; i <= (page << 8) + 0xFF; i++)
		{

			if (i % 16 == 0)
			{
				UI_line++;
				charCnt = 0;
				DrawString(UI_leftColumn + charCnt * UI_charWidth, UI_line * UI_lineHeight, "$" + Hex((page << 8) | offset, 4) + ": ");
				charCnt += 8;
			}
			if (i % 4 == 0)
				charCnt++;
			if (i == cpu->PC) //If memory location matches that of the PC,SP or last read/write address, change color accordingly
				DrawString(UI_leftColumn + charCnt * UI_charWidth, UI_line * UI_lineHeight, Hex(cpu->mem[i], 2), olc::RED);
			else if (i == cpu->SP)
				DrawString(UI_leftColumn + charCnt * UI_charWidth, UI_line * UI_lineHeight, Hex(cpu->mem[i], 2), olc::GREEN);
			else if (i == cpu->lastReadAddr)
				DrawString(UI_leftColumn + charCnt * UI_charWidth, UI_line * UI_lineHeight, Hex(cpu->mem[i], 2), olc::YELLOW);
			else if (i == cpu->lastWriteAddr)
				DrawString(UI_leftColumn + charCnt * UI_charWidth, UI_line * UI_lineHeight, Hex(cpu->mem[i], 2), olc::BLUE);
			else
				DrawString(UI_leftColumn + charCnt * UI_charWidth, UI_line * UI_lineHeight, Hex(cpu->mem[i], 2));
			charCnt += 2;
			offset++;

		}
	}

	void Info()
	{
		int offset = 14;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "ADDITIONAL INFORMATION", olc::DARK_YELLOW);
		UI_line++;

		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "  OPCODE CYCLES:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, std::to_string(cpu->t) + "/" + std::to_string(cpu->cycles));
		UI_line++;

		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "   TOTAL CYCLES:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, std::to_string(cpu->totalCycles));
		UI_line++;

		DrawString(UI_rightColumn, UI_line * UI_lineHeight, " EXECUTION MODE:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, (stepMode ? "STEP" : "CONTINUOUS"));
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "      FOLLOW PC:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, follow ? "YES" : "NO");
		UI_line++;

		DrawString(UI_rightColumn, UI_line * UI_lineHeight, " LAST READ ADDR:", olc::DARK_YELLOW);
		if (cpu->lastReadAddr > -1)
			DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "$" + Hex(cpu->lastReadAddr, 4), olc::YELLOW);
		else
			DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "N/A", olc::YELLOW);
		UI_line++;

		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "LAST WRITE ADDR:", olc::DARK_YELLOW);
		if (cpu->lastWriteAddr > -1)
			DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "$" + Hex(cpu->lastWriteAddr, 4), olc::BLUE);
		else
			DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "N/A", olc::BLUE);

		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, " IRQ/BRK VECTOR:", olc::DARK_YELLOW);

		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "$" + Hex(cpu->mem[0xFFFF], 2) + Hex(cpu->mem[0xFFFE], 2));

		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "   RESET VECTOR:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "$" + Hex(cpu->mem[0xFFFD], 2) + Hex(cpu->mem[0xFFFC], 2));
		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "     NMI VECTOR:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, "$" + Hex(cpu->mem[0xFFFB], 2) + Hex(cpu->mem[0xFFFA], 2));

		UI_line++;
		DrawString(UI_rightColumn, UI_line * UI_lineHeight, "EXECUTION SPEED:", olc::DARK_YELLOW);
		DrawString(UI_rightColumn + offset * UI_charWidth, UI_line * UI_lineHeight, (stepMode ? "N/A" : std::to_string(delayCnt) + "/" + std::to_string(MAX_SPEED)));

	}

	//Decimal->hexadecimal conversion
	std::string Hex(int n, int d)
	{
		std::string str(d, '0'); //Create a string with d amount of digits (in practice 2 or 4), all zeroes
		for (int i = d - 1; i >= 0; i--) //Start looping from the last character in the string
		{
			str[i] = "0123456789ABCDEF"[n & 0x0F]; //Change the character to one in the hex string - performing a bitwise and with 0x0F gives us the correct index
			n >>= 4; //Shift bits to the right by four (the amount of bits in one hexadecimal digit)
		}
		return str;
	}

};



bool loadTest(cpu6502& cpu)
{
	char* block;
	std::streampos size;
	std::ifstream file;
	file.open("test.bin", std::ios::in | std::ios::binary | std::ios::ate);
	if (file.is_open())
	{
		size = file.tellg();
		block = new char[size];
		file.seekg(0, std::ios::beg);
		file.read(block, size);
		file.close();
		std::vector<char> prg(block, block + size);
		cpu.LoadProgram(prg, 0x00); //Test file is 64kB, start loading it to the very beginning of memory
		cpu.PC = 0x400; //Execution starts at 0x400
		delete[] block;
		return true;
	}
	else
	{
		std::cout << "could not load file";
		return false;
	}
}


int main()
{


	/*
	* ***NOTE***
	* This is Windows specific, so it needs to be changed or removed when compiling to other platforms
	* Get the console window handle to hide the console while runninng the monitor
	*/
	HWND cli = GetConsoleWindow();
	//Show console, if it was hidden the last time the application was run ***REMOVE IF NOT COMPILED ON WINDOWS***
	ShowWindow(cli, 1);

	/*
	*
	* **NOTE***
	* To perform the Klaus Dormann's test on emulator core change coreTest to true
	* The test is performed with the plain core of the emulator as the test takes a *VERY* long time in terms of cycles (96,241,364 cycles).
	* Even with the visualization turned off and advancing clock on every tick, running it with the monitor built on PixelGameEngine means it can only manage about
	* 3,600,000 cycles in an hour on the test machine. Completing the whole test using the monitor would therefore take more than a day, which is not feasible.
	*
	* To verify it actually works without the visualizations, the driver program will interrogate the value in memory location 0x0200, which holds the number of
	* the last performed test. Value 0xF0 means that every test has passed and the driver program will exit, as the test program would remain in an endless
	* loop at location 0x3469. Core test will take about a minute to complete, most of which will be spent executing test 41 (a comprehensive ADC/SBC binary test, around 55-65 seconds on the test machine)
	*
	*/

	std::cout << "Core test is optional and will take approximately 60 seconds\nTest the core and exit? (Y/N)\n";
	char c;
	std::cin >> c;

	bool coreTest = (toupper(c) == 'Y');
	cpu6502 cpu(coreTest); //Create a new instance of the cpu. Coretest is passed as an argument, and used to determinen whether the reset sequence is run or not and where the stack pointer points to, as the test assumes it to be at 0x1FF

	if (coreTest)
	{


		watch test, total; //Stopwatch instances both for individual test and total time
		uint8_t f = 0;
		if (!loadTest(cpu))
			return -1;
		std::cout << "Test " << std::to_string(f) << ": ";
		test.start();
		total.start();

		while (1)
		{
			if (cpu.mem[0x200] != f)
			{

				std::cout << " passed in " << test.stop() << " seconds\n";
				f = cpu.mem[0x200];

				if (f == 0xf0)
				{
					std::cout << "Tests completed in " << total.stop() << " seconds and " << cpu.totalCycles << " cycles\n";
					std::cout << "Press Enter";
					std::system("pause"); //Windows specific, remove or replace for different platform
					break;
				}
				std::cout << "Test " << std::to_string(f) << ": ";
				test.start();
			}
			cpu.Clock();
		}
	}
	else
	{
		ShowWindow(cli, 0); //Hide the console *** REMOVE IF NOT COMPILED FOR WINDOWS ***
		Monitor6502 m(&cpu);
		if (m.Construct(800, 700, 1, 1))
			m.Start();

	}
	return 0;
}
