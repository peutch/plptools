// $Id$
//
//  PLP - An implementation of the PSION link protocol
//
//  Copyright (C) 1999  Philip Proudman
//  Modifications for plptools:
//    Copyright (C) 1999 Fritz Elfert <felfert@to.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  e-mail philip.proudman@btinternet.com


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <fstream.h>
#include <iomanip.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>

extern "C" {
#include "mp_serial.h"
}
#include "bufferstore.h"
#include "packet.h"
#include "iowatch.h"

#define BUFFERLEN 2000

packet::packet(const char *fname, int _baud, IOWatch & _iow, short int _verbose = 0):
iow(_iow)
{
	verbose = _verbose;
	devname = strdup(fname);
	baud = _baud;
	inPtr = inBuffer = new unsigned char[BUFFERLEN + 1];
	outPtr = outBuffer = new unsigned char[BUFFERLEN + 1];
	inLen = outLen = termLen = 0;
	foundSync = 0;
	esc = false;
	lastFatal = false;
	serialStatus = -1;
	crcIn = crcOut = 0;

	fd = init_serial(devname, baud, 0);
	if (fd == -1)
		lastFatal = true;
	else
		iow.addIO(fd);
}

void packet::reset()
{
	if (verbose & PKT_DEBUG_LOG)
		cout << "resetting serial connection" << endl;
	if (fd != -1) {
		iow.remIO(fd);
		ser_exit(fd);
	}
	usleep(100000);
	inLen = outLen = termLen = 0;
	inPtr = inBuffer;
	outPtr = outBuffer;
	foundSync = 0;
	serialStatus = -1;
	esc = false;
	crcIn = crcOut = 0;

	fd = init_serial(devname, baud, 0);
	if (fd != -1) {
		iow.addIO(fd);
		lastFatal = false;
	}
	if (verbose & PKT_DEBUG_LOG)
		cout << "serial connection reset, fd=" << fd << endl;
	sleep(1);
}

short int packet::
getVerbose()
{
	return verbose;
}

void packet::
setVerbose(short int _verbose)
{
	verbose = _verbose;
}

packet::~packet()
{
	if (fd != -1) {
		iow.remIO(fd);
		ser_exit(fd);
	}
	usleep(100000);
	delete[]inBuffer;
	delete[]outBuffer;
	free(devname);
}

void packet::
send(unsigned char type, const bufferStore & b)
{
	if (verbose & PKT_DEBUG_LOG) {
		cout << "packet: type " << hex << (int) type << " >> ";
		if (verbose & PKT_DEBUG_DUMP)
			cout << b << endl;
		else
			cout << "len=" << b.getLen() << endl;
	}
	opByte(0x16);
	opByte(0x10);
	opByte(0x02);

	crcOut = 0;
	opByte(type);
	addToCrc(type, &crcOut);

	long len = b.getLen();
	for (int i = 0; i < len; i++) {
		unsigned char c = b.getByte(i);
		if (c == 0x10)
			opByte(c);
		opByte(c);
		addToCrc(c, &crcOut);
	}

	opByte(0x10);
	opByte(0x03);

	opByte(crcOut >> 8);
	opByte(crcOut & 0xff);
	realWrite();
}

void packet::
addToCrc(unsigned short c, unsigned short *crc)
{
	c <<= 8;
	for (int i = 0; i < 8; i++) {
		if ((*crc ^ c) & 0x8000)
			*crc = (*crc << 1) ^ 0x1021;
		else
			*crc <<= 1;
		c <<= 1;
	}
}

void packet::
opByte(unsigned char a)
{
	*outPtr++ = a;
	outLen++;
	if (outLen >= BUFFERLEN)
		realWrite();
}

void packet::
realWrite()
{
	outPtr = outBuffer;
	while (outLen > 0) {
		int r = write(fd, outPtr, outLen);
		if (verbose & PKT_DEBUG_LOG)
			cout << "packet: WR=" << dec << r << endl;
		if (r > 0) {
			outLen -= r;
			outPtr += r;
		}
	}
	outPtr = outBuffer;
}

bool packet::
get(unsigned char &type, bufferStore & ret)
{
	while (!terminated()) {
		if (linkFailed())
			return false;
		int res = read(fd, inPtr, BUFFERLEN - inLen);
		if (res > 0) {
			if (verbose & PKT_DEBUG_LOG)
				cout << "packet: rcv " << dec << res << endl;
			inPtr += res;
			inLen += res;
		}
		if (res < 0)
			return false;
		// XXX Solaris returns 0 on non blocking TTY lines
		// even when VMIN > 0
		if( res == 0 && inLen == 0 )
			return false;
		if (inLen >= BUFFERLEN) {
			cerr << "packet: input buffer overflow!!!!" << endl;
			inLen = 0;
			inPtr = inBuffer;
			return false;
		}
	}
	if (verbose & PKT_DEBUG_LOG) {
		cout << "packet: get ";
		if (verbose & PKT_DEBUG_DUMP) {
			for (int i = foundSync - 3; i < termLen; i++)
				cout << hex << setw(2) << setfill('0') << (int) inBuffer[i] << " ";
		} else
			cout << "len=" << dec << termLen;
		cout << endl;
	}
	inLen -= termLen;
	termLen = 0;
	foundSync = 0;
	bool crcOk = (endPtr[0] == ((crcIn >> 8) & 0xff) && endPtr[1] == (crcIn & 0xff));
	if (inLen > 0)
		memmove(inBuffer, &endPtr[2], inLen);
	inPtr = inBuffer + inLen;
	if (crcOk) {
		type = rcv.getByte(0);
		ret = rcv;
		ret.discardFirstBytes(1);
		return true;
	} else {
		if (verbose & PKT_DEBUG_LOG)
			cout << "packet: BAD CRC" << endl;
	}
	return false;
}

bool packet::
terminated()
{
	unsigned char *p;
	int l;

	if (inLen < 6)
		return false;
	p = inBuffer + termLen;
	if (!foundSync) {
	  while (!foundSync && (inLen - termLen >= 6))
	  {
		termLen++;
		if (*p++ != 0x16)
			continue;
		termLen++;
		if (*p++ != 0x10)
			continue;
		termLen++;
		if (*p++ != 0x02)
			continue;
		foundSync = termLen;
	  }
	  if (!foundSync)
			return false;

	if (verbose & PKT_DEBUG_LOG) {
	  if (foundSync != 3)
	    cout << "packet: terminated found sync at " << foundSync << endl;
	}
		esc = false;
		// termLen = 3;
		crcIn = 0;
		rcv.init();
	}
	for (l = termLen; l < inLen - 2; p++, l++) {
		if (esc) {
			esc = false;
			if (*p == 0x03) {
				endPtr = p + 1;
				termLen = l + 3;
				return true;
			}
			addToCrc(*p, &crcIn);
			rcv.addByte(*p);
		} else {
			if (*p == 0x10)
				esc = true;
			else {
				addToCrc(*p, &crcIn);
				rcv.addByte(*p);
			}
		}
	}
	termLen = l;
	return false;
}

bool packet::
linkFailed()
{
	int arg;
	int res;
	bool failed = false;

	if (lastFatal)
		reset();
	res = ioctl(fd, TIOCMGET, &arg);
	if (res < 0)
		lastFatal = true;
	if ((serialStatus == -1) || (arg != serialStatus)) {
		if (verbose & PKT_DEBUG_HANDSHAKE)
			cout << "packet: < DTR:" << ((arg & TIOCM_DTR)?1:0)
			     << " RTS:" << ((arg & TIOCM_RTS)?1:0)
			     << " DCD:" << ((arg & TIOCM_CAR)?1:0)
			     << " DSR:" << ((arg & TIOCM_DSR)?1:0)
			     << " CTS:" << ((arg & TIOCM_CTS)?1:0) << endl;
		if (!((arg & TIOCM_RTS) && (arg & TIOCM_DTR))) {
			arg |= (TIOCM_DTR | TIOCM_RTS);
			res = ioctl(fd, TIOCMSET, &arg);
			if (res < 0)
				lastFatal = true;
			if (verbose & PKT_DEBUG_HANDSHAKE)
				cout << "packet: > DTR:" << ((arg & TIOCM_DTR)?1:0)
				     << " RTS:" << ((arg & TIOCM_RTS)?1:0)
				     << " DCD:" << ((arg & TIOCM_CAR)?1:0)
				     << " DSR:" << ((arg & TIOCM_DSR)?1:0)
				     << " CTS:" << ((arg & TIOCM_CTS)?1:0) << endl;
		}
		serialStatus = arg;
	}
#ifdef sun
	if ((arg & TIOCM_CTS) == 0)
#else
	if (((arg & TIOCM_DSR) == 0) || ((arg & TIOCM_CTS) == 0))
#endif
		failed = true;
	if ((verbose & PKT_DEBUG_LOG) && lastFatal)
		cout << "packet: linkFATAL\n";
	if ((verbose & PKT_DEBUG_LOG) && failed)
		cout << "packet: linkFAILED\n";
	return lastFatal || failed;
}

