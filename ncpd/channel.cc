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
#include <stream.h>

#include "channel.h"
#include "ncp.h"

channel::channel(ncp * _ncpController)
{
	verbose = 0;
	ncpChannel = 0;
	ncpController = _ncpController;
	_terminate = false;
}

void channel::
ncpSend(bufferStore & a)
{
	ncpController->send(ncpChannel, a);
}

bool channel::
terminate()
{
	return _terminate;
}

void channel::
terminateWhenAsked()
{
	_terminate = true;
}

void channel::
ncpConnect()
{
	ncpController->connect(this);
}

void channel::
ncpRegister()
{
	ncpController->Register(this);
}

void channel::
ncpDoRegisterAck(int ch)
{
	ncpController->RegisterAck(ch);
}

void channel::
ncpDisconnect()
{
	ncpController->disconnect(ncpChannel);
}

short int channel::
ncpProtocolVersion()
{
	return ncpController->getProtocolVersion();
}

void channel::
setNcpChannel(int chan)
{
	ncpChannel = chan;
}

int channel::
getNcpChannel()
{
	return ncpChannel;
}

void channel::
newNcpController(ncp * _ncpController)
{
	ncpController = _ncpController;
}

void channel::
setVerbose(short int _verbose)
{
	verbose = _verbose;
}

short int channel::
getVerbose()
{
	return verbose;
}
