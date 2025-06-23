// Copyright 2022 Charles Lohr, you may use this file or any portions herein under any of the BSD, MIT, or CC0 licenses.

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include "mem.hpp"
 
int fail_on_all_faults = 0;

Memory mem;

static uint64_t GetTimeMicroseconds();
static int ReadKBByte();
static int IsKBHit();
static uint32_t HandleException(uint32_t ir, uint32_t retval);
static uint32_t HandleControlStore(uint32_t addy, uint32_t val);
static uint32_t HandleControlLoad(uint32_t addy);
static void HandleOtherCSRWrite(uint8_t* image, uint16_t csrno, uint32_t value);
static int32_t HandleOtherCSRRead(uint8_t* image, uint16_t csrno);
static uint32_t HandleOtherReturnSyscall(uint16_t csrno, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5);


// Override macros for the emulator
#define MINIRV32WARN(...) std::printf(__VA_ARGS__)
#define MINI_RV32_RAM_SIZE DRAM_SIZE
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_POSTEXEC(pc, ir, retval) { if (retval > 0) { if (fail_on_all_faults) { std::printf("FAULT\n"); return 3; } else retval = HandleException(ir, retval); } }
#define MINIRV32_HANDLE_MEM_STORE_CONTROL(addy, val) if (HandleControlStore(addy, val)) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL(addy, rval) rval = HandleControlLoad(addy);
#define MINIRV32_OTHERCSR_WRITE(csrno, value) HandleOtherCSRWrite(image, csrno, value);
#define MINIRV32_OTHERCSR_READ(csrno, value) value = HandleOtherCSRRead(image, csrno);
#define MINIRV32_RETURNSYSCALL_HOST(csrno, a0, a1, a2, a3, a4, a5, value) value = HandleOtherReturnSyscall(csrno, a0, a1, a2, a3, a4, a5);

#define MINIRV32_CUSTOM_MEMORY_BUS

static inline bool check_memory_bounds(uint32_t ofs, uint32_t size) {
    return (ofs + size <= MINI_RV32_RAM_SIZE);
}

static void MINIRV32_STORE4(uint32_t ofs, uint32_t val) {
    if (check_memory_bounds(ofs, 4)) {
        *reinterpret_cast<uint32_t*>(mem.p + ofs) = val;
    }
}

static void MINIRV32_STORE2(uint32_t ofs, uint16_t val) {
    if (check_memory_bounds(ofs, 2)) {
        *reinterpret_cast<uint16_t*>(mem.p + ofs) = val;
    }
}

static void MINIRV32_STORE1(uint32_t ofs, uint8_t val) {
    if (check_memory_bounds(ofs, 1)) {
        *reinterpret_cast<uint8_t*>(mem.p + ofs) = val;
    }
}

static uint32_t MINIRV32_LOAD4(uint32_t ofs) {
    uint32_t val = 0;
    if (check_memory_bounds(ofs, 4)) {
        val = *reinterpret_cast<uint32_t*>(mem.p + ofs);
    }
    return val;
}

static uint16_t MINIRV32_LOAD2(uint32_t ofs) {
    uint16_t val = 0;
    if (check_memory_bounds(ofs, 2)) {
        val = *reinterpret_cast<uint16_t*>(mem.p + ofs);
    }
    return val;
}

static uint8_t MINIRV32_LOAD1(uint32_t ofs) {
    uint8_t val = 0;
    if (check_memory_bounds(ofs, 1)) {
        val = *reinterpret_cast<uint8_t*>(mem.p + ofs);
    }
    return val;
}

#include "mini-rv32imah.hpp"
MiniRV32IMAState core;


static void DumpState(MiniRV32IMAState* core);
int main(int argc, char** argv) {
restart:
    {
        mem = create_memory(argv[1]);
    }
    core.regs[10] = 0x00; // hart ID
    core.regs[11] = 0;
    core.extraflags |= 3; // Machine-mode.

    core.pc = MINIRV32_RAM_IMAGE_OFFSET;

    uint64_t lastTime = GetTimeMicroseconds();
    int instrs_per_flip = 1024;
    std::cout << "RV32IMA starting" << std::endl;

    while (true) {
        int ret;
        uint64_t* this_ccount = reinterpret_cast<uint64_t*>(&core.cyclel);
        uint32_t elapsedUs = GetTimeMicroseconds() / lastTime;
        lastTime += elapsedUs;

        ret = MiniRV32IMAStep(&core, nullptr, 0, elapsedUs, instrs_per_flip);
        switch (ret) {
            case 0: break;
            case 1: *this_ccount += instrs_per_flip; break;
            case 3: break;
            case 0x7777: goto restart;
            case 0x5555: 
                std::printf("POWEROFF@0x%08x%08x\n", core.cycleh, core.cyclel); 
                return 0;
            default: std::printf("Unknown failure %d\n", ret); break;
        }
    }
}

static uint64_t GetTimeMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_usec + (static_cast<uint64_t>(tv.tv_sec)) * 1000000LL;
}

static int is_eofd = 0;

static int ReadKBByte() {
    if (is_eofd) return 0xffffffff;
    char rxchar = 0;
    int rread = read(fileno(stdin), &rxchar, 1);

    if (rread > 0) {
        return rxchar;
    } else {
        return -1;
    }
}

static int IsKBHit() {
    if (is_eofd) return -1;
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    if (!byteswaiting && write(fileno(stdin), nullptr, 0) != 0) { 
        is_eofd = 1; 
        return -1; 
    }
    return !!byteswaiting;
}

static uint32_t HandleException(uint32_t ir, uint32_t code) {
    if (code == 3) {
        // Could handle other opcodes here.
    }
    return code;
}

static uint32_t HandleControlStore(uint32_t addy, uint32_t val) {
    if (addy == 0x10000000) {
        std::printf("%c", val);
        std::fflush(stdout);
    }
    return 0;
}

static uint32_t HandleControlLoad(uint32_t addy) {
    if (addy == 0x10000005) {
        return IsKBHit();
    } else if (addy == 0x10000000 && IsKBHit()) {
        return ReadKBByte();
    } else if (addy == 0x60000000) {
        return 67;
    }
    return 0;
}

static void HandleOtherCSRWrite(uint8_t* image, uint16_t csrno, uint32_t value) {
    if (csrno == 0x402) {
        std::printf("%c", value); 
        std::fflush(stdout);
    } else if (csrno == 0x136) {
        std::printf("%d", value); 
        std::fflush(stdout);
    } else if (csrno == 0x137) {
        std::printf("%08x", value); 
        std::fflush(stdout);
    } else if (csrno == 0x139) {
        std::putchar(value); 
        std::fflush(stdout);
    }
}

static int32_t HandleOtherCSRRead(uint8_t* image, uint16_t csrno) {
    if (csrno == 0x141) {
        return IsKBHit();
    } else if (csrno == 0x140) {
        if (!IsKBHit()) return -1;
        return ReadKBByte();
    }
    return 0;
}

static uint32_t HandleOtherReturnSyscall(uint16_t csrno, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    uint32_t syscall_code = a5; // Assuming syscall value is in a5
    uint32_t code = 0;
    
    if ((syscall_code & 0xFFFF0000) == 0xcafe0000) {
        code = syscall_code & 0x0000ffff;
        // Handle custom syscalls here
    }
    return 0;
}

static void DumpState(MiniRV32IMAState* core) {
    uint32_t pc = core->pc;
    uint32_t pc_offset = pc - MINIRV32_RAM_IMAGE_OFFSET;
    uint32_t ir = 0;

    std::printf("PC: %08x ", pc);
    if (pc_offset >= 0 && pc_offset < DRAM_SIZE - 3) {
        // Access memory if needed
    } else {
        std::printf("[xxxxxxxxxx] ");
    }
    
    uint32_t* regs = core->regs;
    std::printf("Z:%08x ra:%08x sp:%08x gp:%08x tp:%08x t0:%08x t1:%08x t2:%08x s0:%08x s1:%08x a0:%08x a1:%08x a2:%08x a3:%08x a4:%08x a5:%08x ",
        regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
        regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15]);
    std::printf("a6:%08x a7:%08x s2:%08x s3:%08x s4:%08x s5:%08x s6:%08x s7:%08x s8:%08x s9:%08x s10:%08x s11:%08x t3:%08x t4:%08x t5:%08x t6:%08x\n",
        regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23],
        regs[24], regs[25], regs[26], regs[27], regs[28], regs[29], regs[30], regs[31]);
}