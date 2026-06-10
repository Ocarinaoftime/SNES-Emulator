#include "emulator.h"
#include "cpu.h"

CPU::CPU(Emulator *emu) : _emu(emu) { 
    state.PC = 0x8000; 
    state.SP = 0x01FF; 
    state.inEmu = true;
    SetFlag(ProcessorFlags::Interrupt | ProcessorFlags::IndexMode8 | ProcessorFlags::MemoryMode8);
}
u8 CPU::Read(u32 addr) {
    return _memory[addr];
}
u8 CPU::ReadCode() {
    u8 code = Read((state.K << 16) | state.PC);
    state.PC++;
    return code;
}
u16 CPU::ReadWord(u32 addr) {
    u8 lo = Read((state.K << 16) | addr);
    u8 hi = Read((state.K << 16) | addr + 1);
    return (hi << 8) | lo;
}
u16 CPU::ReadCodeWord() {
    u8 lo = ReadCode();
    u8 hi = ReadCode();
    return (hi << 8) | lo;
}
u32 CPU::ReadCodeLong() {
    u8 b1 = ReadCode();
    u8 b2 = ReadCode();
    u8 b3 = ReadCode();
    return (b3 << 16) | (b2 << 8) | b1;
}
u8 CPU::GetOpCode() {
    return ReadCode();
}
void CPU::Write(u32 addr, u8 value) {
    _memory[addr] = value;
}
void CPU::WriteWord(u32 addr, u16 value) {
    Write(addr, (u8)value);
    Write((addr + 1) & _rwMask, (u8)(value >> 8));
}
void CPU::WriteWordRMW(u32 addr, u16 value) {
    Write((addr + 1) & _rwMask, (u8)(value >> 8));
    Write(addr, (u8)value);
}

void CPU::PushByte(u8 value, bool allowEmu) {
    _memory[(state.K << 16) | state.SP] = value;
    SetSP(state.SP - 1, allowEmu);
}
void CPU::PushWord(u16 value, bool allowEmu) {
    PushByte(value >> 8, allowEmu);
    PushByte((u8)value, allowEmu);
}
u8 CPU::PopByte(bool allowEmu) {
    SetSP(state.SP + 1, allowEmu);
    return ReadData(state.SP);
}
u16 CPU::PopWord(bool allowEmu) {
    u8 lo = PopByte(allowEmu);
    u8 hi = PopByte(allowEmu);
    return (hi << 8) | lo;
}

void CPU::Add8(u8 value) {
    u32 result;
    if (CheckFlag(ProcessorFlags::Decimal)) {
        result = (state.A & 0x0F) + (value & 0x0F) + (state.PSW & ProcessorFlags::Carry);
        if (result > 9)
            result += 6;
        result = (state.A & 0xF0) + (value & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0F);
    } else {
        result = (state.A & 0xFF) + value + (state.PSW & ProcessorFlags::Carry);
    }

    if (~(state.A ^ value) & (state.A ^ result) & 0x80)
        SetFlag(ProcessorFlags::Overflow);
    else
        ClearFlag(ProcessorFlags::Overflow);

    if (CheckFlag(ProcessorFlags::Decimal) && result > 0x9F)
        result += 0x60;

    ClearFlag(ProcessorFlags::Carry | ProcessorFlags::Negative | ProcessorFlags::Zero);

    SetZN((u8)result);

    if (result > 0xFF)
        SetFlag(ProcessorFlags::Carry);

    state.A = (state.A & 0xFF00) | (u8)result;
}

void CPU::Add16(u16 value) {
    u32 result;
    if (CheckFlag(ProcessorFlags::Decimal)) {
        result = (state.A & 0x0F) + (value & 0x0F) + (state.PSW & ProcessorFlags::Carry);

        if (result > 0x09)
            result += 0x06;
        result = (state.A & 0xF0) + (value & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0F);

        if (result > 0x9F)
            result += 0x60;
        result = (state.A & 0xF00) + (value & 0xF00) + (result > 0xFF ? 0x100 : 0) + (result & 0xFF);

        if (result > 0x9FF)
            result += 0x600;
        result = (state.A & 0xF000) + (value & 0xF000) + (result > 0xFFF ? 0x1000 : 0) + (result & 0xFFF);
    } else {
        result = state.A + value + (state.PSW & ProcessorFlags::Carry);
    }

    if (~(state.A ^ value) & (state.A ^ result) & 0x8000) {
        SetFlag(ProcessorFlags::Overflow);
    } else {
        ClearFlag(ProcessorFlags::Overflow);
    }

    if (CheckFlag(ProcessorFlags::Decimal) && result > 0x9FFF) {
        result += 0x6000;
    }

    ClearFlag(ProcessorFlags::Carry | ProcessorFlags::Negative | ProcessorFlags::Zero);
    SetZN((uint16_t)result);

    if (result > 0xFFFF) {
        SetFlag(ProcessorFlags::Carry);
    }

    state.A = (uint16_t)result;
}

void CPU::Sub8(u8 value) {
    int32_t result;
	if(CheckFlag(ProcessorFlags::Decimal)) {
		result = (state.A & 0x0F) + (value & 0x0F) + (state.PSW & ProcessorFlags::Carry);
		if(result <= 0x0F) result -= 0x06;
		result = (state.A & 0xF0) + (value & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0F);
	} else {
		result = (state.A & 0xFF) + value + (state.PSW & ProcessorFlags::Carry);
	}

	if(~(state.A ^ value) & (state.A ^ result) & 0x80) {
		SetFlag(ProcessorFlags::Overflow);
	} else {
		ClearFlag(ProcessorFlags::Overflow);
	}

	if(CheckFlag(ProcessorFlags::Decimal) && result <= 0xFF) {
		result -= 0x60;
	}

	ClearFlag(ProcessorFlags::Carry | ProcessorFlags::Negative | ProcessorFlags::Zero);
	SetZN((uint8_t)result);

	if(result > 0xFF) {
		SetFlag(ProcessorFlags::Carry);
	}

	state.A = (state.A & 0xFF00) | (uint8_t)result;
}

void CPU::Sub16(u16 value) {
    int32_t result;
	if(CheckFlag(ProcessorFlags::Decimal)) {
		result = (state.A & 0x0F) + (value & 0x0F) + (state.PSW & ProcessorFlags::Carry);

		if(result <= 0x0F) result -= 0x06;
		result = (state.A & 0xF0) + (value & 0xF0) + (result > 0x0F ? 0x10 : 0) + (result & 0x0F);

		if(result <= 0xFF) result -= 0x60;
		result = (state.A & 0xF00) + (value & 0xF00) + (result > 0xFF ? 0x100 : 0) + (result & 0xFF);

		if(result <= 0xFFF) result -= 0x600;
		result = (state.A & 0xF000) + (value & 0xF000) + (result > 0xFFF ? 0x1000 : 0) + (result & 0xFFF);
	} else {
		result = state.A + value + (state.PSW & ProcessorFlags::Carry);
	}

	if(~(state.A ^ value) & (state.A ^ result) & 0x8000) {
		SetFlag(ProcessorFlags::Overflow);
	} else {
		ClearFlag(ProcessorFlags::Overflow);
	}

	if(CheckFlag(ProcessorFlags::Decimal) && result <= 0xFFFF) {
		result -= 0x6000;
	}

	ClearFlag(ProcessorFlags::Carry | ProcessorFlags::Negative | ProcessorFlags::Zero);
	SetZN((uint16_t)result);

	if(result > 0xFFFF) {
		SetFlag(ProcessorFlags::Carry);
	}

	state.A = (uint16_t)result;
}

//Instructions
//Add/Sub
void CPU::ADC() {
    if (MEM8)
        Add8(GetOperandByte());
    else
        Add16(GetOperandWord());
}
void CPU::SBC() {
    if (MEM8) 
        Sub8(GetOperandByte());
    else
        Sub16(GetOperandWord());
}
//Branch
void CPU::BCC() {
    BranchRel(!CheckFlag(ProcessorFlags::Carry));
}
void CPU::BCS() {
    BranchRel(CheckFlag(ProcessorFlags::Carry));
}
void CPU::BEQ() {
    BranchRel(CheckFlag(ProcessorFlags::Zero));
}
void CPU::BMI() {
    BranchRel(CheckFlag(ProcessorFlags::Negative));
}
void CPU::BNE() {
    BranchRel(!CheckFlag(ProcessorFlags::Zero));
}
void CPU::BPL() {
    BranchRel(!CheckFlag(ProcessorFlags::Negative));
}
void CPU::BRA() {
    BranchRel(true);
}
void CPU::BRL() {
    state.PC = (u16)(state.PC + (s16)_operand);
    //IdleTakeBranch();
}
void CPU::BVC() {
    BranchRel(!CheckFlag(ProcessorFlags::Overflow));
}
void CPU::BVS() {
    BranchRel(CheckFlag(ProcessorFlags::Overflow));
}
//Set / Clear Flags
void CPU::CLC() {
    ClearFlag(ProcessorFlags::Carry);
}
void CPU::CLD() {
    ClearFlag(ProcessorFlags::Decimal);
}
void CPU::CLI() {
    ClearFlag(ProcessorFlags::Interrupt);
}
void CPU::CLV() {
    ClearFlag(ProcessorFlags::Overflow);
}
void CPU::SEC() {
    SetFlag(ProcessorFlags::Carry);
}
void CPU::SED() {
    SetFlag(ProcessorFlags::Decimal);
}
void CPU::SEI() {
    SetFlag(ProcessorFlags::Interrupt);
}
void CPU::REP() {
    ClearFlag((u8)_operand);
    if (state.inEmu)
        SetFlag(ProcessorFlags::MemoryMode8 | ProcessorFlags::IndexMode8);
}
void CPU::SEP() {
    SetFlag((u8)_operand);
    if (INDEX8) {
        state.X &= 0xFF;
        state.Y &= 0xFF;
    }
}
//Inc/Dec
void CPU::DEX() {
    SetReg(state.X, state.X - 1, INDEX8);
}
void CPU::DEY() {
    SetReg(state.Y, state.Y - 1, INDEX8);
}
void CPU::INX() {
    SetReg(state.X, state.X + 1, INDEX8);
}
void CPU::INY() {
    SetReg(state.Y, state.Y + 1, INDEX8);
}
void CPU::DEC() {
    //offset = -1
    if (MEM8) {
        u8 value = GetOperandByte();
        //IdleOrDummyWrite not implemented yet
        value -= 1;
        SetZN(value);
        Write(_operand, value);
    } else {
        u16 value = GetOperandWord();
        //Idle();
        value -= 1;
        SetZN(value);
        WriteWordRMW(_operand, value);
    }
}
void CPU::INC() {
    if (MEM8) {
        u8 value = GetOperandByte();
        //IdleOrDummyWrite not implemented yet
        value += 1;
        SetZN(value);
        Write(_operand, value);
    } else {
        u16 value = GetOperandWord();
        //Idle();
        value += 1;
        SetZN(value);
        WriteWordRMW(_operand, value);
    }
}
void CPU::DEC_A() {
    SetReg(state.A, state.A - 1, MEM8);
}
void CPU::INC_A() {
    SetReg(state.A, state.A + 1, MEM8);
}
//Compare
void CPU::Compare(u16 reg, bool mode8) {
    if (mode8) {
        u8 value = GetOperandByte();
        if ((u8)reg >= value)
            SetFlag(ProcessorFlags::Carry);
        else
            ClearFlag(ProcessorFlags::Carry);

        u8 result = (u8)reg - value;
        SetZN(result);
    } else {
        u16 value = GetOperandWord();
        if (reg >= value)
            SetFlag(ProcessorFlags::Carry);
        else
            ClearFlag(ProcessorFlags::Carry);

        u16 result = reg - value;
        SetZN(result);
    }
}
void CPU::CMP() {
    Compare(state.A, MEM8);
}
void CPU::CPX() {
    Compare(state.X, INDEX8);
}
void CPU::CPY() {
    Compare(state.Y, INDEX8);
}
//Jump
void CPU::JML() {
    state.K = (_operand >> 16) & 0xFF;
    state.PC = (u16)_operand;
    //IdleEndJump not implemented yet
}
void CPU::JMP() {
    state.PC = (u16)_operand;
    //IdleEndJump();
}
void CPU::JSL() {
    u8 b1 = ReadCode();
    u8 b2 = ReadCode();

    PushByte(state.K, false);
    //Idle();

    u8 b3 = ReadCode();

    PushWord(state.PC - 1, false);

    state.K = b3;
    state.PC = b1 | (b2 << 8);
    RestrictSP();
    //IdleEndJump() not implemented yet
}
void CPU::JSR() {
    PushWord(state.PC - 1);
    state.PC = (u16)_operand;
    //IdleEndJump();
}
void CPU::RTI() {
    //Idle() x2

    if (state.inEmu) {
        state.PSW = (PopByte() | ProcessorFlags::MemoryMode8 | ProcessorFlags::IndexMode8);
        if (INDEX8) {
            state.Y &= 0xFF;
            state.X &= 0xFF;
        }
    } else {
        state.PSW = PopByte();
        state.PC = PopWord();
        state.K = PopByte();
    }
    //IdleEndJump;
}
void CPU::RTL() {
    //Idle() x2

    state.PC = PopWord(false);
    state.PC++;
    state.K = PopByte(false);
    RestrictSP();
    //IdleEndJump;
}
void CPU::RTS() {
    //Idle() x2

    state.PC = PopWord();
    //Idle();
    state.PC++;
    //IdleEndJump();
}
void CPU::JMP_AbsInxIndX() {
    u16 base = ReadCodeWord() + state.X;
    //Idle();
    u8 l = ReadData(GetProgramAddr(base));
    u8 h = ReadData(GetProgramAddr(base + 1));

    _operand = (u16)GetProgramAddr(l | (h << 8));
    JMP();
}
void CPU::JSR_AbsInxIndX() {
    u8 l = ReadCode();
    PushWord(state.PC, false);
    u8 h = ReadCode();

    u16 base = (l | (h << 8)) + state.X;
    //idle();

    l = ReadData(GetProgramAddr(base));
    h = ReadData(GetProgramAddr(base + 1));
    state.PC = (u16)GetProgramAddr(l | (h << 8));
    RestrictSP();
    //IdleEndJump
}
//Interrupts
void CPU::BRK() {
    ProcessInterrupt(state.inEmu ? (u32)0x00FFFE : (u32)0x00FFE6, false);
}
void CPU::COP() {
    ProcessInterrupt(state.inEmu ? (u32)0x00FFF4 : (u32)0x00FFE4, false);
}
//Bits
void CPU::AND() {
    if (MEM8)
        SetReg(state.A, state.A & GetOperandByte(), true);
    else
        SetReg(state.A, state.A & GetOperandWord(), false);
}
void CPU::EOR() {
    if (MEM8)
        SetReg(state.A, state.A ^ GetOperandByte(), true);
    else
        SetReg(state.A, state.A ^ GetOperandWord(), false);
}
void CPU::ORA() {
    if (MEM8)
        SetReg(state.A, state.A | GetOperandByte(), true);
    else
        SetReg(state.A, state.A | GetOperandWord(), false);
}
//Shifts
template<typename T> T CPU::SLeft(T value) {
    T result = value << 1;
	if(value & (1 << (sizeof(T) * 8 - 1))) {
		SetFlag(ProcessorFlags::Carry);
	} else {
		ClearFlag(ProcessorFlags::Carry);
	}
	SetZN(result);
	return result;
}
template<typename T> T CPU::RLeft(T value) {
    T result = value << 1 | (state.PSW & ProcessorFlags::Carry);
	if(value & (1 << (sizeof(T) * 8 - 1))) {
		SetFlag(ProcessorFlags::Carry);
	} else {
		ClearFlag(ProcessorFlags::Carry);
	}
	SetZN(result);
	return result;
}
template<typename T> T CPU::SRight(T value) {
    T result = value >> 1;
	if(value & 0x01) {
		SetFlag(ProcessorFlags::Carry);
	} else {
		ClearFlag(ProcessorFlags::Carry);
	}
	SetZN(result);
	return result;
}
template<typename T> T CPU::RRight(T value) {
    T result = value >> 1 | ((state.PSW & 0x01) << (sizeof(T) * 8 - 1));
	if(value & 0x01) {
		SetFlag(ProcessorFlags::Carry);
	} else {
		ClearFlag(ProcessorFlags::Carry);
	}
	SetZN(result);
	return result;
}
void CPU::ASL_A()
{
    if (MEM8)
        state.A = (state.A & 0xFF00) | (SLeft<u8>((u8)state.A));
    else
        state.A = SLeft<u16>(state.A);
}
void CPU::ASL() {
    if (MEM8) {
        u8 value = GetOperandByte();
        //IdleOrDummyWrite;
        Write(_operand, SLeft<u8>(value));
    } else {
        u16 value = GetOperandWord();
        //Idle();
        WriteWordRMW(_operand, SLeft<u16>(value));
    }
}
void CPU::LSR_A() {
    if (MEM8)
        state.A = (state.A & 0xFF00) | SRight<u8>((u8)state.A);
    else 
        state.A = SRight<u16>(state.A);
}
void CPU::LSR() {
    if (MEM8) {
        u8 value = GetOperandByte();
        //IdleOrDummyWrite();
        Write(_operand, SRight<u8>(value));
    } else {
        u16 value = GetOperandWord();
        //Idle();
        WriteWordRMW(_operand, SRight<u16>(value));
    }
}
void CPU::ROL_A() {
    if (MEM8)
        state.A = (state.A & 0xFF00) | RLeft<u8>((u8)state.A);
    else
        state.A = RLeft<u16>(state.A);
}
void CPU::ROL() {
    if (MEM8) {
        u8 value = GetOperandByte();
        //IdleOrDW();
        Write(_operand, RLeft<u8>(value));
    } else {
        u16 value = GetOperandWord();
        //Idle();
        WriteWordRMW(_operand, RLeft<u16>(value));
    }
}
void CPU::ROR_A() {
    if (MEM8)
        state.A = (state.A & 0xFF00) | RRight<u8>((u8)state.A);
    else
        state.A = RRight<u16>(state.A);
}
void CPU::ROR() {
    if (MEM8) {
        u8 value = GetOperandByte();
        //IdleOrDW();
        Write(_operand, RRight<u8>(value));
    } else {
        u16 value = GetOperandWord();
        //Idle();
        WriteWordRMW(_operand, RRight<u16>(value));
    }
}
//Move
void CPU::MVN() {
    state.DBR = _operand & 0xFF;
    u32 dest = state.DBR << 16;
    u32 src = (_operand << 8) & 0xFF0000;
    u8 value = ReadData(src | state.X);
    Write(dest | state.Y, value);

    //Idle() x2
    state.X++;
    state.Y++;
    if (INDEX8) {
        state.X &= 0xFF;
        state.Y &= 0xFF;
    }

    state.A--;

    if (state.A != 0xFFFF) {
        state.PC -= 3;
    }
}
void CPU::MVP() {
    state.DBR = _operand & 0xFF;
    u32 dest = state.DBR << 16;
    u32 src = (_operand << 8) & 0xFF0000;

    u8 value = ReadData(src | state.X);
    Write(dest | state.Y, value);

    //Idle() x2
    state.X--;
    state.Y--;
    if (INDEX8) {
        state.X &= 0xFF;
        state.Y &= 0xFF;
    }

    state.A--;
    if (state.A != 0xFFFF) {
        state.PC -= 3;
    }
}
//Push and Pull
void CPU::PEA() {
    PushWord((u16)_operand, false);
    RestrictSP();
}
void CPU::PEI() {
    PushWord(ReadDataWord(_operand), false);
    RestrictSP();
}
void CPU::PER() {
    PushWord((u16)((s16)_operand + state.PC), false);
    RestrictSP();
}
void CPU::PHB() {
    //Idle;
    PushByte(state.DBR);
}
void CPU::PHD() {
    //Idle
    PushWord(state.D, false);
    RestrictSP();
}
void CPU::PHK() {
    //Idle();
    PushByte(state.K);
}
void CPU::PHP() {
    //Idle
    PushByte(state.PSW);
}
void CPU::PLB() {
    //Idle x2
    SetReg(state.DBR, PopByte(false));
    RestrictSP();
}
void CPU::PLD() {
    //Idle x2
    SetReg(state.D, PopWord(false), false);
    RestrictSP();
}
void CPU::PLP() {
    //Idle x2
    if (state.inEmu)
        SetPS(PopByte() | ProcessorFlags::MemoryMode8 | ProcessorFlags::IndexMode8);
    else
        SetPS(PopByte());
}
void CPU::PHA() {
    //Idle
    PushReg(state.A, MEM8);
}
void CPU::PHX() {
    //Idle
    PushReg(state.X, INDEX8);
}
void CPU::PHY() {
    //Idle
    PushReg(state.Y, INDEX8);
}
void CPU::PLA() {
    //idle x2
    PullReg(state.A, MEM8);
}
void CPU::PLX() {
    //idle x2
    PullReg(state.X, INDEX8);
}
void CPU::PLY() {
    //idle x2
    PullReg(state.Y, INDEX8);
}
void CPU::PushReg(u16 reg, bool mem8) {
    if (mem8)
        PushByte((u8)reg);
    else
        PushWord(reg);
}
void CPU::PullReg(u16 &reg, bool mem8) {
    if (mem8)
        SetReg(reg, PopByte(), true);
    else
        SetReg(reg, PopWord(), false);
}
//Store/load
void CPU::LoadReg(u16 &reg, bool mem8) {
    if (mem8)
        SetReg(reg, GetOperandByte(), true);
    else 
        SetReg(reg, GetOperandWord(), false);
}
void CPU::StoreReg(u16 value, bool mem8) {
    if (mem8)
        Write(_operand, (u8)value);
    else
        WriteWord(_operand, value);
}
void CPU::SetReg(u16 &reg, u16 value, bool mode8) {
    if (mode8) {
        SetZN((u8)value);
        reg = (reg & 0xFF00) | (u8)value;
    } else {
        SetZN(value);
        reg = value;
    }
}
void CPU::SetReg(u8 &reg, u8 value) {
    SetZN(value);
    reg = value;
}
void CPU::LDA() {
    LoadReg(state.A, MEM8);
}
void CPU::LDX() {
    LoadReg(state.X, INDEX8);
}
void CPU::LDY() {
    LoadReg(state.Y, INDEX8);
}
void CPU::STA() {
    StoreReg(state.A, MEM8);
}
void CPU::STX() {
    StoreReg(state.X, INDEX8);
}
void CPU::STY() {
    StoreReg(state.Y, INDEX8);
}
void CPU::STZ() {
    StoreReg(0, MEM8);
}
template<typename T> void CPU::TestBits(T val, bool zeroOnly) {
    if (zeroOnly) {
        if (((T)state.A & val) == 0) {
			SetFlag(ProcessorFlags::Zero);
		} else {
			ClearFlag(ProcessorFlags::Zero);
		}
    } else {
        ClearFlag(ProcessorFlags::Zero | ProcessorFlags::Overflow | ProcessorFlags::Negative);

		if (((T)state.A & val) == 0) {
			SetFlag(ProcessorFlags::Zero);
		}

		if (val & (1 << (sizeof(T) * 8 - 2))) {
			SetFlag(ProcessorFlags::Overflow);
		}
		if (val & (1 << (sizeof(T) * 8 - 1))) {
			SetFlag(ProcessorFlags::Negative);
		}
    }
}
void CPU::BIT() {
    if (MEM8)
        TestBits<u8>(GetOperandByte(), _imm);
    else
        TestBits<u16>(GetOperandWord(), _imm);
}
void CPU::TRB() {
    if (MEM8) {
        u8 value = GetOperandByte();
        TestBits<u8>(value, true);

        //IdleOrDW
        value &= ~state.A;

        Write(_operand, value);
    } else {
        u16 value = GetOperandWord();
        TestBits<u16>(value, true);

        //Idle
        value &= ~state.A;
        WriteWordRMW(_operand, value);
    }
}
void CPU::TSB() {
    if(MEM8) {
		uint8_t value = GetOperandByte();
		TestBits<u8>(value, true);

		//IdleOrDummyWrite;
		value |= state.A;

		Write(_operand, value);
	} else {
		uint16_t value = GetOperandWord();
		TestBits<u16>(value, true);

		//Idle();
		value |= state.A;

		WriteWordRMW(_operand, value);
	}
}
//Transfers
void CPU::TAX() {
    SetReg(state.X, state.A, INDEX8);
}
void CPU::TAY() {
    SetReg(state.Y, state.A, INDEX8);
}
void CPU::TCD() {
    SetReg(state.D, state.A, false);
}
void CPU::TCS() {
    SetSP(state.A);
}
void CPU::TDC() {
    SetReg(state.A, state.D, false);
}
void CPU::TSC() {
    SetReg(state.A, state.SP, false);
}
void CPU::TSX() {
    SetReg(state.X, state.SP, INDEX8);
}
void CPU::TXA() {
    SetReg(state.A, state.X, MEM8);
}
void CPU::TXS() {
    SetSP(state.X);
}
void CPU::TXY() {
    SetReg(state.Y, state.X, INDEX8);
}
void CPU::TYA() {
    SetReg(state.A, state.Y, MEM8);
}
void CPU::TYX() {
    SetReg(state.X, state.Y, INDEX8);
}
void CPU::XBA() {
    //Idle
    state.A = ((state.A & 0xFF) << 8) | ((state.A >> 8) & 0xFF);
    SetZN((u8)state.A);
}
void CPU::XCE() {
    bool c = CheckFlag(ProcessorFlags::Carry);
    if (state.inEmu)
        SetFlag(ProcessorFlags::Carry);
    else
        ClearFlag(ProcessorFlags::Carry);
    state.inEmu = c;
    if (state.inEmu) {
        SetPS(state.PSW | ProcessorFlags::IndexMode8 | ProcessorFlags::MemoryMode8);
        state.SP = 0x100 | (state.SP & 0xFF);
    }
}
//No operation
void CPU::NOP() {
    //one byte
}
void CPU::WDM() {
    //two bytes
}
void CPU::STP() {
    state.stopState = CPUStopState::Stopped;
}
void CPU::WAI() {
    state.stopState = CPUStopState::WaitInterrupt;
    _waitOver = false;
}
//End Instructions

void CPU::CheckForInterrupts() {
    if (state.needNmi) {
        state.needNmi = false;
        GetProgramAddr(state.PC);
        ProcessInterrupt(state.inEmu ? (u16)0xFFFA : (u16)0xFFEA, true);
    } else if (state.prevIrqSrc) {
        GetProgramAddr(state.PC);
        ProcessInterrupt(state.inEmu ? (u16)0xFFFE : (u16)0xFFEE, true);
    }
}
void CPU::ProcessHaltedState() {
    if (state.stopState == CPUStopState::Stopped) {
        //do nothing
        _emu->SetHalted();
    } else {
        bool over = _waitOver;
        //Idle
        if (over) {
            state.stopState = CPUStopState::Running;
            CheckForInterrupts();
        }
    }
}

bool first = true;

void CPU::Exec() {
    _imm = false;
    _rwMask = 0xFFFFFF;
    if (state.stopState == CPUStopState::Running) {
        u8 opCode = GetOpCode();
        printf("Running opCode 0x%02X at PC %06X\n", opCode, (state.K << 16) | state.PC - 1);
        RunOpCode(opCode);
        CheckForInterrupts();
    } else {
        ProcessHaltedState();
    }
}
void CPU::RunOpCode(u8 op) {
    switch (op) {
        case 0x00: Addr_Imm8(); BRK(); break;
		case 0x01: Addr_DirInxIndX(); ORA(); break;
		case 0x02: Addr_Imm8(); COP(); break;
		case 0x03: Addr_StackRelative(); ORA(); break;
		case 0x04: Addr_Dir(); TSB(); break;
		case 0x05: Addr_Dir(); ORA(); break;
		case 0x06: Addr_Dir(); ASL(); break;
		case 0x07: Addr_DirIndLong(); ORA(); break;
		case 0x08: PHP(); break;
		case 0x09: Addr_ImmM(); ORA(); break;
		case 0x0A: Addr_A(); ASL_A(); break;
		case 0x0B: PHD(); break;
		case 0x0C: Addr_Abs(); TSB(); break;
		case 0x0D: Addr_Abs(); ORA(); break;
		case 0x0E: Addr_Abs(); ASL(); break;
		case 0x0F: Addr_AbsLong(); ORA(); break;
		case 0x10: Addr_Relative(); BPL(); break;
		case 0x11: Addr_DirIndInxY(); ORA(); break;
		case 0x12: Addr_DirInd(); ORA(); break;
		case 0x13: Addr_StackRelativeIndInxY(); ORA(); break;
		case 0x14: Addr_Dir(); TRB(); break;
		case 0x15: Addr_DirInxX(); ORA(); break;
		case 0x16: Addr_DirInxX(); ASL(); break;
		case 0x17: Addr_DirIndLongInxY(); ORA(); break;
		case 0x18: Addr_Imp(); CLC(); break;
		case 0x19: Addr_AbsInxX(); ORA(); break;
		case 0x1A: Addr_A(); INC_A(); break;
		case 0x1B: Addr_Imp(); TCS(); break;
		case 0x1C: Addr_Abs(); TRB(); break;
		case 0x1D: Addr_AbsInxX(); ORA(); break;
		case 0x1E: Addr_AbsInxX(); ASL(); break;
		case 0x1F: Addr_AbsLongInxX(); ORA(); break;
		case 0x20: Addr_AbsJmp(); JSR(); break;
		case 0x21: Addr_DirInxIndX(); AND(); break;
		case 0x22: JSL(); break;
		case 0x23: Addr_StackRelative(); AND(); break;
		case 0x24: Addr_Dir(); BIT(); break;
		case 0x25: Addr_Dir(); AND(); break;
		case 0x26: Addr_Dir(); ROL(); break;
		case 0x27: Addr_DirIndLong(); AND(); break;
		case 0x28: PLP(); break;
		case 0x29: Addr_ImmM(); AND(); break;
		case 0x2A: Addr_A(); ROL_A(); break;
		case 0x2B: PLD(); break;
		case 0x2C: Addr_Abs(); BIT(); break;
		case 0x2D: Addr_Abs(); AND(); break;
		case 0x2E: Addr_Abs(); ROL(); break;
		case 0x2F: Addr_AbsLong(); AND(); break;
		case 0x30: Addr_Relative(); BMI(); break;
		case 0x31: Addr_DirIndInxY(); AND(); break;
		case 0x32: Addr_DirInd(); AND(); break;
		case 0x33: Addr_StackRelativeIndInxY(); AND(); break;
		case 0x34: Addr_DirInxX(); BIT(); break;
		case 0x35: Addr_DirInxX(); AND(); break;
		case 0x36: Addr_DirInxX(); ROL(); break;
		case 0x37: Addr_DirIndLongInxY(); AND(); break;
		case 0x38: Addr_Imp(); SEC(); break;
		case 0x39: Addr_AbsInxX(); AND(); break;
		case 0x3A: Addr_A(); DEC_A(); break;
		case 0x3B: Addr_Imp(); TSC(); break;
		case 0x3C: Addr_AbsInxX(); BIT(); break;
		case 0x3D: Addr_AbsInxX(); AND(); break;
		case 0x3E: Addr_AbsInxX(); ROL(); break;
		case 0x3F: Addr_AbsLongInxX(); AND(); break;
		case 0x40: RTI(); break;
		case 0x41: Addr_DirInxIndX(); EOR(); break;
		case 0x42: Addr_Imm8(); WDM(); break;
		case 0x43: Addr_StackRelative(); EOR(); break;
		case 0x44: Addr_BlkMov(); MVP(); break;
		case 0x45: Addr_Dir(); EOR(); break;
		case 0x46: Addr_Dir(); LSR(); break;
		case 0x47: Addr_DirIndLong(); EOR(); break;
		case 0x48: PHA(); break;
		case 0x49: Addr_ImmM(); EOR(); break;
		case 0x4A: Addr_A(); LSR_A(); break;
		case 0x4B: PHK(); break;
		case 0x4C: Addr_AbsJmp(); JMP(); break;
		case 0x4D: Addr_Abs(); EOR(); break;
		case 0x4E: Addr_Abs(); LSR(); break;
		case 0x4F: Addr_AbsLong(); EOR(); break;
		case 0x50: Addr_Relative(); BVC(); break;
		case 0x51: Addr_DirIndInxY(); EOR(); break;
		case 0x52: Addr_DirInd(); EOR(); break;
		case 0x53: Addr_StackRelativeIndInxY(); EOR(); break;
		case 0x54: Addr_BlkMov(); MVN(); break;
		case 0x55: Addr_DirInxX(); EOR(); break;
		case 0x56: Addr_DirInxX(); LSR(); break;
		case 0x57: Addr_DirIndLongInxY(); EOR(); break;
		case 0x58: Addr_Imp(); CLI(); break;
		case 0x59: Addr_AbsInxX(); EOR(); break;
		case 0x5A: PHY(); break;
		case 0x5B: Addr_Imp(); TCD(); break;
		case 0x5C: Addr_AbsLongJmp(); JML(); break;
		case 0x5D: Addr_AbsInxX(); EOR(); break;
		case 0x5E: Addr_AbsInxX(); LSR(); break;
		case 0x5F: Addr_AbsLongInxX(); EOR(); break;
		case 0x60: RTS(); break;
		case 0x61: Addr_DirInxIndX(); ADC(); break;
		case 0x62: Addr_RelativeLong(); PER(); break;
		case 0x63: Addr_StackRelative(); ADC(); break;
		case 0x64: Addr_Dir(); STZ(); break;
		case 0x65: Addr_Dir(); ADC(); break;
		case 0x66: Addr_Dir(); ROR(); break;
		case 0x67: Addr_DirIndLong(); ADC(); break;
		case 0x68: PLA(); break;
		case 0x69: Addr_ImmM(); ADC(); break;
		case 0x6A: Addr_A(); ROR_A(); break;
		case 0x6B: RTL(); break;
		case 0x6C: Addr_AbsInd(); JMP(); break;
		case 0x6D: Addr_Abs(); ADC(); break;
		case 0x6E: Addr_Abs(); ROR(); break;
		case 0x6F: Addr_AbsLong(); ADC(); break;
		case 0x70: Addr_Relative(); BVS(); break;
		case 0x71: Addr_DirIndInxY(); ADC(); break;
		case 0x72: Addr_DirInd(); ADC(); break;
		case 0x73: Addr_StackRelativeIndInxY(); ADC(); break;
		case 0x74: Addr_DirInxX(); STZ(); break;
		case 0x75: Addr_DirInxX(); ADC(); break;
		case 0x76: Addr_DirInxX(); ROR(); break;
		case 0x77: Addr_DirIndLongInxY(); ADC(); break;
		case 0x78: Addr_Imp(); SEI(); break;
		case 0x79: Addr_AbsInxX(); ADC(); break;
		case 0x7A: PLY(); break;
		case 0x7B: Addr_Imp(); TDC(); break;
		case 0x7C: JMP_AbsInxIndX(); break;
		case 0x7D: Addr_AbsInxX(); ADC(); break;
		case 0x7E: Addr_AbsInxX(); ROR(); break;
		case 0x7F: Addr_AbsLongInxX(); ADC(); break;
		case 0x80: Addr_Relative(); BRA(); break;
		case 0x81: Addr_DirInxIndX(); STA(); break;
		case 0x82: Addr_RelativeLong(); BRL(); break;
		case 0x83: Addr_StackRelative(); STA(); break;
		case 0x84: Addr_Dir(); STY(); break;
		case 0x85: Addr_Dir(); STA(); break;
		case 0x86: Addr_Dir(); STX(); break;
		case 0x87: Addr_DirIndLong(); STA(); break;
		case 0x88: Addr_Imp(); DEY(); break;
		case 0x89: Addr_ImmM(); BIT(); break;
		case 0x8A: Addr_Imp(); TXA(); break;
		case 0x8B: PHB(); break;
		case 0x8C: Addr_Abs(); STY(); break;
		case 0x8D: Addr_Abs(); STA(); break;
		case 0x8E: Addr_Abs(); STX(); break;
		case 0x8F: Addr_AbsLong(); STA(); break;
		case 0x90: Addr_Relative(); BCC(); break;
		case 0x91: Addr_DirIndInxY(); STA(); break;
		case 0x92: Addr_DirInd(); STA(); break;
		case 0x93: Addr_StackRelativeIndInxY(); STA(); break;
		case 0x94: Addr_DirInxX(); STY(); break;
		case 0x95: Addr_DirInxX(); STA(); break;
		case 0x96: Addr_DirInxY(); STX(); break;
		case 0x97: Addr_DirIndLongInxY(); STA(); break;
		case 0x98: Addr_Imp(); TYA(); break;
		case 0x99: Addr_AbsInxX(); STA(); break;
		case 0x9A: Addr_Imp(); TXS(); break;
		case 0x9B: Addr_Imp(); TXY(); break;
		case 0x9C: Addr_Abs(); STZ(); break;
		case 0x9D: Addr_AbsInxX(); STA(); break;
		case 0x9E: Addr_AbsInxX(); STZ(); break;
		case 0x9F: Addr_AbsLongInxX(); STA(); break;
		case 0xA0: Addr_ImmX(); LDY(); break;
		case 0xA1: Addr_DirInxIndX(); LDA(); break;
		case 0xA2: Addr_ImmX(); LDX(); break;
		case 0xA3: Addr_StackRelative(); LDA(); break;
		case 0xA4: Addr_Dir(); LDY(); break;
		case 0xA5: Addr_Dir(); LDA(); break;
		case 0xA6: Addr_Dir(); LDX(); break;
		case 0xA7: Addr_DirIndLong(); LDA(); break;
		case 0xA8: Addr_Imp(); TAY(); break;
		case 0xA9: Addr_ImmM(); LDA(); break;
		case 0xAA: Addr_Imp(); TAX(); break;
		case 0xAB: PLB(); break;
		case 0xAC: Addr_Abs(); LDY(); break;
		case 0xAD: Addr_Abs(); LDA(); break;
		case 0xAE: Addr_Abs(); LDX(); break;
		case 0xAF: Addr_AbsLong(); LDA(); break;
		case 0xB0: Addr_Relative(); BCS(); break;
		case 0xB1: Addr_DirIndInxY(); LDA(); break;
		case 0xB2: Addr_DirInd(); LDA(); break;
		case 0xB3: Addr_StackRelativeIndInxY(); LDA(); break;
		case 0xB4: Addr_DirInxX(); LDY(); break;
		case 0xB5: Addr_DirInxX(); LDA(); break;
		case 0xB6: Addr_DirInxY(); LDX(); break;
		case 0xB7: Addr_DirIndLongInxY(); LDA(); break;
		case 0xB8: Addr_Imp(); CLV(); break;
		case 0xB9: Addr_AbsInxX(); LDA(); break;
		case 0xBA: Addr_Imp(); TSX(); break;
		case 0xBB: Addr_Imp(); TYX(); break;
		case 0xBC: Addr_AbsInxX(); LDY(); break;
		case 0xBD: Addr_AbsInxX(); LDA(); break;
		case 0xBE: Addr_AbsInxX(); LDX(); break;
		case 0xBF: Addr_AbsLongInxX(); LDA(); break;
		case 0xC0: Addr_ImmX(); CPY(); break;
		case 0xC1: Addr_DirInxIndX(); CMP(); break;
		case 0xC2: Addr_Imm8(); REP(); break;
		case 0xC3: Addr_StackRelative(); CMP(); break;
		case 0xC4: Addr_Dir(); CPY(); break;
		case 0xC5: Addr_Dir(); CMP(); break;
		case 0xC6: Addr_Dir(); DEC(); break;
		case 0xC7: Addr_DirIndLong(); CMP(); break;
		case 0xC8: Addr_Imp(); INY(); break;
		case 0xC9: Addr_ImmM(); CMP(); break;
		case 0xCA: Addr_Imp(); DEX(); break;
		case 0xCB: WAI(); break;
		case 0xCC: Addr_Abs(); CPY(); break;
		case 0xCD: Addr_Abs(); CMP(); break;
		case 0xCE: Addr_Abs(); DEC(); break;
		case 0xCF: Addr_AbsLong(); CMP(); break;
		case 0xD0: Addr_Relative(); BNE(); break;
		case 0xD1: Addr_DirIndInxY(); CMP(); break;
		case 0xD2: Addr_DirInd(); CMP(); break;
		case 0xD3: Addr_StackRelativeIndInxY(); CMP(); break;
		case 0xD4: Addr_Dir(); PEI(); break;
		case 0xD5: Addr_DirInxX(); CMP(); break;
		case 0xD6: Addr_DirInxX(); DEC(); break;
		case 0xD7: Addr_DirIndLongInxY(); CMP(); break;
		case 0xD8: Addr_Imp(); CLD(); break;
		case 0xD9: Addr_AbsInxX(); CMP(); break;
		case 0xDA: PHX(); break;
		case 0xDB: Addr_Imp(); STP(); break;
		case 0xDC: Addr_AbsIndLong(); JML(); break;
		case 0xDD: Addr_AbsInxX(); CMP(); break;
		case 0xDE: Addr_AbsInxX(); DEC(); break;
		case 0xDF: Addr_AbsLongInxX(); CMP(); break;
		case 0xE0: Addr_ImmX(); CPX(); break;
		case 0xE1: Addr_DirInxIndX(); SBC(); break;
		case 0xE2: Addr_Imm8(); SEP(); break;
		case 0xE3: Addr_StackRelative(); SBC(); break;
		case 0xE4: Addr_Dir(); CPX(); break;
		case 0xE5: Addr_Dir(); SBC(); break;
		case 0xE6: Addr_Dir(); INC(); break;
		case 0xE7: Addr_DirIndLong(); SBC(); break;
		case 0xE8: Addr_Imp(); INX(); break;
		case 0xE9: Addr_ImmM(); SBC(); break;
		case 0xEA: Addr_Imp(); NOP(); break;
		case 0xEB: Addr_Imp(); XBA(); break;
		case 0xEC: Addr_Abs(); CPX(); break;
		case 0xED: Addr_Abs(); SBC(); break;
		case 0xEE: Addr_Abs(); INC(); break;
		case 0xEF: Addr_AbsLong(); SBC(); break;
		case 0xF0: Addr_Relative(); BEQ(); break;
		case 0xF1: Addr_DirIndInxY(); SBC(); break;
		case 0xF2: Addr_DirInd(); SBC(); break;
		case 0xF3: Addr_StackRelativeIndInxY(); SBC(); break;
		case 0xF4: Addr_Imm16(); PEA(); break;
		case 0xF5: Addr_DirInxX(); SBC(); break;
		case 0xF6: Addr_DirInxX(); INC(); break;
		case 0xF7: Addr_DirIndLongInxY(); SBC(); break;
		case 0xF8: Addr_Imp(); SED(); break;
		case 0xF9: Addr_AbsInxX(); SBC(); break;
		case 0xFA: PLX(); break;
		case 0xFB: Addr_Imp(); XCE(); break;
		case 0xFC: JSR_AbsInxIndX(); break;
		case 0xFD: Addr_AbsInxX(); SBC(); break;
		case 0xFE: Addr_AbsInxX(); INC(); break;
		case 0xFF: Addr_AbsLongInxX(); SBC(); break;
    }
}

CPU::~CPU() {}