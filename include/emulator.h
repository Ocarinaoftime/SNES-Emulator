#ifndef EMU_H
#define EMU_H
#include "cpu.h"
#include "ppu.h"
#include <string>
#include <vector>
#include <cstdint>
#include <thread>

class Window;

using namespace std;

class Emulator {
private:
    CPU *_cpu;
    //PPU *_ppu;
    //APU *_apu;
    bool ready;
    bool started;
    string _romFile;
    unique_ptr<thread> _emuThread;
    uint8_t _buttons;
    bool doStep;
    Window *_win;
    bool _halt;
    u32 headerIndex;
    struct Header {
        string title;
        u8 map;
        u8 speed;
        u8 chipset;
        u64 romSize;
        u64 ramSize;
        u8 countryCode;
        u8 devId;
        u8 romVer;
        u16 csComp;
        u16 cs;
        vector<u8> interruptVectors;
        bool ntsc;
    } _header;
    void LoadRom();
    void CalculateChecksum();
public:
    Emulator();
    void Start();
    void Run();
    void SetRomFile(string);
    bool hasRom() { return !_romFile.empty(); };
    bool isReady() { return ready; }
    bool hasStarted() { return started; }
    void promptForFile(SDL_Window *);
    void SetButtons(uint8_t buttons) { _buttons |= (1 << buttons); }
    void SetStep(bool t) { doStep = t; }
    void SetWindow(Window *to) { _win = to; };
    Window *GetWindow() { return _win; }
    CPU *GetCPU() { return _cpu; }
    void SetWindowErrMsg(string msg);
    void SetHalted() { _halt = true; } 
    ~Emulator();
};

#endif