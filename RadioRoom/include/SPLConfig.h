/*
 SPLConfig.h

 Iridium SBD telemetry for ArduPilot.

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
#include <string>
#include <termios.h>

using namespace std;

#define DEFAULT_MAVLINK_SERIAL      "/dev/ttyUSB0"
#define DEFAULT_ISBD_SERIAL         "/dev/ttyUSB1"
#define DEFAULT_SERIALS             "/dev/ttyUSB0,/dev/ttyUSB1,/dev/ttyUSB2,/dev/ttyS0,/dev/ttyAMA0"

#define AP_TELEM_BAUD_RATE          B57600
#define ISBD_BAUD_RATE              B19200



#define DEFAULT_REPORT_PERIOD  300L // 5 minutes

/**
 * SPL configuration service.
 */
class SPLConfig {

    string   mavlink_serial;
    string   isbd_serial;
    speed_t  mavlink_serial_speed;
    speed_t  isbd_serial_speed;
    bool     auto_detect_serials;
    string   serials;
    unsigned long report_period;

public:
    SPLConfig();

    int init(int argc, char** argv);

    const char* get_mavlink_serial() const;
    void set_mavlink_serial(const char* path);

    speed_t get_mavlink_serial_speed() const;
    void set_mavlink_serial_speed(speed_t speed);

    const char* get_isbd_serial() const;
    void set_isbd_serial(const char* path);

    speed_t get_isbd_serial_speed() const;
    void set_isbd_serial_speed(speed_t speed);

    bool get_auto_detect_serials() const;
    void set_auto_detect_serials(bool a);

    const char* get_serials() const;
    void set_serials(const char* s);

    unsigned long get_report_period() const;
    void set_report_period(unsigned long period);
};

extern SPLConfig config;
