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

#ifndef _packet_h
#define _packet_h

#include <stdio.h>

#include "bool.h"
class psiEmul;
class bufferStore;
class IOWatch;

#define PKT_DEBUG_LOG  1
#define PKT_DEBUG_DUMP 2

class packet {
	public:
		packet(const char *fname, int baud, IOWatch &iow, short int verbose = 0);
		~packet();
		void send(unsigned char type, const bufferStore &b);
		bool get(unsigned char &type, bufferStore &b);
		void setVerbose(short int);
		short int getVerbose();
		bool linkFailed();
  
	private:
		bool terminated();
		void addToCrc(unsigned short a, unsigned short *crc);
		void opByte(unsigned char a);
		void realWrite();

		unsigned short crcOut;
		unsigned short crcIn;
		unsigned char *inPtr;
		unsigned char *outPtr;
		unsigned char *endPtr;
		unsigned char *inBuffer;
		unsigned char *outBuffer;
		bufferStore rcv;
		int inLen;
		int outLen;
		int termLen;
		int fd;
		bool verbose;
		bool esc;
		char *devname;
		int baud;
		IOWatch &iow;
};

#endif
