/* Aurora Power-One Interface program 
 *  Created by Martin Robinson on 02/12/2010.
 *  Copyright 2010 Naturalwatt. All rights reserved.
 *
 */

#include <stdio.h>	// for FILE
#include <stdlib.h>	// for timeval
#include <string.h>	// for strlen etc
#include <time.h>	// for ctime
#include <sys/types.h>	// for fd_set
// #include <sys/socket.h>
// #include <netinet/in.h>
#include <netdb.h>	// for sockaddr_in 
#include <fcntl.h>	// for O_RDWR
#include <termios.h>	// for termios
#include <unistd.h>		// for getopt
#ifdef linux
#include <errno.h>		// for Linux
#endif
#include <sys/mman.h>	// for PROT_READ
#include <sys/stat.h>	// for struct stat
#include <getopt.h>
#include <signal.h>

#include "../Common/common.h"
#include "aurora.h"

static char* id="@(#)$Id: aurora.c,v 1.3 2011/09/12 14:16:54 martin Exp $";
#define REVISION "$Revision: 1.3 $"

#ifndef linux
extern
#endif
int errno;  

// Procedures in this file
int processSocket(void);			// process server message
void usage(void);					// standard usage message
char * getversion(void);
int getbuf(int fd, int max);	// get a buffer full of message
time_t timeMod(time_t t);
void dumpbuf();		// Dump out a buffer for debug
void sendCommand(int fd, int addr, int cmd);
int synctostart(int fd);		// wait for 0xAA in the input. 0 == success.
int isPort(char *);				// Decide if connection to hostname:port
void processPacket(int currentInv);			// Output it all
void stateMessage(int state, int global, int inverter);
int detect(int commfd, int maxinverters);
u16 fcs16(u16 fcs, uchar * cp, int len);
void catcher(int sig);

/* GLOBALS */
FILE * logfp = NULL;
int sockfd[MAXINVERTERS];	// Because openSockets expects an array.
int inverter[MAXINVERTERS];	// The address of inverter[i]
int numinverters;
int debug = 0;
int noserver = 0;		// prevents socket connection when set to 1
float offset = 0.0;		// Added to kwh

// Common Serial Framework
#define BUFSIZE 128	/* should be longer than max possible line of text */
struct data {	// The serial buffer
	int count;
	unsigned char buf[BUFSIZE];
	int escape;		// Count the escapes in this message
} data;

unsigned char sendBuf[60];
int controllernum = -1;	//	only used in logon message

#define debugfp stderr
char buffer[256];		// For logmsg strings
char * serialName = SERIALNAME;

int currentCmd = 0;

/********/
/* MAIN */
/********/
int main(int argc, char *argv[])
// arg1: serial device file
// arg2: optional timeout in seconds, default 60
{
    int commfd;
	int maxinverters = 1;
	int currentInv;
	int nolog = 0;
	time_t next;
	int address = 0;
	
	int run = 1;		// set to 0 to stop main loop
	fd_set readfd, errorfd; 
	int numfds;
	struct timeval timeout;
	int tmout = 10;
	int logerror = 0;
	int online = 1;		// used to prevent messages every minute in the event of disconnection
	int option; 
	int test = 0;		// Single value to read from inverter
	// Command line arguments
	bzero(data.buf, BUFSIZE);
	data.count = 0;
	opterr = 0;
	
	while ((option = getopt(argc, argv, "sl?dVZn:No:a:t:")) != -1) {
		switch (option) {
			case 's': noserver = 1; break;
			case 'l': nolog = 1; break;
			case '?': usage(); exit(1);
			case 'd': debug++; break;
			case 'n': maxinverters = atoi(optarg); break;
			case 'V': printf("Version %s %s\n", getversion(), id); exit(0);
			case 'N': break;	// Ignore it.
			case 'o': offset = strtod(optarg, NULL); break;
			case 'a': address = atoi(optarg); break;
			case 't': test = strtol(optarg, NULL, 0); break;
			case 'Z': decode("(b+#Gjv~z`mcx-@ndd`rxbwcl9Vox=,/\x10\x17\x0e\x11\x14\x15\x11\x0b\x1a" 
							 "\x19\x1a\x13\x0cx@NEEZ\\F\\ER\\\x19YTLDWQ'a-1d()#!/#(-9' >q\"!;=?51-??r"); exit(0);
		}
	}
	
	DEBUG printf("Debug on %d. optind %d argc %d offset %f\n", debug, optind, argc, offset);
	
	if (optind < argc) serialName = argv[optind];		// get seria/device name: parameter 1
	optind++;
	if (optind < argc) controllernum = atoi(argv[optind]);	// get optional controller number: parameter 2
	
	sprintf(buffer, LOGFILE, controllernum);
	
	if (!nolog) if ((logfp = fopen(buffer, "a")) == NULL) logerror = errno;	
	
	// There is no point in logging the failure to open the logfile
	// to the logfile, and the socket is not yet open.
	
	sprintf(buffer, "STARTED %s on %s as %d timeout %d %s", argv[0], serialName, controllernum, tmout, nolog ? "nolog" : "");
	logmsg(INFO, buffer);
	
	openSockets(0, maxinverters, "inverter", REVISION, PROGNAME, 0);
	
	if (test) {	// Decode the status value
		stateMessage(test, 0, 0);
		return 0;
	}
	
	// Open serial port
	if ((commfd = openSerial(serialName, BAUD, 0, CS8, 1)) < 0) {
		sprintf(buffer, "FATAL " PROGNAME " %d Failed to open %s: %s", controllernum, serialName, strerror(errno));
#ifdef DEBUGCOMMS
		logmsg(INFO, buffer);			// FIXME AFTER TEST
		fprintf(stderr, "Using stdio\n");
		commfd = 0;		// use stdin
#else
		logmsg(FATAL, buffer);
#endif
	}
	
#ifndef DEBUGCOMMS
	if (flock(commfd, LOCK_EX | LOCK_NB) == -1) {
		sprintf(buffer, "FATAL " PROGNAME " is already running, cannot start another one on %s", serialName);
		logmsg(FATAL, buffer);
	}
#endif
	
	// If we failed to open the logfile and were NOT called with nolog, warn server
	// Obviously don't use logmsg!
	if (logfp == NULL && nolog == 0) {
		sprintf(buffer, "event WARN " PROGNAME " %d could not open logfile %s: %s", controllernum, LOGFILE, strerror(logerror));
		sockSend(sockfd[0], buffer);
	}
	
	numfds = (sockfd[0] > commfd ? sockfd[0] : commfd) + 1;		// nfds parameter to select. One more than highest descriptor
	
	// Start with detecting inverters
	if (! address)
		while (detect(commfd, maxinverters)) {
			DEBUG fprintf(stderr,"Detect failed .. pause 60 sec\n");
			sleep(60);
		}
	else {
		numinverters = 1;
		inverter[0] = address;
	}
	
	// Main Loop
	FD_ZERO(&readfd); 
	FD_ZERO(&errorfd); 
	
	next = 0;	// so we always get one at startup.
	DEBUG2 fprintf(stderr, "Now is %zd next is %zd \n", time(NULL), next);
	if (offset != 0.0) {
		sprintf(buffer, "INFO " PROGNAME " %d Using offset %f", controllernum, offset);
		logmsg(INFO, buffer);
	}
	
	signal(SIGKILL, catcher);
	signal(SIGINT, catcher);
	signal(SIGTERM, catcher);
	currentInv = 0;
	currentCmd = 0;
	while(run) {
		timeout.tv_sec = tmout;
		timeout.tv_usec = 0;
		FD_SET(sockfd[0], &readfd);
		FD_SET(commfd, &readfd);
		FD_SET(sockfd[0], &errorfd);
		FD_SET(commfd, &errorfd);
		DEBUG2 fprintf(stderr, "Before: Readfd %lx ", readfd);
		DEBUG2 fprintf(stderr, "ErrorFD %lx ", errorfd);
		blinkLED(0, REDLED);
		sendCommand(commfd, inverter[currentInv], cmdList[currentCmd]);
	
		if (select(numfds, &readfd, NULL, &errorfd, &timeout) == 0) {	// select timed out. Bad news 
			DEBUG fprintf(stderr, "Timeout .. ");
			if (online == 1) {
				sprintf(buffer, "WARN " PROGNAME " %d No data for last period", controllernum);
				logmsg(WARN, buffer);
				online = 0;	// Don't send a message every minute from now on
			}
			// For USB, attempt to reconnect
			if (isPort(serialName)) {
				close(commfd);
				if ((commfd = openSerial(serialName, BAUD, 0, CS8, 1)) < 0) {
					sprintf(buffer, "WARN " PROGNAME " %d Failed to reopen %s", serialName, controllernum);
					logmsg(WARN, buffer);
				}
			}
			goto next;
		}
		else		// Got some data ...
		{
			blinkLED(1, REDLED);
			DEBUG2 fprintf(stderr, "After: Readfd %lx ", readfd);
			DEBUG2 fprintf(stderr, "ErrorFD %lx ", errorfd);
			if (FD_ISSET(commfd, &readfd)) {
				data.count = 0;
				data.escape = 0;
				// synctostart(commfd);
				getbuf(commfd, 18);
			
				processPacket(currentInv);
			};
			blinkLED(0, REDLED);
			// Set to next commmand;
			currentCmd++;
			if (cmdList[currentCmd] == 0) {
next:
				currentCmd = 0;
				currentInv++;
				if (currentInv = numinverters) {
					currentInv = 0;
				}
				sleep(10);
			}
		}
		if ((noserver == 0) && FD_ISSET(sockfd[0], &readfd))
			run = processSocket();	// the server may request a shutdown by setting run to 0
	}
	logmsg(INFO,"INFO " PROGNAME " Shutdown requested");
	close(sockfd[0]);
	closeSerial(commfd);
	
	return 0;
}

/***********/
/* CATCHER */
/***********/
void catcher(int sig) {
	// Turn LED off at exit
	blinkLED(0, REDLED);
	exit(0);
}

/*********/
/* USAGE */
/*********/
void usage(void) {
	printf("Usage: %s [-t timeout] [-l] [-s] [-d] [-a XX] [-e] [-V] [-o offset] /dev/ttyname controllernum\n", progname);
	printf("-l: no log  -s: no server  -d: debug on\n -V: version -i: interval in seconds\n");
	printf("-e Eavesdrop -a Address\n");
	return;
}

/*****************/
/* PROCESSSOCKET */
/*****************/
int processSocket(void){
	// Deal with commands from MCP.  Return to 0 to do a shutdown
	short int msglen, numread;
	char buffer2[192];	// about 128 is good but rather excessive since longest message is 'truncate'
	char * cp = &buffer[0];
	int retries = NUMRETRIES;
	
	if (read(sockfd[0], &msglen, 2) != 2) {
		logmsg(WARN, "WARN " PROGNAME " Failed to read length from socket");
		return 1;
	}
	msglen =  ntohs(msglen);
	while ((numread = read(sockfd[0], cp, msglen)) < msglen) {
		cp += numread;
		msglen -= numread;
		if (--retries == 0) {
			logmsg(WARN, "WARN " PROGNAME " Timed out reading from server");
			return 1;
		}
		usleep(RETRYDELAY);
	}
	cp[numread] = '\0';	// terminate the buffer 
	
	if (strcmp(buffer, "exit") == 0)
		return 0;	// Terminate program
	if (strcmp(buffer, "Ok") == 0)
		return 1;	// Just acknowledgement
	if (strcmp(buffer, "truncate") == 0) {
		if (logfp) {
			// ftruncate(logfp, 0L);
			// lseek(logfp, 0L, SEEK_SET);
			freopen(NULL, "w", logfp);
			logmsg(INFO, "INFO " PROGNAME " Truncated log file");
		} else
			logmsg(INFO, "INFO " PROGNAME " Log file not truncated as it is not open");
		return 1;
	}
	if (strcmp(buffer, "debug 0") == 0) {	// turn off debug
		debug = 0;
		return 1;
	}
	if (strcmp(buffer, "debug 1") == 0) {	// enable debugging
		debug = 1;
		return 1;
	}
	if (strcmp(buffer, "debug 2") == 0) {	// enable debugging
		debug = 2;
		return 1;
	}
	if (strcmp(buffer, "help") == 0) {
		strcpy(buffer2, "INFO " PROGNAME " Commands are: debug 0|1|2, exit, truncate");
		logmsg(INFO, buffer2);
		return 1;
	}
	
	strcpy(buffer2, "INFO " PROGNAME " Unknown message from server: ");
	strcat(buffer2, buffer);
	logmsg(INFO, buffer2);	// Risk of loop: sending unknown message straight back to server
	
	return 1;	
};

/**************/
/* GETVERSION */
/**************/
char *getversion(void) {
	// return pointer to version part of REVISION macro
	static char version[10] = "";	// Room for xxxx.yyyy
	if (!strlen(version)) {
		strcpy(version, REVISION+11);
		version[strlen(version)-2] = '\0';
	}
	return version;
}

/***********/
/* DUMPBUF */
/***********/
void dumpbuf() {
	int i;
	for (i = 0; i < data.count; i++) {
		fprintf(stderr, "%02x", data.buf[i]);
		if (i == 1 || i == 5 || i == 9 || i ==11)
			putc('-', stderr);
		else
			putc(' ', stderr);
	}
	putc('\n', stderr);
}

/**********/
/* GETBUF */
/**********/
int getbuf(int fd, int max) {
	// Read up to max chars into supplied buf. Return number
	// of chars read or negative error code if applicable

	int ready, numtoread, now;
	fd_set readfd; 
	struct timeval timeout;
	FD_ZERO(&readfd);
	numtoread = max;
	DEBUG2 fprintf(stderr, "Getbuf entry %d count=%d ", max ,data.count);
	
	while(1) {
		FD_SET(fd, &readfd);
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;	 // 0.5sec
		ready = select(fd + 1, &readfd, NULL, NULL, &timeout);
		DEBUG4 {
			gettimeofday(&timeout, NULL);
			fprintf(stderr, "%03ld.%03d ", timeout.tv_sec%100, timeout.tv_usec / 1000);
		}
		if (ready == 0) {
			DEBUG2 fprintf(stderr, "Gotbuf %d bytes ", data.count);
			return data.count;		// timed out - return what we've got
		}
		DEBUG4 fprintf(stderr, "Getbuf: before read1 ");
		now = read(fd, data.buf + data.count, 1);
		DEBUG4 fprintf(stderr, "After read1\n");
		DEBUG3 fprintf(stderr, "0x%02x ", data.buf[data.count]);
		if (now < 0)
			return now;
		if (now == 0) {
			fprintf(stderr, "ERROR fd was ready but got no data\n");
			// VBUs / LAN  - can't use standard Reopenserial as device name hostname: port is not valid
			fd = reopenSerial(fd, serialName, BAUD, 0, CS8, 1);
			continue;
		}
		
		data.count += now;
		numtoread -= now;
		if (numtoread == 0) return data.count;
		if (numtoread < 0) {	// CANT HAPPEN
			fprintf(stderr, "ERROR buffer overflow - increase max from %d (numtoread = %d numread = %d)\n", 
					max, numtoread, data.count);
			return data.count;
			
		}
	}
}

/***********/
/* TIMEMOD */
/***********/
time_t timeMod(time_t interval) {
	// Return a time in the future at modulus t;
	// ie, if t = 3600 (1 hour) the time returned
	// will be the next time on the hour.
	//	time_t now = time(NULL);
	char buffer[20];
	if (interval == 0) interval = 600;
	time_t t = time(NULL);
	time_t newt =  (t / interval) * interval + interval;
	DEBUG2 {
		struct tm * tm;
		tm = localtime(&t);
		strftime(buffer, sizeof(buffer), "%F %T", tm);
		fprintf(stderr,"TimeMod now = %s delta = %zd ", buffer, interval);
		tm = localtime(&newt);
		strftime(buffer, sizeof(buffer), "%F %T", tm);
		fprintf(stderr, "result %s\n", buffer);
	} 
	return newt;
}

/**************/
/* SENDSERIAL */
/**************/
int sendSerial(int fd, unsigned char data) {
	// Send a single byte.  Return 1 for a logged failure
	int retries = SERIALNUMRETRIES;
	int written;
#ifdef DEBUGCOMMS
	printf("Comm 0x%02x(%d) ", data, data);
	return 0;
#endif
	
	DEBUG2 fprintf(DEBUGFP, "%02x.", data);
	while ((written = write(fd, &data, 1)) < 1) {
        fprintf(DEBUGFP, "Serial wrote %d bytes errno = %d", written, errno);
        perror("");
		if (--retries == 0) {
			logmsg(WARN, "WARN " PROGNAME " timed out writing to serial port");
			return 1;
		}
		DEBUG fprintf(DEBUGFP, "Pausing %d ... ", SERIALRETRYDELAY);
		usleep(SERIALRETRYDELAY);
	}
	return 0;       // ok
}

/***************/
/* SENDCOMMAND */
/***************/
void sendCommand(int fd, int addr, int cmd)
{
	// As before, return 1 for a logged failure, otherwise 0
	// Send from sendMsg buffer
	
	int i, fcs;
	
	DEBUG2 fprintf(DEBUGFP, "SendCommand to %x: %x\t", addr, cmd);
	
	sendBuf[0] = addr & 0xFF;
	sendBuf[1] = (cmd >> 8) & 0xFF;
	sendBuf[2] = cmd & 0xff;
	fcs = INITFCS;
	fcs = fcs16(fcs, sendBuf, 8);
	fcs ^= 0xffff;		// Get complement of CRC.
	
	for (i = 0; i < 8; i++) {
		sendSerial(fd, sendBuf[i]);
	}
	// Checksum
	sendSerial(fd, fcs & 0xff);
	sendSerial(fd, (fcs >>8) & 0xff);
}

/* These differ from the SMA function of the same name as the Aurora is big-endian  */
inline int getShort(unsigned char * ptr) {
	return *(ptr+1) | (*ptr << 8);
}

inline int getLong(unsigned char * ptr) {
	return *(ptr+3)  | (*(ptr+2) << 8) | (*(ptr+1) << 16) |  (*ptr << 24);
}

int isPort(char * name) {
	// Return TRUE if name is of form hostname:port
	// else FALSE
	if (strchr(name, ':'))
		return 1;
	else
		return 0;
}

/******************/
/* PROCESS PACKET */
/******************/
void processPacket(int currentInv) {
	// Output the buffer
	// RS232 - must be 8 bytes
	// RS485 - must be 18 bytes - first 10 is outgoing message
	int checksum;
	unsigned char * dataptr;
	static float vac, wac, hz, t1, vdc, idc, kwh, iac;
	static int state[MAXINVERTERS];
	union {float f; int i;} u;
	DEBUG2 fprintf(stderr,"Packet len:%d ", data.count);
	DEBUG2 dumpbuf();
	if (data.count == 0)
		return;
	dataptr = &data.buf[0];
	if (data.count == 10) // RS485 at night: send 10, receive 10. No data.
		return;
	
	if (data.count == 18) 	 { // RS485
		dataptr = &data.buf[10];
		data.count -= 10;
	}
	
	if (data.count != 8) {
		sprintf(buffer, "WARN " PROGNAME " %d Packet length %d not 8 - discarding.", 
				controllernum, data.count);
		logmsg(WARN, buffer);
		DEBUG dumpbuf();
		if (debug == 0) return;	// Process faulty packets in debug mode.
	}
	checksum = fcs16(INITFCS, dataptr, 6);
	DEBUG3 fprintf(stderr, "Intermediate FCS = %x (^%x)", checksum, ~checksum & 0xFFFF);
	checksum = fcs16(checksum, dataptr + 6, 2);
	DEBUG3 fprintf(stderr, "Final FCS = %x ", checksum);
	if (checksum != GOODFCS) {
		sprintf(buffer, "WARN " PROGNAME " %d Checksum failed got %x instead of %x", 
				controllernum + currentInv, checksum, GOODFCS);
		logmsg(WARN, buffer);
		return;
	}
	u.i = getLong(dataptr + 2);
	switch(cmdList[currentCmd]) {
		case 0x3f00:	// Get Serial Number
			DEBUG fprintf(stderr, "Serial Number %c%c%c%c%c%c\n", *dataptr, *(dataptr+1),
				*(dataptr+2), *(dataptr+3), *(dataptr+4), *(dataptr+5));
			break;
		case 0x3b01:
			DEBUG fprintf(stderr, "Vac:%.1f ", u.f);	
			vac = u.f;  break;
		case 0x3b02:
			iac = u.f;
			DEBUG fprintf(stderr, "Iac:%.2f ", u.f);	break;
		case 0x3b03:
			wac = u.f;
			DEBUG fprintf(stderr, "Wac:%.1f ", u.f);	break;
		case 0x3b04:
			hz = u.f;
			DEBUG fprintf(stderr, "HZ:%.1f ", u.f);	break;
		case 0x3b07:
			DEBUG fprintf(stderr, "Leakage:%.3f ", u.f);	break;
		case 0x3b08:
			DEBUG fprintf(stderr, "Wdc:%.1f ", u.f);	break;
		case 0x3b09:
			DEBUG fprintf(stderr, "3b09:%.3f ", u.f);	break;
		case 0x3b15:
			t1 = u.f;
			DEBUG fprintf(stderr, "T1:%.1f ", u.f);	break;
		case 0x3b16:
			DEBUG fprintf(stderr, "3b16:%.3f ", u.f);	break;
		case 0x3b17:
			vdc = u.f;
			DEBUG fprintf(stderr, "Vdc1:%.1f ", u.f);	break;
		case 0x3b19:
			idc = u.f;
			DEBUG fprintf(stderr, "Idc1:%.1f ", u.f);	break;
		case 0x3b1a:
			DEBUG fprintf(stderr, "Vdc2:%.1f ", u.f);	break;
		case 0x3b1b:
			DEBUG fprintf(stderr, "Idc2:%.1f ", u.f);	break;
		case 0x3b1e:
			DEBUG2 fprintf(stderr, "Float %x %f\n", u.i, u.f);	
			DEBUG fprintf(stderr, "%.2f ", u.f);
			break;
		case 0x3200:	// Status
			DEBUG fprintf(stderr, "Status %x ", u.i);
			if (u.i != state[currentInv])
				stateMessage(u.i, *(dataptr+1), currentInv);
			state[currentInv] = u.i;
			break;			
		case 0x4e00:		// Daily generation
			DEBUG fprintf(stderr, "4e00:%d ", u.i);	break;
		case 0x4e05:		// Total generation
			kwh = u.i / 1000.0;
			DEBUG fprintf(stderr, "Kwh:%0.3f(+ %f = %f)\n", kwh, offset, kwh + offset);
			sprintf(buffer, "inverter watts:%.1f kwh:%.3f iac:%.2f vac:%.1f hz:%.2f idc:%.2f vdc:%1.f t1:%.2f",
					wac, kwh + offset, iac, vac, hz, idc, vdc, t1);
			sockSend(sockfd[currentInv], buffer);
			break;
		default:
			DEBUG2 fprintf(stderr, "Data 0x%04x 0x%x 0x%x %d %f\n", cmdList[currentCmd], getShort(dataptr), u.i, u.i, u.f);
			DEBUG fprintf(stderr, "0x%x:%d(%.3f) ", cmdList[currentCmd], u.i, u.f);
			break;
	}
}

#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )

/****************/
/* STATEMESSAGE */
/****************/
void stateMessage(int state, int global, int currentInv) {
	// Write out the statemessage
	// Assume value < 0x1000 are INFO and the rest are WARN
	int alarm, dc1, dc2, status, inverter;
	char buf2[140];
	inverter = (state >> 24) & 0xFF;
	dc1 = (state >> 16) & 0xFF;
	dc2 = (state >> 8) & 0xFF;
	alarm = state & 0xFF;
	int num = controllernum + currentInv;
	status = 0;
	DEBUG fprintf(stderr, "Status %d global %d inverter %d dc1 %d dc2 %d alarm %d\n", status, global, inverter, dc1, dc2, alarm);
	if (global < MAXGLOBAL) status = max(status, global_status[global].code);	else global = MAXGLOBAL;
	if (inverter < MAXGLOBAL) status = max(status, inverter_status[inverter].code);	else inverter = MAXINVERTERSTATUS;
	if (dc1 < MAXDCDC) status = max(status, dcdc_status[dc1].code);				else dc1 = MAXDCDC;
	if (dc2 < MAXDCDC) status = max(status, dcdc_status[dc2].code);				else dc2 = MAXDCDC;
	if (alarm < MAXALARM) status = max(status, alarm_status[alarm].code);	else alarm = MAXALARM;
	DEBUG fprintf(stderr, "Status %d global %d dc1 %d dc2 %d alarm %d\n", status, global, dc1, dc2, alarm);
	if (status == INFO)
		sprintf(buffer, "INFO ");
	if (status == WARN)
		sprintf(buffer, "WARN ");
	if (status == ERROR)
		sprintf(buffer, "ERROR ");
	DEBUG fprintf(stderr, "status %d ", status);
	strcat (buffer, PROGNAME " ");
	snprintf(buf2, sizeof(buf2), "%d Status %02x %08x G:%s/I:%s/DC1:%s/DC2:%s/A:%s", 
			 num, global, state, global_status[global].message, 
			 inverter_status[inverter].message,
			 dcdc_status[dc1].message, 
			 dcdc_status[dc2].message,
			 alarm_status[alarm].message);
	strcat(buffer, buf2);
	logmsg(status, buffer);
}

/**********/
/* DETECT */
/**********/
int detect(int commfd, int maxinverters) {
	// This simply sends out a GetSerialNumber request to each address in order.
	
	fd_set readfd, errorfd;
	struct timeval timeout;
	unsigned int i;	// Range is 01 to 254
	numinverters = 0;
	FD_ZERO(&readfd);
	FD_ZERO(&errorfd);
	for(i = 1; i < 10; i++) {
		timeout.tv_sec = 0;		// 0.1 second.  Takes up to 32 seconds to find a complete address
		timeout.tv_usec = 100000;
		FD_SET(commfd, &readfd);
		FD_SET(commfd, &errorfd);
		DEBUG2 fprintf(stderr, "Before: Readfd %lx ", readfd);
		DEBUG2 fprintf(stderr, "ErrorFD %lx ", errorfd);
		blinkLED(0, REDLED);
		DEBUG2 fprintf(stderr, "Detecting %d .. ", i);
		sendCommand(commfd, i, 0x3F00);		// Get SN
		
		if (select(commfd + 1, &readfd, NULL, &errorfd, &timeout) == 0) {	// select timed out. Bad news 
			continue;
		}
		blinkLED(1, REDLED);
		DEBUG4 fprintf(stderr, "After: Readfd %lx ", readfd);
		DEBUG4 fprintf(stderr, "ErrorFD %lx ", errorfd);
		if (FD_ISSET(commfd, &readfd)) {
			data.count = 0;
			getbuf(commfd, 18);
			if (data.count == 8 || data.count == 18) {
				processPacket(numinverters);
				DEBUG fprintf(stderr,"Got %s inverter[%d]=%d\n", data.count == 8 ? "RS232" : "RS485", 
					numinverters, i);
				DEBUG dumpbuf();
				inverter[numinverters] = i;
				numinverters ++;
				if (numinverters > MAXINVERTERS) {
					sprintf(buffer, "ERROR " PROGNAME " Increase MAXINVERTERS from %d", MAXINVERTERS);
					logmsg(ERROR, buffer);
				}
			}
		};
		blinkLED(0, REDLED);
	}
	
	return numinverters == 0;	// Not found.
}


u16 fcs16(u16 fcs, uchar * cp, int len) {
	// If return from this is GOODFCS when on entry FCS = INITFCS, the packet is valid
	while (len--)
		fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xFF];
	return fcs;
}
