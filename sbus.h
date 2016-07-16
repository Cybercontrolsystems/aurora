/*  Copyright 2004-2009, Technologic Systems
 *  All Rights Reserved.
 */

void sbus_poke16(unsigned int, unsigned short);
unsigned short sbus_peek16(unsigned int);
void sbuslock(void);
void sbusunlock(void);
void sbuspreempt(void); 
void setdiopin(int, int);
int getdiopin(int);
float gettemp(void);
