/*
 radioroom.cpp

 This file is a part of SPLRadioRoom project.

 (C) Copyright 2017 Envirover.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

//#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include "SPLRadioRoom.h"

#define LOG_IDENTITY "radioroom"

SPLRadioRoom radioroom;

int main(int argc, char** argv)
{
    cout << "Radioroom started.";

    openlog(LOG_IDENTITY, LOG_CONS | LOG_NDELAY, LOG_USER);
    //setlogmask (LOG_UPTO (LOG_INFO));

    syslog(LOG_NOTICE, "radioroom started.");

    //close(STDIN_FILENO);
    //close(STDOUT_FILENO);
    //close(STDERR_FILENO);

    if (config.init(argc, argv)) {
        syslog(LOG_ERR, "Invalid configuration.\n");
        return 1;
    }

    radioroom.init();

    while(1) {
        radioroom.loop();
    }

    return 0;
}
