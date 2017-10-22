/*
 SPLRadioRoom.cpp

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

 Created on: Oct 17, 2017
     Author: Pavel Bobov
 */

#include <unistd.h>
#include <vector>
#include <syslog.h>
#include "SPLRadioRoom.h"


SPLRadioRoom::SPLRadioRoom() :
    autopilot(),
    isbd(),
    high_latency_msg(ARDUPILOT_SYSTEM_ID, ARDUPILOT_COMPONENT_ID),
    last_report_time(0)
{
}

SPLRadioRoom::~SPLRadioRoom()
{
}

void SPLRadioRoom::print_mavlink_msg(const mavlink_message_t& msg) const
{
    syslog(LOG_DEBUG, "** nmsgid = %d, compid = %d", msg.msgid, msg.compid);
}

bool SPLRadioRoom::isbd_send_receive_message(const mavlink_message_t& mo_msg, mavlink_message_t& mt_msg, bool& received)
{
    uint8_t buf[ISBD_MAX_MT_MGS_SIZE];
    size_t buf_size = sizeof(buf);
    uint16_t len = 0;

    if (mo_msg.len != 0 && mo_msg.msgid != 0) {
        syslog(LOG_DEBUG, "Sending MO message.");
        print_mavlink_msg(mo_msg);

        len = mavlink_msg_to_send_buffer(buf, &mo_msg);
    }

    received = false;

    if (isbd.sendReceiveSBDBinary(buf, len, buf, buf_size) != ISBD_SUCCESS) {
        return false;
    }

    if (buf_size > 0) {
        mavlink_status_t mavlink_status;

        for (size_t i = 0; i < buf_size; i++) {
            if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &mt_msg, &mavlink_status)) {
                received = true;

                syslog(LOG_DEBUG, "MT message received.");
                print_mavlink_msg(mt_msg);

                break;
            }
        }
    }

    return true;
}

bool SPLRadioRoom::handle_param_set(const mavlink_message_t& msg, mavlink_message_t& ack)
{
    if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
        char param_id[17];
        mavlink_msg_param_set_get_param_id(&msg, param_id);
        if (strncmp(param_id, HL_REPORT_PERIOD_PARAM, 16) == 0) {
            float value = mavlink_msg_param_set_get_param_value(&msg);
            config.set_report_period(value);

            mavlink_param_value_t paramValue;
            paramValue.param_value = value;
            paramValue.param_count = 0;
            paramValue.param_index = 0;
            mavlink_msg_param_set_get_param_id(&msg, paramValue.param_id);
            paramValue.param_type = mavlink_msg_param_set_get_param_type(&msg);

            mavlink_msg_param_value_encode(ARDUPILOT_SYSTEM_ID, ARDUPILOT_COMPONENT_ID, &ack, &paramValue);

            return true;
        }
    }

    return false;
}

bool SPLRadioRoom::handle_mission_write(const mavlink_message_t& msg, mavlink_message_t& ack)
{
    mavlink_message_t missions[MAX_MISSION_COUNT];

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {
        syslog(LOG_DEBUG, "MISSION_COUNT MT message received.");

        uint16_t count = mavlink_msg_mission_count_get_count(&msg);

        if (count > MAX_MISSION_COUNT) {
            syslog(LOG_INFO, "Not enough memory for storing missions.");

            mavlink_mission_ack_t mission_ack;
            mission_ack.target_system = msg.sysid;
            mission_ack.target_component = msg.compid;
            mission_ack.type = MAV_MISSION_NO_SPACE;
            mavlink_msg_mission_ack_encode(ARDUPILOT_SYSTEM_ID, ARDUPILOT_COMPONENT_ID, &ack, &mission_ack);
            ack.seq = 0; //TODO: use global counter for message sequence numbers.

            return true;
        }

        mavlink_message_t mt_msg, mo_msg;
        mo_msg.len = mo_msg.msgid = 0;

        syslog(LOG_DEBUG, "Receiving mission items from ISBD.");

        uint16_t idx = 0;

        for (uint16_t i = 0; i < count * MAX_SEND_RETRIES && idx < count; i++) {
            bool received = false;

            if (isbd_send_receive_message(mo_msg, mt_msg, received)) {
                if (received && mt_msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM) {
                    syslog(LOG_DEBUG, "MISSION_ITEM MT message received.");
                    memcpy(missions + idx, &mt_msg, sizeof(mavlink_message_t));
                    idx++;
                }
            } else {
                usleep(5000000);
            }
        }

        if (idx != count) {
            syslog(LOG_INFO, "Not all mission items received.");

            mavlink_mission_ack_t mission_ack;
            mission_ack.target_system = msg.sysid;
            mission_ack.target_component = msg.compid;
            mission_ack.type = MAV_MISSION_ERROR;
            mavlink_msg_mission_ack_encode(ARDUPILOT_SYSTEM_ID, ARDUPILOT_COMPONENT_ID, &ack, &mission_ack);
            ack.seq = 0; //TODO: use global counter for message sequence numbers.

            return true;
        }

        syslog(LOG_DEBUG, "Sending mission items to ArduPilot...");

        for (int i = 0; i < MAX_SEND_RETRIES; i++) {
            if (autopilot.send_message(msg)) {
                break;
            }

            usleep(10000);
        }

        for (uint16_t i = 0; i < count; i++) {
            autopilot.send_receive_message(missions[i], ack);

            syslog(LOG_DEBUG, "Mission item sent to ArduPilot.");

            usleep(10000);
        }

        if (mavlink_msg_mission_ack_get_type(&ack) == MAV_MISSION_ACCEPTED) {
            syslog(LOG_DEBUG, "Mission accepted by ArduPilot.");
        } else {
            syslog(LOG_DEBUG, "Mission not accepted by ArduPilot: %d", mavlink_msg_mission_ack_get_type(&ack));
        }

        return true;
    }

    return false;
}

void SPLRadioRoom::isbd_session(mavlink_message_t& mo_msg)
{

    syslog(LOG_DEBUG, "ISBD session started.");

    bool received;
    bool ack_received = false;
    mavlink_message_t mt_msg;

    do {
        ack_received = false;

        if (isbd_send_receive_message(mo_msg, mt_msg, received)) {
            mo_msg.len = mo_msg.msgid = 0;
            if (received) {
                ack_received = handle_param_set(mt_msg, mo_msg);

                if (!ack_received) {
                    ack_received = handle_mission_write(mt_msg, mo_msg);
                    syslog(LOG_INFO, "MISSION_ACK received.");
                }

                if (!ack_received) {
                    ack_received = autopilot.send_receive_message(mt_msg, mo_msg);

                    if (ack_received) {
                        syslog(LOG_INFO, "ACK received from ArduPilot.");
                        print_mavlink_msg(mo_msg);
                    }
                }
            }
        }
    } while (isbd.getWaitingMessageCount() > 0 || ack_received);

    syslog(LOG_DEBUG, "ISBD session ended.");
}

/**
 * Filters out MO messages from ArduPilot.
 *
 * Returns true if the message is allowed to pass though the filter.
boolean filterMessage(const mavlink_message_t& msg) {
  //TODO: Add all relevant messages
  return false;
}
*/

void SPLRadioRoom::comm_receive()
{
    mavlink_message_t msg;

    //digitalWrite(LED_PIN, LOW);

    if (autopilot.receive_message(msg)) {
        //digitalWrite(LED_PIN, HIGH);

        high_latency_msg.update(msg);

        /*
        if (filterMessage(msg)) {
          print_mavlink_msg(msg);

          isbd_session(msg);
        }
        */
    }
}


/*
bool ISBDCallback() {
  mavlink_message_t msg;

  digitalWrite(LED_PIN, LOW);

  if (autopilot.receiveMessage(msg)) {
    digitalWrite(LED_PIN, HIGH);

    high_latency_msg.update(msg);
  }

  nss.listen();

  return true;
}
*/


/**
 * Open serial devices for autopilot and ISBD transceiver.
 *
 * Automatically detect the correct serial devices if autopilot and ISBD transceiver
 * do not respond on the devices specified by the configuration properties.
 *
 */
bool SPLRadioRoom::init()
{
    vector<string> devices;
    if (config.get_auto_detect_serials()) {
        Serial::get_serial_devices(devices);
    }

    bool autopilot_connected = autopilot.init(config.get_mavlink_serial(),
                                              config.get_mavlink_serial_speed(),
                                              devices);

    // Exclude the serial device used by autopilot from the device list used
    // for ISBD transceiver serial device auto-detection.
    for (std::vector<string>::iterator iter = devices.begin(); iter != devices.end(); ++iter) {
        if (*iter == autopilot.get_path()) {
            devices.erase(iter);
            break;
        }
    }

    string isbd_serial = config.get_isbd_serial();

    if (autopilot_connected && isbd_serial == autopilot.get_path() && devices.size() > 0) {
        syslog(LOG_WARNING,
               "Autopilot detected at serial device '%s' that was assigned to ISBD transceiver by the configuration settings.",
               autopilot.get_path().data());

        isbd_serial = devices[0];
    }

    bool isbd_connected = isbd.init(isbd_serial,
                                    config.get_isbd_serial_speed(),
                                    devices);

    last_report_time = clock();

    return autopilot_connected && isbd_connected;
}

void SPLRadioRoom::loop()
{
    // Request data streams
    uint8_t req_stream_ids[] = {MAV_DATA_STREAM_EXTRA1, MAV_DATA_STREAM_EXTRA2, MAV_DATA_STREAM_EXTENDED_STATUS,
                                MAV_DATA_STREAM_POSITION, MAV_DATA_STREAM_RAW_CONTROLLER
                               };
    uint16_t req_message_rates[] = {2, 3, 2, 2, 2};

    for (size_t i = 0; i < sizeof(req_stream_ids)/sizeof(req_stream_ids[0]); i++) {
        mavlink_message_t msg;
        mavlink_msg_request_data_stream_pack(255, 1, &msg, 1, 1, req_stream_ids[i], req_message_rates[i], 1);
        if (!autopilot.send_message(msg)) {
            syslog(LOG_WARNING, "Failed to send message to autopilot.");
        }
        usleep(10000);
    }

    for (int i = 0; i < 100; i++) {
        comm_receive();
        usleep(10000);
    }

    high_latency_msg.print();

    uint16_t mo_flag = 0, mo_msn = 0, mt_flag = 0, mt_msn = 0, ra_flag = 0, msg_waiting = 0;

    int err = isbd.getStatusExtended(mo_flag, mo_msn, mt_flag, mt_msn, ra_flag, msg_waiting);

    if (err != 0) {
        syslog(LOG_INFO, "SBDSX failed: error %d", err);
    } else {
        syslog(LOG_INFO, "Ring Alert flag: %d", ra_flag);
    }

    clock_t current_time = clock();

    unsigned long elapsedTime = (current_time - last_report_time) / CLOCKS_PER_SEC;

    syslog(LOG_INFO, "Elapsed time: %lu", elapsedTime);

    syslog(LOG_INFO, "Report period: %lu", config.get_report_period());

    // Start ISBD session if ring alert is received or HIGH_LATENCY report period is elapsed.
    if (ra_flag || elapsedTime > config.get_report_period()) {
        //high_latency_msg.print();

        mavlink_message_t msg;
        high_latency_msg.encode(msg);

        isbd_session(msg);

        last_report_time = current_time;
    }
}
