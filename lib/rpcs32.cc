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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream.h>
#include <iomanip.h>
#include <time.h>
#include <string.h>

#include "rpcs32.h"
#include "bufferstore.h"
#include "bufferarray.h"
#include "ppsocket.h"

rpcs32::rpcs32(ppsocket * _skt)
{
	skt = _skt;
	reset();
}

rpcs32::~rpcs32()
{
	bufferStore a;
	a.addStringT("Close");
	if (status == rfsv::E_PSI_GEN_NONE)
		skt->sendBufferStore(a);
	skt->closeSocket();
}

Enum<rfsv::errs> rpcs32::
queryDrive(char drive, bufferArray &ret)
{
	bufferStore a;
	Enum<rfsv::errs> res;

	a.addByte(drive);
	if (!sendCommand(rpcs::QUERY_DRIVE, a))
		return rfsv::E_PSI_FILE_DISC;
	if ((res = getResponse(a, false)) != rfsv::E_PSI_GEN_NONE)
		return res;
	int l = a.getLen();
	ret.clear();
	while (l > 0) {
		bufferStore b, c;
		const char *s;
		char *p;
		int pid;
		int sl;

		s = a.getString(0);
		sl = strlen(s) + 1;
		l -= sl;
		a.discardFirstBytes(sl);
		if ((p = strstr(s, ".$"))) {
			*p = '\0'; p += 2;
			sscanf(p, "%d", &pid);
		} else
			pid = 0;
		b.addWord(pid);
		b.addStringT(s);
		s = a.getString(0);
		sl = strlen(s) + 1;
		l -= sl;
		a.discardFirstBytes(sl);
		c.addStringT(s);
		ret.push(c);
		ret.push(b);
	}
	return res;
}

Enum<rfsv::errs> rpcs32::
getCmdLine(const char *process, bufferStore &ret)
{
	bufferStore a;
	Enum<rfsv::errs> res;

	a.addStringT(process);
	if (!sendCommand(rpcs::GET_CMDLINE, a))
		return rfsv::E_PSI_FILE_DISC;
	res = getResponse(a, true);
	ret = a;
	return res;
}

Enum<rfsv::errs> rpcs32::
getMachineInfo(machineInfo &mi)
{
	bufferStore a;
	Enum<rfsv::errs> res;

	if (!sendCommand(rpcs::GET_MACHINE_INFO, a))
		return rfsv::E_PSI_FILE_DISC;
	if ((res = getResponse(a, true)) != rfsv::E_PSI_GEN_NONE)
		return res;
	if (a.getLen() != 256)
		return rfsv::E_PSI_GEN_FAIL;
	mi.machineType = (enum rpcs::machs)a.getDWord(0);
	strncpy(mi.machineName, a.getString(16), 16);
	mi.machineName[16] = '\0';
	mi.machineUID = a.getDWord(44);
	mi.machineUID <<= 32;
	mi.machineUID |= a.getDWord(40);
	mi.countryCode = a.getDWord(56);
	mi.uiLanguage = (enum rpcs::languages)a.getDWord(164);

	mi.romMajor = a.getByte(4);
	mi.romMinor = a.getByte(5);
	mi.romBuild = a.getWord(6);
	mi.romSize = a.getDWord(140);

	mi.ramSize = a.getDWord(136);
	mi.ramMaxFree = a.getDWord(144);
	mi.ramFree = a.getDWord(148);
	mi.ramDiskSize = a.getDWord(152);

	mi.registrySize = a.getDWord(156);
	mi.romProgrammable = (a.getDWord(160) != 0);

	mi.displayWidth = a.getDWord(32);
	mi.displayHeight = a.getDWord(36);

	mi.time.tv_low = a.getDWord(48);
	mi.time.tv_high = a.getDWord(52);

	mi.tz.utc_offset = a.getDWord(60);
	mi.tz.dst_zones = a.getDWord(64);
	mi.tz.home_zone = a.getDWord(68);

	mi.mainBatteryInsertionTime.tv_low = a.getDWord(72);
	mi.mainBatteryInsertionTime.tv_high = a.getDWord(76);
	mi.mainBatteryStatus = (enum rpcs::batterystates)a.getDWord(80);
	mi.mainBatteryUsedTime.tv_low = a.getDWord(84);
	mi.mainBatteryUsedTime.tv_high = a.getDWord(88);
	mi.mainBatteryCurrent = a.getDWord(92);
	mi.mainBatteryUsedPower = a.getDWord(96);
	mi.mainBatteryVoltage = a.getDWord(100);
	mi.mainBatteryMaxVoltage = a.getDWord(104);

	mi.backupBatteryStatus = (enum rpcs::batterystates)a.getDWord(108);
	mi.backupBatteryVoltage = a.getDWord(112);
	mi.backupBatteryMaxVoltage = a.getDWord(116);
	mi.backupBatteryUsedTime.tv_low = a.getDWord(124);
	mi.backupBatteryUsedTime.tv_high = a.getDWord(128);

	mi.externalPower = (a.getDWord(120) != 0);

	return res;
}

static unsigned long hhh;

Enum<rfsv::errs> rpcs32::
regOpenIter(void)
{
	bufferStore a;
	Enum<rfsv::errs> res;

	a.addStringT("HKLM\\");
	if (!sendCommand(rpcs::REG_OPEN_ITER, a))
		return rfsv::E_PSI_FILE_DISC;
	res = getResponse(a, true);
	cout << "ro: r=" << res << " a=" << a << endl;
	if (a.getLen() > 0)
		hhh = a.getDWord(0);
	return rfsv::E_PSI_GEN_NONE;
}

Enum<rfsv::errs> rpcs32::
configOpen(void)
{
	bufferStore a;
	Enum<rfsv::errs> res;

	if (!sendCommand(rpcs::CONFIG_OPEN, a))
		return rfsv::E_PSI_FILE_DISC;
	res = getResponse(a, true);
	cout << "co: r=" << res << " a=" << a << endl;
	if (a.getLen() > 0)
		hhh = a.getDWord(0);
	return rfsv::E_PSI_GEN_NONE;
}

Enum<rfsv::errs> rpcs32::
configRead(void)
{
	bufferStore a;
	Enum<rfsv::errs> res;
	int l;
	FILE *f;

	f = fopen("blah", "w");
	do {
		a.init();
		a.addDWord(hhh);
		if (!sendCommand(rpcs::CONFIG_READ, a))
			return rfsv::E_PSI_FILE_DISC;
		if ((res = getResponse(a, true)) != rfsv::E_PSI_GEN_NONE)
			return res;
		l = a.getLen();
		cout << "cr: " << l << endl;
		fwrite(a.getString(0), 1, l, f);
	} while (l > 0);
	fclose(f);
//cout << "cr: r=" << res << " a=" << a << endl;
	return rfsv::E_PSI_GEN_NONE;
}
