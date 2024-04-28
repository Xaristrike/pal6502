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


struct MEM
{
	static constexpr uint32 MAX_MEM = 1024 * 64;
	
	Byte Data[MAX_MEM];
	
	void Init()
	{
		for (uint32 i = 0; i < MAX_MEM; i++)
		{
			Data[i] = 0;
		}
	}
	
	
	Byte operator[](uint32 address) const
	{
		return Data[address];
	}
	
	
	Byte& operator[](uint32 address)
	{
		return Data[address];
	}
	
	
	void WriteWord(uint32& cycles, uint32 address, Word val)
	{
		Data[address]	= val & 0xFF;
		Data[address+1]	= (val >> 8);
		
		cycles -= 2;
	}
};


struct CPU
{
	Word PC;		// Program Counter
	Word SP;		// Stack Pointer
	
	Byte A, X, Y;	// GPRs
	
	// Bit field (status flags) (yes this is new) (yes i just learned this)
	
	Byte C : 1;		// Carry flag
	Byte Z : 1;		// Zero flag
	Byte I : 1;		// Interrupt disable
	Byte D : 1;		// Decimal mode
	Byte B : 1;		// Break command
	Byte V : 1;		// Overflow flag
	Byte N : 1;		// Negative flag
	
	void Reset(MEM& memory)
	{
		PC = 0xFFFC;
		SP = 0x0100;
		
		C = Z = I = D = B = V = N = 0;
		
		A = X = Y = 0;
		
		memory.Init();
	}
	
	
	Byte FetchByte(uint32& cycles, MEM& memory)
	{
		Byte data = memory[PC];
		
		PC++;
		cycles--;
		
		return data;
	}
	
	
	Word FetchWord(uint32& cycles, MEM& memory)
	{
		// !!6502 was LITTLE ENDIAN!!
		
		Word Data = memory[PC];
		PC++;
		
		Data |= (memory[PC] << 8);
		PC++;
		
		cycles -= 2;
		
		return Data;
	}
	
	
	Byte ReadByte(uint32& cycles, Byte address, MEM& memory)
	{
		Byte data = memory[address];
		
		cycles--;
		
		return data; 
	}
	
	// NOTE: ISA
	static constexpr Byte
		INS_LDA_IMM		= 0xA9,			// LOAD IMMEDIATE
		INS_LDA_ZP		= 0xA5,			// LOAD FROM MEMORY
		INS_LDA_ZPX		= 0xB5,			// LOAD FROM MEMORY OFFSET BY REG_X
										
		INS_JSR			= 0x20			// JUMP TO SUBROUTINE
		;

	
	void LDA_set_status()
	{
		Z = (A == 0);
		N = (A & 0b10000000) > 0;
	}
	
	
	void Exec(uint32 cycles, MEM& memory)
	{
		while (cycles > 0)
		{
			Byte instruction = FetchByte(cycles, memory);
			
			switch (instruction)
			{
				// Executing (FETCH - DECODE - EXECUTE)
				case INS_LDA_IMM:
				{
					Byte val = FetchByte(cycles, memory);
					
					A = val;
					
					LDA_set_status();
					
				} break;
				
				
				case INS_LDA_ZP:
				{
					Byte zeropageaddr = FetchByte(cycles, memory);
					
					A = ReadByte(cycles, zeropageaddr, memory);
					
					LDA_set_status();
					
				} break;
				
				
				case INS_LDA_ZPX:
				{
					Byte zeropageaddr = FetchByte(cycles, memory);
					
					zeropageaddr += X;
					
					cycles--;
					
					A = ReadByte(cycles, zeropageaddr, memory);
					
					LDA_set_status();
					
				} break;
				
				
				case INS_JSR:
				{
					Word subaddr = FetchWord(cycles, memory);
					
					memory.WriteWord(cycles, SP, PC-1); // PUSH the return address to the stack (Because JSR)
					
					SP++;
					
					PC = subaddr;
					
					cycles--;
					
				} break;
				
				
				
				
				
				default:
				{
					printf("INSTRUCTION UNCLEAR!\n");
				} break;
			}
		}
	}
};


int main()
{
	MEM mem;
	CPU cpu;
	
	cpu.Reset(mem);
	
	// CHEATING
	mem[0xFFFC] = CPU::INS_JSR; // reset vector
	
	mem[0xFFFD] = 0x42;
	mem[0xFFFE] = 0x42;
	
	mem[0x4242] = CPU::INS_LDA_IMM;
	mem[0x4243] = 0x84;
	
	cpu.Exec(8, mem);
	
	return 0;
}