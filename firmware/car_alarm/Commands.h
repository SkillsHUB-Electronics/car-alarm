#pragma once
#include <Arduino.h>
#include "Alerts.h"
#include "BtLink.h"
#include "DistanceSensor.h"
#include "Display.h"
#include "Motion.h"

// Line commands from USB serial and the BLE module (PLAN.md D12). Same syntax on both; the reply goes
// back on the channel that sent the command. Malformed lines get `ERR,<cmd>`; unknown ones are ignored.
//   PING | VER                      RSP,VER,<fw>,<model 0/1>
//   CFG,GET | CFG,RESET             RSP,CFG,warn,danger,shock100,brake100,brakeMs,holdS,buzzer
//   CFG,SET,<warn>,<danger>,<shock100>,<brake100>,<brakeMs>,<holdS>,<buzzer>
//   HW,GET | HW,RESET               RSP,HW,<lcdAddr 8..119>,<lcdOn>,<backlight>,<charger>,<ina>
//   HW,SET,<lcdAddr>,<lcdOn>,<backlight>,<charger>,<ina>   (charger/ina are stored only; no driver yet)
//   PWR,GET                         RSP,PWR,<built 0/1>,<ina ok 0/1>,<mV>,<mA>,<charging 0/1>
//   I2C,SCAN                        RSP,I2C,<addr>,<addr>... (decimal addresses that answered)
//   CAL,GET | CAL,RESET             RSP,CAL,<gain>,<offset>
//   CAL,RAW[,n]                     RSP,CAL,RAW,<mean raw distance in 0.1 cm>
//   CAL,SET,<gain>,<offset>
//   IO,ON | IO,OFF                  manual output test (ends by itself after IO_TIMEOUT_MS)
//   IO,LED,<R|Y|G>,<0|1>  IO,BUZ,<0|1>  IO,LCD,<line1>[|<line2>]
//   IO,READ                         RSP,IO,READ,<raw 0.1cm>,<ax mg>,<ay mg>,<az mg>,<mpu 0/1>,<lcd 0/1>
//   REC,START,<label> | REC,STOP    stream FEAT lines (label 0..31)
//   DBG,<0|1>                       DBG,status,cm,shockDev,fwd,pkShock,pkBrake,pkAccel,ax_mg,ay_mg,az_mg every 0.5 s
class Commands {
 public:
  void begin(BtLink *link, DistanceSensor *dist, Motion *motion, Alerts *alerts, Display *display, bool *motionOk);
  void update(unsigned long now);

  bool ioActive() const { return io_; }
  bool recording() const { return rec_; }
  uint8_t label() const { return label_; }
  bool debug() const { return dbg_; }

 private:
  static constexpr uint8_t CMD_LINE_MAX = 44;
  struct Channel {
    char buf[CMD_LINE_MAX];
    uint8_t len = 0;
    bool overflow = false;
  };

  void poll(Stream &port, Channel &ch, unsigned long now);
  void handle(char *line, Print &out, unsigned long now);
  void cmdCfg(char **a, uint8_t n, Print &out);
  void cmdCal(char **a, uint8_t n, Print &out);
  void cmdIo(char **a, uint8_t n, Print &out, unsigned long now);
  void ioOff();
  void printCfg(Print &out);
  void printCal(Print &out);
  void cmdHw(char **a, uint8_t n, Print &out);
  void printHw(Print &out);

  BtLink *link_ = nullptr;
  DistanceSensor *dist_ = nullptr;
  Motion *motion_ = nullptr;
  Alerts *alerts_ = nullptr;
  Display *display_ = nullptr;
  bool *motionOk_ = nullptr;

  Channel usb_, ble_;
  bool io_ = false;
  unsigned long ioLast_ = 0;
  bool led_[3] = {false, false, false};  // R, Y, G
  bool buz_ = false;
  bool rec_ = false;
  uint8_t label_ = 0;
  bool dbg_ = false;
};
