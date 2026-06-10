#ifndef CPU_H
#define CPU_H
#include <stdint.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_log.h>
#include <vector>
#include "types.h"

using namespace std;

namespace ProcessorFlags {
	enum ProcessorFlags {
		Negative = 128,
        Overflow = 64,
        MemoryMode8 = 32,
        IndexMode8 = 16,
        Decimal = 8,
        Interrupt = 4,
        Zero = 2,
        Carry = 1
	};
};

enum class CPUStopState : u8 {
    Running = 0,
    Stopped = 1,
    WaitInterrupt = 2
};

class R16 {
    uint8_t *_h;
    uint8_t *_l;
public:
    R16(uint8_t *h, uint8_t *l) {
        _h = h;
        _l = l;
    }

    uint16_t Read() {
        return (*_h << 8) | *_l;
    };

    void Write(uint16_t val) {
        *_h = (uint8_t)(val >> 8);
        *_l = (uint8_t)val;
    }

    operator uint16_t() { return Read(); }

};

class Emulator;
struct CPUState {
    u16 A;
    u16 X;
	u16 Y;
    u16 PC;
    u16 D;
    u8 K;
    u8 DBR;
    u16 SP;
	u8 PSW;
    u64 cycleCount;

    bool irqLock;

    u8 irqSrc;

    u8 prevIrqSrc;

    u8 nmiFlagCounter;

    bool needNmi;

    bool inEmu;

    CPUStopState stopState;


}; 

class CPU {
    Emulator *_emu;

    CPUState state;

    vector<u8> _memory;

    bool _imm;
    bool _waitOver;

    u32 _operand;
    u32 _rwMask;

    void Write(u32, u8);
    void WriteWord(u32, u16);
    void WriteWordRMW(u32, u16);
    u8 Read(u32);
    u8 ReadData(u32 addr) {
        return Read(addr & _rwMask);
    }
    u16 ReadDataWord(u32 addr) {
        u8 l = ReadData(addr);
        u8 h = ReadData(addr + 1);
        return (h << 8) | l;
    }
    u8 ReadCode();
    u16 ReadWord(u32);
    u16 ReadCodeWord();
    u32 ReadCodeLong();
    u8 GetOpCode();
    void RunOpCode(u8);
    void BranchRel(bool br) {
        if (br) {
            s8 offset = _operand;
            if (state.inEmu && ((u16)(state.PC + offset) & 0xFF00) != (state.PC & 0xFF00)) {
                //extra cycle not implemented yet
            }
            state.PC = (u16)(state.PC + offset);
            //IdleTakeBranch not implemented yet
        }
    }



    u8 GetOperandByte() {
        if (_imm)
            return (u8)_operand;
        else
            return ReadData(_operand);
    }

    u16 GetOperandWord() {
        if (_imm)
            return (u16)_operand;
        else
            return ReadDataWord(_operand);
    }

    u16 GetDirAddr(u16 offset, bool allowEmu = true) {
        if (allowEmu && state.inEmu && (state.D & 0xFF) == 0)
            return (u16)((state.D & 0xFF00) | (offset & 0xFF));
        else 
            return (u16)(state.D + offset);
    }
    u16 GetDirAddrIndWord(u16 offset) {
        u8 l = ReadData(GetDirAddr(offset));
        u8 h = ReadData(GetDirAddr(offset));
        return (h << 8) | l;
    }

    u32 GetProgramAddr(u16 addr) {
        return (state.K << 16) | addr;
    }

    u32 GetDataAddr(u16 addr) {
        return (state.DBR << 16) | addr;
    }

    u16 GetDirAddrIndWordPW(u16 offset) {
        uint8_t l = ReadData(GetDirAddr(offset + 0));
        if (!state.inEmu || (state.D & 0xFF) == 0) {
            return (ReadData(GetDirAddr(offset + 1)) << 8) | l;
        } else {
            uint16_t addr = GetDirAddr(offset + 1);
            uint8_t h;
            if((addr & 0xFF) == 0) {
        		//Crossed to next page, wrap around to start of previous page instead (cpu bug?)
                h = ReadData((uint16_t)(addr - 0x100));
            } else {
                h = ReadData(addr);
            }
            return (h << 8) | l;
        }
    }

    u32 GetDirAddrIndLong(u16 offset){
	    u8 b1 = ReadData(GetDirAddr(offset + 0, false));
	    u8 b2 = ReadData(GetDirAddr(offset + 1, false));
	    u8 b3 = ReadData(GetDirAddr(offset + 2, false));
	    return (b3 << 16) | (b2 << 8) | b1;
    }

    //Addressing modes
    void Addr_DP() {
        if (state.inEmu)
            _operand = (u16)((state.D & 0xFF00) | (ReadCode() & 0xFF));
        else
            _operand = (u16)(state.D + ReadCode());
    }
    void Addr_Abs() {
        _operand = (state.DBR << 16) | ReadCodeWord();
    }
    void Addr_AbsInxX() {
        u32 baseAddr = (state.DBR << 16) | ReadCodeWord();
        _operand = (baseAddr + state.X) & 0xFFFFFF;
        //TODO: implement CPU cycles
    }
    void Addr_InxY() {
        u32 baseAddr = (state.DBR << 16) | ReadCodeWord();
        _operand = (baseAddr + state.Y) & 0xFFFFFF;
        //TODO: implement CPU cycles
    }
    void Addr_AbsLong() {
        _operand = ReadCodeLong();
    }
    void Addr_AbsLongInxX() {
        _operand = (ReadCodeLong() + state.X) & 0xFFFFFF;
    }
    void Addr_AbsJmp() {
        _operand = (state.K << 16) | ReadCodeWord();
    }
    void Addr_AbsLongJmp() {
        _operand = ReadCodeLong();
    }
    void Addr_AbsInd() {
        u16 addr = ReadCodeWord();
        u8 lb = ReadData(addr);
        u8 mb = ReadData((u16)(addr + 1));
        _operand = lb | (mb << 8);
    }
    void Addr_AbsIndLong() {
        u16 addr = ReadCodeWord();

        u8 b1 = ReadData(addr);
        u8 b2 = ReadData((u16)(addr + 1));
        u8 b3 = ReadData((u16)(addr + 2));

        _operand = b1 | (b2 << 8) | (b3 << 16);
    }
    void Addr_A() {
        if ((!state.irqLock && ((state.irqSrc != 0 || state.prevIrqSrc != 0) && !CheckFlag(ProcessorFlags::Interrupt))) || (state.nmiFlagCounter == 1 || state.needNmi))
            ReadCode();
    }
    void Addr_BlkMov() {
        _operand = ReadCodeWord();
    }
    void Addr_Dir() {
        _rwMask = 0xFFFF;
        _operand = GetDirAddr(ReadCode());
    }
    void Addr_DirInxX() {
        _rwMask = 0xFFFF;
        _operand = GetDirAddr(ReadCode() + state.X);
    }
    void Addr_DirInxY() {
        _rwMask = 0xFFFF;
        _operand = GetDirAddr(ReadCode() + state.Y);
    }
    void Addr_DirInd() {
        _operand = GetDataAddr(GetDirAddrIndWord(ReadCode()));
    }
    void Addr_DirInxIndX() {
        _operand = GetDataAddr(GetDirAddrIndWordPW(ReadCode() + state.X));
    }
    void Addr_DirIndInxY() {
        u32 baseAddr = GetDataAddr(GetDirAddrIndWord(ReadCode()));
        _operand = (baseAddr + state.Y) & 0xFFFFFF;
    }
    void Addr_DirIndLong() {
        _operand = GetDirAddrIndLong(ReadCode());
    }
    void Addr_DirIndLongInxY() {
        _operand = (GetDirAddrIndLong(ReadCode()) + state.Y) & 0xFFFFFF;
    }
    void Addr_Imm8() {
        _imm = true;
        _operand = ReadCode();
    }
    void Addr_Imm16() {
        _imm = true;
        _operand = ReadCodeWord();
    }
    void Addr_ImmX() {
        _imm = true;
        _operand = CheckFlag(ProcessorFlags::IndexMode8) ? ReadCode() : ReadCodeWord();
    }
    void Addr_ImmM() {
        _imm = true;
        _operand = CheckFlag(ProcessorFlags::MemoryMode8) ? ReadCode() : ReadCodeWord();
    }
    void Addr_Imp() {
        if ((!state.irqLock && ((state.irqSrc != 0 || state.prevIrqSrc != 0) && !CheckFlag(ProcessorFlags::Interrupt))) || (state.nmiFlagCounter == 1 || state.needNmi))
            ReadCode();
    }
    void Addr_RelativeLong() {
        _operand = ReadCodeWord();
    }
    void Addr_Relative() {
        _operand = ReadCode();
    }
    void Addr_StackRelative() {
        _operand = (u16)(ReadCode() + state.SP);
    }
    void Addr_StackRelativeIndInxY() {
        u16 addr = (u16)(ReadCode() + state.SP);
        u8 l = ReadData(addr);
        u8 h = ReadData((addr + 1) & 0xFFFF);
        _operand = (GetDataAddr(l | (h << 8)) + state.Y) & 0xFFFFFF;
    }

    void Add8(u8);
    void Add16(u16);

    void Sub8(u8);
    void Sub16(u16);
    //Instructions (92 of them)
    void ADC();

    void SBC();

    void BCC();
    void BCS();
    void BEQ();
    void BMI();
    void BNE();
    void BPL();
    void BRA();
    void BRL();
    void BVC();
    void BVS();

    void CLC();
    void CLD();
    void CLI();
    void CLV();
    void SEC();
    void SED();
    void SEI();

    void REP();
    void SEP();

    void DEX();
    void DEY();
    void INX();
    void INY();
    void DEC();
    void INC();

    void DEC_A();
    void INC_A();

    void Compare(u16, bool);
    void CMP();
    void CPX();
    void CPY();

    void JML();
    void JMP();
    void JSL();
    void JSR();
    void RTI();
    void RTL();
    void RTS();

    void JMP_AbsInxIndX();
    void JSR_AbsInxIndX();

    void ProcessInterrupt(u16 vector, bool forHWI) {
        if (forHWI) {
            ReadCode();
            //Idle();
        }
        if (state.inEmu) {
            PushWord(state.PC);
            PushByte(state.PSW | 0x20);
            SetFlag(ProcessorFlags::Interrupt);
            ClearFlag(ProcessorFlags::Decimal);
            state.K = 0;
            state.PC = ReadDataWord(vector);
        } else {
            PushByte(state.K);
            PushWord(state.PC);
            PushByte(state.PSW);
            SetFlag(ProcessorFlags::Interrupt);
            ClearFlag(ProcessorFlags::Decimal);
            state.K = 0;
            state.PC = ReadDataWord(vector);
        }
    }

    void BRK();
    void COP();

    void AND();
    void EOR();
    void ORA();

    template<typename T> T SLeft(T);
    template<typename T> T RLeft(T);
    template<typename T> T SRight(T);
    template<typename T> T RRight(T);

    void ASL_A();
    void ASL();
    void LSR_A();
    void LSR();
    void ROL_A();
    void ROL();
    void ROR_A();
    void ROR();

    void MVN();
    void MVP();

    void PEA();
    void PEI();
    void PER();
    void PHB();
    void PHD();
    void PHK();
    void PHP();
    void PLB();
    void PLD();
    void PLP();

    void PHA();
    void PHX();
    void PHY();
    void PLA();
    void PLX();
    void PLY();

    void PushReg(u16, bool);
    void PullReg(u16 &, bool);

    void LoadReg(u16 &, bool);
    void StoreReg(u16, bool);
    void SetReg(u16 &, u16, bool);
    void SetReg(u8 &, u8);

    void LDA();
    void LDX();
    void LDY();

    void STA();
    void STX();
    void STY();
    void STZ();

    template<typename T> void TestBits(T, bool);
    void BIT();

    void TRB();
    void TSB();

    void TAX();
    void TAY();
    void TCD();
    void TCS();
    void TDC();
    void TSC();
    void TSX();
    void TXA();
    void TXS();
    void TXY();
    void TYA();
    void TYX();
    void XBA();
    void XCE();

    void NOP();
    void WDM();

    void STP();
    void WAI();

    void CheckForInterrupts();
    void ProcessHaltedState();


public:
    vector<u8> *GetMemory() { return &_memory; }
    vector<u8> &GetMemoryRef() { return _memory; }
    CPUState &GetState() { return state; }
    void PushByte(u8 value, bool = true);
	void PushWord(u16 value, bool = true);
    void SetSP(u16 value, bool allowEmu = true) {
        if (allowEmu && state.inEmu)
            state.SP = 0x100 | (value & 0xFF);
        else
            state.SP = value;
    }
    void RestrictSP() {
        if (state.inEmu) {
            state.SP = 0x100 | (state.SP & 0xFF);
        }
    }
    void SetPS(u8 ps) {
        state.PSW = ps;
        if (CheckFlag(ProcessorFlags::IndexMode8)) {
            state.Y &= 0xFF;
            state.X &= 0xFF;
        }
    }

    u8 PopByte(bool = true);
	u16 PopWord(bool = true);

    void SetA(u8 value) { state.A = value; };
    u8 A() { return state.A; };
    void SetFlag(u8 flag) { state.PSW |= flag; }
    bool CheckFlag(u8 flag) { return (state.PSW & flag) == flag; }
    void ClearFlag(u8 flag) { state.PSW &= ~flag; }
    void SetZN(u8 val) {
        ClearFlag(ProcessorFlags::Zero | ProcessorFlags::Negative);
        if (val == 0) SetFlag(ProcessorFlags::Zero);
        else if (val & 0x80) SetFlag(ProcessorFlags::Negative);
    }
    void SetZN(u16 val) {
        ClearFlag(ProcessorFlags::Zero | ProcessorFlags::Negative);
        if (val == 0) SetFlag(ProcessorFlags::Zero);
        else if (val & 0x8000) SetFlag(ProcessorFlags::Negative);
    }
    void Reset() {
        state.A = 0;
        state.X = 0;
        state.Y = 0;
        state.PC = 0x8000;
        state.SP = 0x0000;
        state.PSW = 0;
        state.cycleCount = 0;
        _memory.clear();
    }

    CPU(Emulator *);
    void Exec();
    ~CPU();
};

#define MEM8 CheckFlag(ProcessorFlags::MemoryMode8)
#define INDEX8 CheckFlag(ProcessorFlags::IndexMode8)

#endif