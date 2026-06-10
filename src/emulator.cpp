#include "emulator.h"
#include "window.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "types.h"
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define RESET "\x1B[0m"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 

Emulator::Emulator() : ready(false), started(false), _halt(false) { _cpu = new CPU(this); }

void Emulator::SetWindowErrMsg(string msg)  { _win->SetErrorMessage(msg); } 

void Emulator::SetRomFile(string to) {
    _romFile = to;
    if (_romFile == "") return;
    LoadRom();
}

void OnFileOpen(void *userdata, const char * const *filelist, int filter) {
    if (!filelist) {
        SDL_Log("An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        SDL_Log("The user did not select any file.");
        SDL_Log("Most likely, the dialog was canceled.");
        return;
    }

    SDL_Log("Got file %s\n", *filelist);
    ((Emulator *)userdata)->SetRomFile(std::string(*filelist));
}

void Emulator::promptForFile(SDL_Window *win) {
    const SDL_DialogFileFilter filters[] = {
        { "Super Nintendo ROMs",  "sfc" },
    };
    SDL_ShowOpenFileDialog(OnFileOpen, this, win, filters, 1, nullptr, false);
}

void Emulator::LoadRom() {
    const char *filename = _romFile.c_str();
    ifstream file(filename, ios::in | ios::binary);
    
    vector<u8> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    _cpu->GetMemoryRef() = data;
    CalculateChecksum();
}

void Emulator::CalculateChecksum() {
    u16 cs = 0;
    vector<u8> &mem = _cpu->GetMemoryRef();
    for (int i = 0; i < mem.size(); i++) {
        cs = (cs + mem[i]) & 0xFFFF;
    }
    if (mem[0x7FDE] == (u8)cs && mem[0x7FDF] == (cs >> 8)) {
        headerIndex = 0x7FC0;
        ready = true;
    } else if (mem[0xFFDE] == (u8)cs && mem[0xFFDF] == (cs >> 8)) {
        headerIndex = 0xFFC0;
        ready = true;
    }
    //set header info in struct
    for (int i = headerIndex; i < headerIndex + 21; i++) {
        _header.title += (char)(mem[i]);
    }
    _header.map = (mem[headerIndex + 21] & 0x0F);
    _header.speed = (mem[headerIndex + 21] >> 4) & 1;
    _header.chipset = mem[headerIndex + 22];
    _header.romSize = 1 << mem[headerIndex + 23]; //in kb
    _header.ramSize = 1 << mem[headerIndex + 24]; //in kb
    _header.countryCode = mem[headerIndex + 25];
    switch(_header.countryCode) {
        case 0x00: //JPN
        case 0x01: //North America
            _header.ntsc = true; 
            break;
        default:
            _header.ntsc = false;
            break;
    }
    _header.devId = mem[headerIndex + 26];
    _header.romVer = mem[headerIndex + 27];
    _header.csComp = cs ^ 0xFFFF;
    _header.cs = cs;
    for (int i = 0; i < 32; i++) {
        _header.interruptVectors.push_back(mem[headerIndex + 32 + i]);
    }
    rotate(mem.rbegin(), mem.rbegin() + 0x8000, mem.rend());
    printf("00: ");
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) printf(" ");
        printf("%02x", mem[0x8000 + i]);
    }
    printf("\n");
    ready = true;
}

void Emulator::Start() {
    if (started) return;
    if (!ready) return;
    started = true;
    _cpu->GetState().PC = 0x8000;
    _cpu->GetState().SP = 0x01FF;
    _emuThread.reset(new thread(&Emulator::Run, this));
}

void Emulator::Run() {
    //#define SNES_DEBUG
    while (true) {
        if (_halt) {
            _win->ShowErrorWindow();
            //print memory
            CPUState state = _cpu->GetState();
            for (size_t i = state.PC - 0x20; i < state.PC + 0x22; i++) {
                if (i % 16 == 0) printf("\n%04zX:  ", i);
                else if (i % 4 == 0) printf(" ");
                vector<u8> *mem = _cpu->GetMemory();
                if (i == state.PC) printf(GRN "%02X" RESET, (unsigned int)((*mem)[i]));
                else printf("%02X", (unsigned int)((*mem)[i]));
            }
            printf("\n");
            started = false;
            ready = false;
            _cpu->Reset();
            SetRomFile("");
            return;
        }
#ifdef SNES_DEBUG
        if (doStep) {
#endif
            _cpu->Exec();
#ifdef SNES_DEBUG
            doStep = false;
        }
#endif
    }
}

Emulator::~Emulator() {
    delete _cpu;
}