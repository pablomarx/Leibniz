#ifndef __FPMODULE_INL
#define __FPMODULE_INL

unsigned int readRegister(const unsigned int nReg);
void writeRegister(const unsigned int nReg, const unsigned int val);
unsigned int readCPSR(void);
void writeCPSR(const unsigned int val);
unsigned int readConditionCodes(void);
void writeConditionCodes(const unsigned int val);
unsigned int readMode(void);
void get_user(unsigned int *val, const unsigned int *addr);
void put_user(unsigned int val, unsigned int *addr);
void sti(void);
void save_flags(unsigned int flags);
void restore_flags(unsigned int flags);


#endif