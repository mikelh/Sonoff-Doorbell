/*
  xdrv_dfplayer.ino - DFPlayer mini MP3 support for Sonoff-Tasmota

  Copyright (C) 2017  Heiko Krupp, Lazar Obradovic and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_DFPLAYER
/*********************************************************************************************\
   written by Constanze Hasselberg 20171209
  // Derived from ESPEasy Plugin to control a MP3-player-module DFPlayer-Mini SKU:DFR0299
  // written by Jochen Krapf (jk@nerd2nerd.org)

  // Important! The module WTV020-SD look similar to the module DFPlayer-Mini but is NOT pin and command compatible!

  // Commands:
  // play,<track>        Plays the n-th track 1...3000 on SD-card in root folder. The track number is the physical ordenr - not the order displayed in file explorer!
  // stop                Stops actual playing sound
  // vol,<volume>        Set volume level 1...30
  // eq,<type>           Set the equalizer type 0=Normal, 1=Pop, 2=Rock, 3=Jazz, 4=classic, 5=Base

  // Circuit wiring
  // 1st-GPIO -> ESP TX to module RX [Pin2]
  // 5V to module VCC [Pin1] (can be more than 100mA) Note: Use a capacitor to denoise VCC
  // GND to module GND [Pin7+Pin10]
  // Speaker to module SPK_1 and SPK_2 [Pin6,Pin8] (not to GND!) Note: If speaker has to low impedance, use a resistor (like 33 Ohm) in line to speaker
  // (optional) module BUSY [Pin16] to LED driver (3.3V on idle, 0V on playing)
  // All other pins unconnected

  // Note: Notification sounds with Creative Commons Attribution license: https://notificationsounds.com/

  // Datasheet: https://www.dfrobot.com/wiki/index.php/DFDFPlayer_Mini_SKU:DFR0299
  \*********************************************************************************************/

#ifdef USE_DFPLAYER_SOFTSERIAL
#ifdef USE_SERIAL_NO_ICACHE
#include <SoftwareSerialNoIram.h>
SoftwareSerialNoIram *DFP_SoftSerial = NULL;
#else
#include <SoftwareSerial.h>
SoftwareSerial *DFP_SoftSerial = NULL;
#endif  // USE_SERIAL_NO_ICACHE
#endif // USE_DFPLAYER_SOFTSERIAL

uint32_t DFP_track = 1;
uint32_t DFP_volume = 15;
uint32_t DFP_eq = 5;


const char kDFPCommands[] PROGMEM = "PLAY|STOP|SETTINGS|STATUS";

/*
  json paramters for SETTINGS and PLAY:
  track
  volume
  eq
*/

#define DFP_PLAY 0
#define DFP_STOP 1
#define DFP_SETTINGS 2
#define DFP_STATUS 3

void DFPlayer_init(void)
{
#ifdef USE_DFPLAYER_SOFTSERIAL  
  if (DFP_SoftSerial)
    delete DFP_SoftSerial;
#ifdef USE_SERIAL_NO_ICACHE
  DFP_SoftSerial = new SoftwareSerialNoIram(SW_SERIAL_UNUSED_PIN, pin[GPIO_DFP_TXD]); // RX not required, only TX
#else
  DFP_SoftSerial = new SoftwareSerial(SW_SERIAL_UNUSED_PIN, pin[GPIO_DFP_TXD]); // RX not required, only TX
#endif  // USE_SERIAL_NO_ICACHE
  DFP_SoftSerial->begin(9600);
  // DFP_SetVol(CONFIG(0));   // set default volume
#endif
}


void DFP_SendCmd(byte cmd, int16_t data)
{
#ifdef USE_DFPLAYER_SOFTSERIAL  
  if (!DFP_SoftSerial)
    return;
#endif
  byte buffer[10] = { 0x7E, 0xFF, 0x06, 0, 0x00, 0, 0, 0, 0, 0xEF };

  buffer[3] = cmd;
  buffer[5] = data >> 8;   // high byte
  buffer[6] = data & 0xFF;   // low byte

  int16_t checksum = -(buffer[1] + buffer[2] + buffer[3] + buffer[4] + buffer[5] + buffer[6]);
  buffer[7] = checksum >> 8;   // high byte
  buffer[8] = checksum & 0xFF;   // low byte
  
#ifdef USE_DFPLAYER_SOFTSERIAL
  DFP_SoftSerial->write(buffer, 10);   //Send the byte array
#else  
    Serial.write(0x7e);
    Serial.write(0xff);
    Serial.write(0x06);
    Serial.write(cmd);
    Serial.write(0x00);
    Serial.write(data >> 8);
    Serial.write(data & 0xFF);
    Serial.write(checksum >> 8);
    Serial.write(checksum & 0xFF);
    Serial.write(0xef);
    Serial.flush();
#endif  
}


/*********************************************************************************************\
   Commands
  \*********************************************************************************************/

/*
   ArduinoJSON entry used to calculate jsonBuf: JSON_OBJECT_SIZE(3) + 40 = 96
  DFPlayer:
  { "command": "PLAY|STOP|SETTINGS", "track": 3, "volume" : 18, "eq": 5 }
*/

boolean DFPlayerCommand(char *type, uint16_t index, char *dataBuf, uint16_t data_len, int16_t payload)
{
  boolean serviced = true;
  boolean error = false;
  char dataBufUc[data_len];
  char command_text[20];
  const char *command;
  uint32_t track = 1;
  uint32_t volume = 15;
  uint32_t eq = 5;

  for (uint16_t i = 0; i <= sizeof(dataBufUc); i++) {
    dataBufUc[i] = toupper(dataBuf[i]);
  }
  if (!strcasecmp_P(type, PSTR(D_CMND_DFPLAYER))) {
    if (data_len) {
      StaticJsonBuffer<128> jsonBuf;
      JsonObject &dfp_json = jsonBuf.parseObject(dataBufUc);
      if (!dfp_json.success()) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_DFPLAYER "\":\"" D_INVALID_JSON "\"}")); // JSON decode failed
      }
      else {
#if 1
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_DFPLAYER "\":\"" D_DONE "\"}"));
#else
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_DFPLAYER "\":{\"" D_DFP_TRACK "\":%d,\"" D_DFP_VOLUME "\":%d,\"" D_DFP_EQ "\":%d}}"),
                         DFP_track, DFP_volume, DFP_eq);
#endif
        command = dfp_json[D_DFP_COMMAND];
        track = dfp_json[D_DFP_TRACK];
        volume = dfp_json[D_DFP_VOLUME];
        eq = dfp_json[D_DFP_EQ];
        if (command) {
          int command_code = GetCommandCode(command_text, sizeof(command_text), command, kDFPCommands);

          switch (command_code) {
            case DFP_PLAY:

              if (volume) {
                if (volume < 1)
                  volume = 1;
                else if (volume > 30)
                  volume = 30;
                DFP_volume = volume;
              }
              else
              {
                volume = DFP_volume;
              }
              DFP_SendCmd(0x06, volume); // volume
              delay(30);

              if (eq) {
                if (eq < 1)
                  eq = 1;
                else if (eq > 5)
                  eq = 5;
                DFP_eq = eq;
              }
              else
              {
                eq = DFP_eq;
              }
              DFP_SendCmd(0x07, eq); // eq
              delay(30);

              if (track) {
                if (track < 1)
                  track = 1;
                if (track > 2999)
                  track = 2999;
                DFP_track = track;
              }
              else
              {
                track = DFP_track;
              }
              DFP_SendCmd(0x03, (int16_t) track); // play track
              break;

            case DFP_STOP:
              DFP_SendCmd(0x0E, 0); // stop
              break;

            case DFP_SETTINGS:
              if (volume) {
                if (volume < 1)
                  volume = 1;
                else if (volume > 30)
                  volume = 30;
                DFP_volume = volume;
                //    DFP_SendCmd(0x06, DFP_volume); // volume
              }
              if (eq) {
                if (eq < 1)
                  eq = 1;
                else if (eq > 5)
                  eq = 5;
                DFP_eq = eq;
                //  DFP_SendCmd(0x07, DFP_eq); // eq
              }
              if (track) {
                if (track < 1)
                  track = 1;
                DFP_track = track;
              }
              break;
#if 1
            case DFP_STATUS:
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_DFPLAYER "\":{\"" D_DFP_TRACK "\":%d,\"" D_DFP_VOLUME "\":%d,\"" D_DFP_EQ "\":%d}}"),
                         DFP_track, DFP_volume, DFP_eq);
              MqttPublishPrefixTopic_P(6, PSTR(D_DFPLAYER));
              break;
#endif
            default:
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_DFPLAYER "\":\"" D_PROTOCOL_NOT_SUPPORTED "\"}"));
          }
        }
        else {
          error = true;
        }
      }
    }
    else {
      error = true;
    }
    if (error) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_DFPLAYER "\":\"" D_NO "\"}"));
    }
  }
  else {
    serviced = false; // Unknown command
  }
  return serviced;
}
#endif // USE_DFPlayer_SOFTSERIAL
