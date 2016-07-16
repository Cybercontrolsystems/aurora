/*
 *  aurora.h
 *  Aurora
 *
 *  Created by Martin Robinson on 02/12/2010.
 *  Copyright 2010 Naturalwatt. All rights reserved.
 *
 */

#define PROGNAME "Aurora"
const char progname[] = PROGNAME;
#define LOGFILE "/tmp/aurora%d.log"
// #define LOGON "diehl"
#define BAUD B19200
#define MAXINVERTERS 2

/*  1.1 07/10/2010 Initial version copied from Diehl 2.4 (via Evoco)
	1.2 10/02/2011 Handles RS485 - the 8 byte message is echoed back
	1.3 12/09/2011 Added -o offset field
 
 */

typedef unsigned short u16;
typedef unsigned char uchar;

// See Aurora.c for proper revision info
// "@(#)$Id: aurora.h,v 1.2 2011/09/12 14:16:54 martin Exp $";
// REVISION "$Revision: 1.2 $"

#define PORTNO 10010
#define SERIALNAME "/dev/ttyAM0"	/* although it MUST be supplied on command line */

// Severity levels.  ERROR and FATAL terminate program
#define INFO	0
#define	WARN	1
#define	ERROR	2
#define	FATAL	3
// Socket retry params
#define NUMRETRIES 3
int numretries = NUMRETRIES;
#define RETRYDELAY	1000000	/* microseconds */
int retrydelay = RETRYDELAY;
// Elster values expected
#define NUMPARAMS 15
// Set to if(0) to disable debugging
#define DEBUGFP stderr   /* set to stderr or logfp as required */
// Serial retry params
#define SERIALNUMRETRIES 10
#define SERIALRETRYDELAY 1000000 /*microseconds */
// If defined, use stdin
// #define DEBUGCOMMS
#define INITFCS 0xFFFF /* Initial FCS value */
#define GOODFCS 0xF0B8 /* Good final FCS */

short cmdList[] = {		// Cycle round this.
	0x3b01,		// Volts AC
	0x3b02,		// Amps AC
	0x3b03,		// Watts AC
	0x3b04,		// Hz
	0x3b08,		// Watts DC
	0x3b15,		// Internal Temp
	0x3b17,		// DC Input 1 Volts
	0x3b19,		// DC Input 1 Amps
	0x3200,		// Status
	0x4e05,		// Wh	
	0					// Termination	
};

struct status {
	int code;	// 0 = INFO, 1 = WARN, 2 = ERROR
	char *message;
};

struct status global_status[] = {
	/* 0  */	INFO, "Sending Params",
	/* 1  */	INFO ,"Waiting Sun",
	/* 2  */	INFO, "Checking Grid",
	/* 3  */	INFO, "Measuring RISO",
	/* 4  */	INFO, "DcDc Start",
	/* 5  */	INFO, "Inverter Start",
	/* 6  */	INFO, "Run",
	/* 7  */	INFO, "Recovery",
	/* 8  */	INFO, "Pause",
	/* 9  */	ERROR,"Ground Fault",
	/* 10 */	ERROR,"OTH Fault",
	/* 11 */	WARN, "Address Setting",
	/* 12 */	INFO, "Self Test",
	/* 13 */	ERROR,"Self Test FAIL",
	/* 14 */	INFO, "Sensor Test",
	/* 15 */	ERROR,"Leak Fault",
	/* 16 */	ERROR,"Wait for manual Reset",
	/* 17 */	ERROR,"E026",
	/* 18 */	ERROR,"E027",
	/* 19 */	ERROR,"E028",
	/* 20 */	ERROR,"E029",
	/* 21 */	ERROR,"E030",
	/* 22 */	WARN, "Sending Wind Table",
	/* 23 */	ERROR,"Failed sending wind table",
	/* 24 */	ERROR,"UTH Fault",
	/* 25 */	INFO, "Remote OFF",
	/* 26 */	ERROR,"Interlock Fail",
	/* 27 */	INFO, "Selftest",
	/* 28 */	INFO, "Selftest 1",
	/* 29 */	INFO, "Selftest 2",
	/* 30 */	INFO, "Waiting Sun",
	/* 31 */	ERROR,"temperature Fault",
	/* 32 */	ERROR,"Fan stuck",
	/* 33 */	ERROR,"Int.Comm.Fault",
	/* 34 */	WARN, "Slave Insertion",
	/* 35 */	WARN, "DC Switch Open",
	/* 36 */	WARN, "TRAS Switch open",
	/* 37 */	WARN, "Master Exclusion",
	/* 38 */	WARN, "Auto Exclusion",
				WARN, "UNKNOWN"};		// 1.2: Downgrade from ERROR
#define MAXGLOBAL 39

struct status dcdc_status[] = {
	/* 0  */	INFO, "DcDc Off",
	/* 1  */	INFO, "Ramp Start",
	/* 2  */	INFO, "MPPT",
	/* 3  */	ERROR, "NOT USED (3)",
	/* 4  */	WARN, "Input OC",
	/* 5  */	INFO, "Input UV",
	/* 6  */	WARN, "Input OV",
	/* 7  */	INFO, "Input Low",
	/* 8  */	WARN, "No Parameters",		// 1.2: Downgrade from ERROR 
	/* 9  */	ERROR, "Bulk OV",
	/* 10 */	ERROR, "Comms Error",
	/* 11 */	ERROR, "Ramp Fail",
	/* 12 */	ERROR, "Internal Error",
	/* §3 */	ERROR, "Input mode error",
	/* 14 */	ERROR, "Ground Fault",
	/* 15 */	ERROR, "Inverter Fail",
	/* 16 */	ERROR, "DcDc IGBT Fail",
	/* 17 */	ERROR, "DcDc ILeak Fail",
	/* 18 */	ERROR, "DcDc Grid Fail",
	/* 19 */	ERROR, "DcDc Comms Error",
				ERROR, "UNKNOWN"};
#define MAXDCDC 20

struct status inverter_status[] = {
	/* 0  */	INFO, "Stand By",
	/* 1  */	INFO, "Checking Grid",
	/* 2  */	INFO, "Run",
	/* 3  */	WARN, "Bulk OV",
	/* 4  */	WARN, "Output OC",
	/* 5  */	WARN, "IGBT Sat",
	/* 6  */	WARN, "Bulk UV",
	/* 7  */	ERROR,"Degauss Error",
	/* 8  */	WARN, "No Parameters",
	/* 9  */	WARN, "Bulk Low",
	/* 10 */	WARN, "Grid OV",
	/* 11 */	ERROR, "Comms Error",
	/* 12 */	WARN, "Degaussing",
	/* 13 */	INFO, "Starting",
	/* 14 */	ERROR, "Bulk Cap Fail",
	/* 15 */	ERROR, "Leak Fail",
	/* 16 */	WARN, "DCDC Fail",			// 1.2: Downgrade from ERROR
	/* 17 */	ERROR, "Ileak sensor Fail",
	/* 18 */	INFO, "Selftest: relay inverter",
	/* 19 */	INFO, "Selftest: wait for sensor test",
	/* 20 */	INFO, "Selftest: test DcDc relay",
	/* 21 */	ERROR, "Selftest: relay inverter FAIL",
	/* 22 */	ERROR, "Selftest: timeout FAIL",
	/* 23 */	ERROR, "Selftest: relay DcDC FAIL",
	/* 24 */	INFO, "Selftest 1",
	/* 25 */	INFO, "Waiting to start selftest",
	/* 26 */	INFO, "DC Injection",
	/* 27 */	INFO, "Selftest 2",
	/* 28 */	INFO, "Selftest 3",
	/* 29 */	INFO, "Selftest 4",
	/* 30 */	ERROR, "Internal Error",
	/* 31 */	ERROR, "Internal Error",
	/* 32 */	ERROR, "Forbidden State",
	/* 33 */	ERROR, "Forbidden State",
	/* 34 */	ERROR, "Forbidden State",
	/* 35 */	ERROR, "Forbidden State",
	/* 36 */	ERROR, "Forbidden State",
	/* 37 */	ERROR, "Forbidden State",
	/* 38 */	ERROR, "Forbidden State",
	/* 39 */	ERROR, "Forbidden State",
	/* 40 */	ERROR, "Forbidden State",
	/* 41 */	ERROR, "Input UC",
	/* 42 */	ERROR, "Zero Power",
	/* 43 */	ERROR, "Grid Not Present",
	/* 44 */	INFO, "Waiting start",
	/* 45 */	INFO, "MPPT",
	/* 46 */	WARN, "Grid Fail",
	/* 47 */	WARN, "Input OC",
				ERROR, "UNKNOWN" };
#define MAXINVERTERSTATUS 48
	
struct status alarm_status[] = {
	/* 0  */	INFO, "No Alarm",
	/* 1  */	WARN, "Sun Low",
	/* 2  */	ERROR, "Input OC",
	/* 3  */	WARN, "Input UV",
	/* 4  */	ERROR, "Input OV",
	/* 5  */	WARN, "Sun Low",
	/* 6  */	ERROR, "No Parameters",
	/* 7  */	ERROR, "Bulk OV",
	/* 8  */	ERROR, "Comms Error",
	/* 9  */	ERROR, "Output OC",
	/* 10 */	ERROR, "IGBT Sat",
	/* 11 */	WARN, "Bulk UV",
	/* 12 */	ERROR, "Internal Error",
	/* 13 */	WARN, "Grid Fail",
	/* 14 */	ERROR, "Bulk Low",
	/* 15 */	ERROR, "RAmp FAIL",
	/* 16 */	ERROR, "DcDc Fail",
	/* 17 */	ERROR, "Wrong Mode",
	/* 18 */	ERROR, "GRound Fault",
	/* 19 */	ERROR, "Over Temp",
	/* 20 */	ERROR, "Bulk Cap Fail",
	/* 21 */	ERROR, "Inverter Fail",
	/* 22 */	ERROR, "Start Timeout",
	/* 23 */	ERROR, "Ground Fault",
	/* 24 */	ERROR, "Degauss Error",
	/* 25 */	ERROR, "Ileak sensor FAIL",
	/* 26 */	ERROR, "DcDc FAIL",
	/* 27 */	ERROR, "Selftest Error 1",
	/* 28 */	ERROR, "Selftest Error 2",
	/* 29 */	ERROR, "Selftest Error 3",
	/* 30 */	ERROR, "Selftest Error 4",
	/* 31 */	ERROR, "DC Inj Error",
	/* 32 */	WARN, "Grid OV",
	/* 33 */	WARN, "Grid UV",
	/* 34 */	WARN, "Grid OF",
	/* 35 */	WARN, "Grid UF",
	/* 36 */	WARN, "Z Grid Hi",
	/* 37 */	ERROR, "Internal Error",
	/* 38 */	ERROR, "Riso Low",
	/* 39 */	ERROR, "Vref Error",
	/* 40 */	ERROR, "Error Meas. V",
	/* 41 */	ERROR, "Error Meas. F",
	/* 42 */	ERROR, "Error Meas. Z",
	/* 43 */	ERROR, "Error Meas ILeak",
	/* 44 */	ERROR, "Error Read V",
	/* 45 */	ERROR, "Error Read I",
	/* 46 */	WARN, "Table Fail",
	/* 47 */	WARN, "Fan Fail",
	/* 48 */	ERROR, "UTH",
	/* 49 */	ERROR, "Interlock Fail",
	/* 50 */	ERROR, "Remote OFF",
	/* 51 */	ERROR, "Vout Avg Error",
	/* 52 */	WARN, "Battery Low",
	/* 53 */	WARN, "Clock FAIL",
	/* 54 */	WARN, "Input UC",
	/* 55 */	WARN, "Zero Power",
	/* 56 */	ERROR, "Fan Stuck",
	/* 57 */	ERROR, "DC Switch Open",
	/* 58 */	ERROR, "Tras Switch Open",
	/* 59 */	ERROR, "AC Switch Open",
	/* 60 */	ERROR, "BULK UV",
	/* 61 */	ERROR, "Auto Exclusion",
	/* 62 */	WARN, "Grid delta F",
	/* 63 */	WARN, "Den Switch Open",
	/* 64 */	WARN, "Jbox Fail",
				ERROR, "UNKNOWN"};
#define MAXALARM 65
		
static u16 fcstab[256] = {
	0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
	0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
	0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
	0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
	0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
	0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
	0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
	0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
	0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
	0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
	0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
	0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
	0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
	0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
	0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
	0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
	0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
	0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
	0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
	0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
	0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
	0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
	0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
	0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
	0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
	0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
	0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
	0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
	0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
	0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
	0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
	0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};
