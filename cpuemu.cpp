/*
 * Simulates a 6502 processor.
 * With original commands and opcodes.
 * With virtual memory.
 * With virtual devices.
 * With virtal everything.
 */

#include <stdio.h>
#include <stdlib.h>


using Byte = unsigned char;
using Word = unsigned short;

using uint32 = unsigned int;


/* Memory image */
struct MEM
{
	/* Max memory size.
	 * 
	 * The original 6502 also supported
	 * 
	 * a maximum of 64KB of memory.
	 */
	
	static constexpr uint32 MAX_MEM = 1024 * 64;
	
	/* Memory is made as an array of bytes. */
	Byte Data[MAX_MEM];
	
	/* Function to initialize all memory to 0s.
	 * 
	 * The original 6502 did not do this in hardware,
	 * 
	 * the running programs had to do it themselves.
	 * 
	 * I use it for convenience.
	 * 
	 * Since it's not an original function of the 6502,
	 * 
	 * it takes no cycles to execute.
	 */
	
	void Init()
	{
		for (uint32 i = 0; i < MAX_MEM; i++)
		{
			Data[i] = 0;
		}
	}
	
	/* Overloading operators to read the memory.
	 * 
	 * And since theese are not hardware operations,
	 * 
	 * they take no cycles to execute.
	 */
	
	Byte operator[](uint32 address) const
	{
		return Data[address];
	}
	
	Byte& operator[](uint32 address)
	{
		return Data[address];
	}
	
	
	/* Function to write a word to memory.
	 * 
	 * I used little endian byte encoding
	 * 
	 * because the original 6502 used it as well.
	 */
	
	void WriteWord(uint32& cycles, uint32 address, Word val)
	{
		Data[address]	= val & 0xFF;
		Data[address+1]	= (val >> 8);
		
		/* Takes two cycles */
		cycles -= 2;
	}
};

/* CPU image */
struct CPU
{
	Word PC;		/* Program Counter (16 bits) */
	Word SP;		/* Stack Pointer (16 bits) */
	
	Byte A, X, Y;	/* GPRs (Registers, 8 bits each) */
	
	/* Status flags
	 * 
	 * I used a bit field to store them all in a single byte,
	 * 
	 * instead of using one byte for each bit.
	 */
	
	Byte C : 1;		// Carry flag
	Byte Z : 1;		// Zero flag
	Byte I : 1;		// Interrupt enable (It's active low so it works like an "Interrupt Disable")
	Byte D : 1;		// Decimal mode
	Byte B : 1;		// Break command
	Byte V : 1;		// Overflow flag
	Byte N : 1;		// Negative flag
	
	/* Function to reset the cpu.
	 * 
	 * Sets program counter to 0xFFFC.
	 * 
	 * Clears all flags to 0.
	 * 
	 * Clears all registers to 0.
	 * 
	 * Initializes all memory to 0.
	 * 
	 * 0xFFFC was where the original 6502 had it's reset vector.
	 */
	
	void Reset(MEM& memory)
	{
		PC = 0xFFFC;
		SP = 0x0100;
		
		C = Z = I = D = B = V = N = 0;
		
		A = X = Y = 0;
		
		memory.Init();
	}
	
	/* Function to fetch the next instruction.
	 * 
	 * It reads from memory at the address in the program counter,
	 * 
	 * then increments the program counter.
	 * 
	 * Example:
	 * 
	 * if the program counter has 5,
	 * 
	 * this will read from memory at address 5.
	 * 
	 * Then it will increment the program counter to 6.
	 * 
	 * (End of example)
	 * 
	 * It is the first step in the execution cycle.
	 * 
	 * Returns one byte.
	 * 
	 * Takes one cycle to execute.
	 */
	
	Byte FetchByte(uint32& cycles, MEM& memory)
	{
		/* Use the overloaded operator */
		Byte data = memory[PC];
		
		PC++;
		cycles--;
		
		return data;
	}
	
	
	/* Function to (sometimes) fetch a large instruction.
	 * 
	 * It also reads from memory at the address in the program counter,
	 * 
	 * then increments the program counter,
	 * 
	 * then reads from memory again
	 * 
	 * and then increments the program counter again.
	 * 
	 * Returns one word. Not two separate bytes.
	 * 
	 * Takes two cycles to execute.
	 */
	
	Word FetchWord(uint32& cycles, MEM& memory)
	{
		/* !!6502 was LITTLE ENDIAN!! */
		
		Word Data = memory[PC];
		PC++;
		
		Data |= (memory[PC] << 8);
		PC++;
		
		cycles -= 2;
		
		return Data;
	}
	
	
	/* Function to read one byte from memory.
	 * 
	 * It takes an 8-bit address (byte).
	 * 
	 * It returns one byte.
	 * 
	 * It takes one cycle to execute.
	 */
	
	Byte ReadByte(uint32& cycles, Byte address, MEM& memory)
	{
		Byte data = memory[address];
		
		cycles--;
		
		return data; 
	}
	
	/* Instruction set architecture.
	 * 
	 * This is where all the instructions are,
	 * 
	 * with their binary values.
	 * 
	 * More will be added.
	 */
	
	/* NOTE: ISA */
	static constexpr Byte
	INS_LDA_IMM		= 0xA9,			/* LOAD IMMEDIATE */
	INS_LDA_ZP		= 0xA5,			/* LOAD FROM MEMORY */
	INS_LDA_ZPX		= 0xB5,			/* LOAD FROM MEMORY OFFSET BY X (the register) */
	
	INS_JSR			= 0x20			/* JUMP TO SUBROUTINE */
	;
	
	
	/* Function to set two of the status flags,
	 * 
	 * after an operation.
	 * 
	 * It calculates the Z (zero) flag,
	 * 
	 * which tests if the result of the operation was 0,
	 * 
	 * and then it calculates the N (negative) flag,
	 * 
	 * which tests if the result of the operation was negative.
	 */
	
	void LDA_set_status()
	{
		Z = (A == 0);
		N = (A & 0b10000000) > 0;
	}
	
	
	/* This is the function that executes instructions
	 * 
	 * This gets ran when the program runs.
	 * 
	 * Takes in the number of cycles to be ran,
	 * 
	 * and a memory structure.
	 */
	
	void Exec(uint32 cycles, MEM& memory)
	{
		while (cycles > 0)
		{
			/* Fetch the next instruction */
			Byte instruction = FetchByte(cycles, memory);
			
			/* Decoding.
			 * 
			 * This is the second step of execution.
			 */
			
			switch (instruction)
			{
				/* Execution.
				 * 
				 * This is the third step.
				 */
				
				/* Load immediate.
				 * 
				 * Reads one byte from the address
				 * 
				 * in the program counter,
				 * 
				 * and stores it in the accumulator.
				 * 
				 * Example:
				 * 
				 * If the program counter is 7,
				 * 
				 * this will read from memory at address 7.
				 * 
				 * (End of example)
				 * 
				 * The result goes in the accumulator.
				 */
				
				case INS_LDA_IMM:
				{
					/* Read the current value. */
					Byte val = FetchByte(cycles, memory);
					
					/* Store it in the accumulator (A). */
					A = val;
					
					/* Calculate the flags. */
					LDA_set_status();
					
					/* End */
					break;
				}
				
				
				/* Load the zero page address.
				 * 
				 * Reads one byte from the address
				 * 
				 * in the program counter,
				 * 
				 * and then reads the data from memory,
				 * 
				 * using the read byte as an address.
				 * 
				 * Example:
				 * 
				 * If the program counter has 10,
				 * 
				 * this will read the data stored at address 10.
				 * 
				 * (End of example)
				 * 
				 * Data read goes into the accumulator.
				 */
				
				case INS_LDA_ZP:
				{
					/* Get the address to read from */
					Byte zeropageaddr = FetchByte(cycles, memory);
					
					/* Read the data and store it into the accumulator (A). */
					A = ReadByte(cycles, zeropageaddr, memory);
					
					/* Calculate the flags. */
					LDA_set_status();
					
					/* End */
					break;
				}
				
				
				/* Load with offset by X.
				 * 
				 * Reads one byte from the address
				 * 
				 * in the program counter,
				 * 
				 * then adds the value at register X,
				 * 
				 * then reads from memory
				 * 
				 * using the result as an address.
				 * 
				 * Example:
				 * 
				 * If the program counter is 31,
				 * 
				 * and the X register is 8,
				 * 
				 * this will read the data from memory
				 * 
				 * at address 39.
				 * 
				 * (End of example)
				 * 
				 * Read data goes into the accumulator.
				 */
				
				case INS_LDA_ZPX:
				{
					/* Get the current instruction */
					Byte zeropageaddr = FetchByte(cycles, memory);
					
					/* Add the value stored at register X. */
					zeropageaddr += X;
					
					/* Take an extra clock cycle
					 * 
					 * because the original 6502
					 * 
					 * also took an extra clock cycle.
					 */
					
					cycles--;
					
					/* Read the data and store it into the accumulator (A). */
					A = ReadByte(cycles, zeropageaddr, memory);
					
					/* Calculate the flags. */
					LDA_set_status();
					
					/* End */
					break;
				}
				
				
				/* Jump to subroutine.
				 * 
				 * Reads two bytes from the address
				 * 
				 * in the program counter and the next one,
				 * 
				 * saves the current address in the stack
				 * 
				 * as the return address,
				 * 
				 * then jumps to the read address.
				 * 
				 * Example:
				 * 
				 * If the program counter is 14,
				 * 
				 * this will read the data from memory at address 14,
				 * 
				 * then read the data from memory at address 15,
				 * 
				 * and then it will use 1514 as an address,
				 * 
				 * and finally it will jump to 1514,
				 * 
				 * by setting the program counter to 1514.
				 */
				
				case INS_JSR:
				{
					/* Read the from memory at the current address plus the next address. */
					Word subaddr = FetchWord(cycles, memory);
					
					/* Save the current address in the stack. */
					memory.WriteWord(cycles, SP, PC-1);
					
					/* Increment the stack pointer (I forgot at first). */
					SP++;
					
					/* Set the program counter to the read address. */
					PC = subaddr;
					
					/* Use one extra cycle because the original also used one extra cycle. */
					cycles--;
					
					/* End */
					break;
				}
				
				
				
				
				/* All other instructions get here */
				default:
				{
					printf("INSTRUCTION UNCLEAR!\n");
					
					break;
				}
			}
		}
	}
};


int main()
{
	/* Make the memory struct and the cpu struct. */
	MEM mem;
	CPU cpu;
	
	/* Reset the cpu. */
	cpu.Reset(mem);
	
	/* Below, I cheat by hardcoding values into the memory.
	 * 
	 * Later this will be replaced by a file.
	 * 
	 * The program will read the instructions from a file.
	 */
	
	/*--CHEATING--*/
	mem[0xFFFC] = CPU::INS_JSR; /* reset vector */
	
	mem[0xFFFD] = 0x42;
	mem[0xFFFE] = 0x42;
	
	mem[0x4242] = CPU::INS_LDA_IMM;
	mem[0x4243] = 0x84;
	/*--CHEATING--*/
	
	/* Run for 8 cycles */
	cpu.Exec(8, mem);
	
	/* Hapily exit. */
	return 0;
}