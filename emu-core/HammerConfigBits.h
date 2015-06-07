/*
	File:		HammerConfigBits.h
 
	Contains:	definitions of bits for Hammer’s Config and Tests menus
 
	Copyright:	© 1995 by Apple Computer, Inc., all rights reserved.
 
	Derived from v1 internal.
 
 */

#ifndef __HAMMERCONFIGBITS_H
#define __HAMMERCONFIGBITS_H

typedef enum {
  kCrippleMemory			= 0x00000001,		// bit 0		Extra Memory
  kEnableListener			= 0x00000002,		// bit 1		No Mac Memory
  kDisableSCP				= 0x00000004,		// bit 2		No Debugger Memory
  kConfigBit3				= 0x00000008,		// bit 3		Allow Low Memory Access
  kStopOnAborts			= 0x00000010,		// bit 4		Stop on Aborts
  kStopOnThrows			= 0x00000020,		// bit 5		Stop on Throws
  kHeapChecking			= 0x00000040,		// bit 6		Heap Checking
  kStackThrashing			= 0x00000080,		// bit 7		Stack Thrashing
  kOSProfiling			= 0x00000100,		// bit 8		OS Profiling
  kDefaultStdioOn			= 0x00000200,		// bit 9		Default Stdio on
  kDiagnostics			= 0x00000400,		// bit 10		Bunwarmer Diagnostics
  kConfigBit11			= 0x00000800,		// bit 11		Sound off
  kLCDContrastAdjust		= 0x00001000,		// bit 12		Old LCD Contrast
  kDontPauseCPU			= 0x00002000,		// bit 13		Don't Sleep
  kFakeBatteryLevel		= 0x00004000,		// bit 14		Fake Battery Level
  kEnableStdout			= 0x00008000,		// bit 15		Enable Stdout
  kEnablePackageSymbols		= 0x00010000,		// bit 16
  kEnablePCSpy			= 0x00020000,		// bit 17
  kEraseInternalStore		= 0x00040000,		// bit 18
  kConfigBit19			= 0x00080000,		// bit 19
  kLicenseeConfig0		= 0x00100000,		// bit 20 -- reserved for licensees
  kLicenseeConfig1		= 0x00200000,		// bit 21 -- reserved for licensees
  kLicenseeConfig2		= 0x00400000,		// bit 22 -- reserved for licensees
  kLicenseeConfig3		= 0x00800000,		// bit 23 -- reserved for licensees
  kConfigBit24			= 0x01000000,		// bit 24
  kConfigBit25			= 0x02000000,		// bit 25
  kConfigBit26			= 0x04000000,		// bit 26
  kConfigBit27			= 0x08000000,		// bit 27
  kConfigBit28			= 0x10000000,		// bit 28
  kConfigBit29			= 0x20000000,		// bit 29
  kConfigBit30			= 0x40000000,		// bit 30
  kConfigBit31			= 0x80000000,		// bit 31
} gNewtConfigBits;


typedef enum {
  kTestNetwork			= 0x00000001,		// bit 0
  kTestCommunications		= 0x00000002,		// bit 1
  kTestOS600			= 0x00000004,		// bit 2
  kTestNewtHardware		= 0x00000008,		// bit 3
  kTestPCMCIA			= 0x00000010,		// bit 4
  kTestTestUtilities		= 0x00000020,		// bit 5
  kTestBlackBird			= 0x00000040,		// bit 6
  kTestFlash			= 0x00000080,		// bit 7
  kTestRomDomainManager		= 0x00000100,		// bit 8
  kTestPackageManager		= 0x00000200,		// bit 9
  kTestSound			= 0x00000400,		// bit 10
  kTestAgent			= 0x00000800,		// bit 11
  kAvailDaemon			= 0x00001000,		// bit 12
  kTestHAL			= 0x00002000,		// bit 13
  kTestUtilityClasses		= 0x00004000,		// bit 14
  kTestBit15			= 0x00008000,		// bit 15
  kTestBit16			= 0x00010000,		// bit 16
  kTestBit17			= 0x00020000,		// bit 17
  kTestBit18			= 0x00040000,		// bit 18
  kTestBit19			= 0x00080000,		// bit 19
  kLicenseeTest0			= 0x00100000,		// bit 20 -- reserved for licensees
  kLicenseeTest1			= 0x00200000,		// bit 21 -- reserved for licensees
  kLicenseeTest2			= 0x00400000,		// bit 22 -- reserved for licensees
  kLicenseeTest3			= 0x00800000,		// bit 23 -- reserved for licensees
  kTestBit24			= 0x01000000,		// bit 24
  kTestBit25			= 0x02000000,		// bit 25
  kTestBit26			= 0x04000000,		// bit 26
  kTestBit27			= 0x08000000,		// bit 27
  kTestBit28			= 0x10000000,		// bit 28
  kTestBit29			= 0x20000000,		// bit 29
  kTestBit30			= 0x40000000,		// bit 30
  kTestBit31			= 0x80000000,		// bit 31
} gNewtTestsBits;

#endif  /* __HAMMERCONFIGBITS_H */

