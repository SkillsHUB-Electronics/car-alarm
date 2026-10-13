#include "Commands.h"
#include "Power.h"
#include <Wire.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "Classifier.h"
#include "Settings.h"
#include "config.h"

namespace {
bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void err(Print &out, const __FlashStringHelper *cmd) {
  out.print(F("ERR,"));
  out.print(cmd);
  out.print('\n');
}

bool is(const char *a, const char *word) { return strcasecmp(a, word) == 0; }
}  // namespace

void Commands::begin(BtLink *link, DistanceSensor *dist, Motion *motion, Alerts *alerts, Display *display, bool *motionOk) {
  link_ = link;
  dist_ = dist;
  motion_ = motion;
  alerts_ = alerts;
  display_ = display;
  motionOk_ = motionOk;
}

void Commands::update(unsigned long now) {
  poll(Serial, usb_, now);
  poll(link_->ble(), ble_, now);
  if (io_ && now - ioLast_ >= IO_TIMEOUT_MS) {
    ioOff();
    link_->both().print(F("RSP,IO,OFF\n"));
  }
}

// Reads whatever is waiting, one line at a time; over-long lines are dropped up to their newline.
void Commands::poll(Stream &port, Channel &ch, unsigned long now) {
  while (port.available() > 0) {
    char c = (char)port.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!ch.overflow && ch.len > 0) {
        ch.buf[ch.len] = '\0';
        handle(ch.buf, port, now);
      }
      ch.len = 0;
      ch.overflow = false;
      continue;
    }
    if (ch.len >= CMD_LINE_MAX - 1) {
      ch.overflow = true;
    } else {
      ch.buf[ch.len++] = c;
    }
  }
}

void Commands::handle(char *line, Print &out, unsigned long now) {
  char *a[10];
  uint8_t n = 0;
  for (char *p = strtok(line, ","); p && n < 10; p = strtok(nullptr, ",")) a[n++] = p;
  if (n == 0) return;

  if (is(a[0], "PING") || is(a[0], "VER")) {
    out.print(F("RSP,VER," FW_VERSION ","));
    out.print(Classifier::present() ? 1 : 0);
    out.print('\n');
  } else if (is(a[0], "CFG")) {
    cmdCfg(a, n, out);
  } else if (is(a[0], "HW")) {
    cmdHw(a, n, out);
  } else if (is(a[0], "PWR") && n == 2 && is(a[1], "GET")) {
    PowerReading r = power.read();
    out.print(F("RSP,PWR,"));
    out.print(POWER_ENABLED ? 1 : 0);
    out.print(',');
    out.print(r.inaOk ? 1 : 0);
    out.print(',');
    out.print(r.mV);
    out.print(',');
    out.print(r.mA);
    out.print(',');
    out.print(r.charging ? 1 : 0);
    out.print('\n');
  } else if (is(a[0], "PCAL")) {
    cmdPcal(a, n, out);
  } else if (is(a[0], "I2C") && n == 2 && is(a[1], "SCAN")) {
    out.print(F("RSP,I2C"));
    for (uint8_t ad = 8; ad < 120; ad++) {
      if (probe(ad)) {
        out.print(',');
        out.print(ad);
      }
    }
    out.print('\n');
  } else if (is(a[0], "CAL")) {
    cmdCal(a, n, out);
  } else if (is(a[0], "IO")) {
    cmdIo(a, n, out, now);
  } else if (is(a[0], "REC")) {
    if (n == 3 && is(a[1], "START") && atoi(a[2]) >= 0 && atoi(a[2]) <= 31) {
      label_ = (uint8_t)atoi(a[2]);
      rec_ = true;
      out.print(F("RSP,REC,1\n"));
    } else if (n == 2 && is(a[1], "STOP")) {
      rec_ = false;
      out.print(F("RSP,REC,0\n"));
    } else {
      err(out, F("REC"));
    }
  } else if (is(a[0], "DBG") && n == 2) {
    dbg_ = atoi(a[1]) != 0;
    out.print(F("RSP,DBG,"));
    out.print(dbg_ ? 1 : 0);
    out.print('\n');
  }
  // anything else (including our own event lines echoed back) is ignored silently
}

void Commands::printCfg(Print &out) {
  out.print(F("RSP,CFG,"));
  out.print(settings.warnCm);
  out.print(',');
  out.print(settings.dangerCm);
  out.print(',');
  out.print(settings.shock100);
  out.print(',');
  out.print(settings.brake100);
  out.print(',');
  out.print(settings.brakeMs);
  out.print(',');
  out.print(settings.holdS);
  out.print(',');
  out.print(settings.buzzer);
  out.print('\n');
}

void Commands::cmdCfg(char **a, uint8_t n, Print &out) {
  if (n == 2 && is(a[1], "GET")) {
    printCfg(out);
  } else if (n == 2 && is(a[1], "RESET")) {
    settingsReset();
    printCfg(out);
  } else if (n == 9 && is(a[1], "SET")) {
    long v[7];
    for (uint8_t i = 0; i < 7; i++) v[i] = atol(a[2 + i]);
    // Range-check before narrowing so out-of-range input is rejected, not wrapped.
    if (v[2] < 0 || v[2] > 255 || v[3] < 0 || v[3] > 255 || v[4] < 0 || v[4] > 65535L || v[5] < 0 || v[5] > 255 ||
        v[6] < 0 || v[6] > 1) {
      err(out, F("CFG"));
      return;
    }
    Settings s;
    s.warnCm = (int16_t)v[0];
    s.dangerCm = (int16_t)v[1];
    s.shock100 = (uint8_t)v[2];
    s.brake100 = (uint8_t)v[3];
    s.brakeMs = (uint16_t)v[4];
    s.holdS = (uint8_t)v[5];
    s.buzzer = (uint8_t)v[6];
    if (v[0] < -1000 || v[0] > 1000 || v[1] < -1000 || v[1] > 1000 || !settingsValid(s)) {
      err(out, F("CFG"));
      return;
    }
    settings = s;
    settingsSave();
    printCfg(out);
  } else {
    err(out, F("CFG"));
  }
}

void Commands::printHw(Print &out) {
  out.print(F("RSP,HW,"));
  out.print(hw.lcdAddr);
  out.print(',');
  out.print(hw.lcdOn);
  out.print(',');
  out.print(hw.backlight);
  out.print(',');
  out.print(hw.charger);
  out.print(',');
  out.print(hw.ina);
  out.print('\n');
}

void Commands::cmdPcal(char **a, uint8_t n, Print &out) {
  if (n == 2 && (is(a[1], "GET") || is(a[1], "RESET"))) {
    if (is(a[1], "RESET")) pcalReset();
  } else if (n == 5 && is(a[1], "SET")) {
    PowerCal p;
    p.vGain = (uint16_t)atoi(a[2]);
    p.iGain = (uint16_t)atoi(a[3]);
    p.iOffsetMa = (int16_t)atoi(a[4]);
    if (!pcalValid(p)) return err(out, F("PCAL"));
    pcal = p;
    pcalSave();
  } else {
    return err(out, F("PCAL"));
  }
  out.print(F("RSP,PCAL,"));
  out.print(pcal.vGain);
  out.print(',');
  out.print(pcal.iGain);
  out.print(',');
  out.print(pcal.iOffsetMa);
  out.print('\n');
}

void Commands::cmdHw(char **a, uint8_t n, Print &out) {
  if (n == 2 && is(a[1], "GET")) {
    printHw(out);
  } else if (n == 2 && is(a[1], "RESET")) {
    hwReset();
    display_->apply();
    printHw(out);
  } else if (n == 7 && is(a[1], "SET")) {
    long v[5];
    for (uint8_t i = 0; i < 5; i++) v[i] = atol(a[2 + i]);
    if (v[0] < 0 || v[0] > 255 || v[1] < 0 || v[1] > 1 || v[2] < 0 || v[2] > 1 || v[3] < 0 || v[3] > 1 || v[4] < 0 ||
        v[4] > 1) {
      err(out, F("HW"));
      return;
    }
    HwConfig h;
    h.lcdAddr = (uint8_t)v[0];
    h.lcdOn = (uint8_t)v[1];
    h.backlight = (uint8_t)v[2];
    h.charger = (uint8_t)v[3];
    h.ina = (uint8_t)v[4];
    if (!hwValid(h)) {
      err(out, F("HW"));
      return;
    }
    hw = h;
    hwSave();
    display_->apply();
    printHw(out);
  } else {
    err(out, F("HW"));
  }
}

void Commands::printCal(Print &out) {
  out.print(F("RSP,CAL,"));
  out.print(calib.gain, 4);
  out.print(',');
  out.print(calib.offset, 2);
  out.print('\n');
}

void Commands::cmdCal(char **a, uint8_t n, Print &out) {
  if (n >= 2 && is(a[1], "GET")) {
    printCal(out);
  } else if (n >= 2 && is(a[1], "RESET")) {
    calibReset();
    printCal(out);
  } else if (n >= 2 && is(a[1], "RAW")) {
    int reps = (n >= 3) ? atoi(a[2]) : 10;
    if (reps < 1) reps = 1;
    if (reps > 20) reps = 20;
    int16_t v = dist_->rawX10((uint8_t)reps);
    if (v < 0) {
      out.print(F("ERR,CAL,NOECHO\n"));
    } else {
      out.print(F("RSP,CAL,RAW,"));
      out.print(v);
      out.print('\n');
    }
  } else if (n == 4 && is(a[1], "SET")) {
    float g = atof(a[2]);
    float o = atof(a[3]);
    if (!calibValid(g, o)) {
      err(out, F("CAL"));
      return;
    }
    calib.gain = g;
    calib.offset = o;
    calibSave();
    printCal(out);
  } else {
    err(out, F("CAL"));
  }
}

void Commands::ioOff() {
  io_ = false;
  buz_ = false;
  for (uint8_t i = 0; i < 3; i++) led_[i] = false;
  alerts_->test(false, false, false, false);  // the next Alerts::update() takes over
}

void Commands::cmdIo(char **a, uint8_t n, Print &out, unsigned long now) {
  if (n == 2 && is(a[1], "ON")) {
    io_ = true;
    ioLast_ = now;
    buz_ = false;
    for (uint8_t i = 0; i < 3; i++) led_[i] = false;
    alerts_->test(false, false, false, false);
    display_->showBoot("IO TEST", "");
    out.print(F("RSP,IO,ON\n"));
    return;
  }
  if (n == 2 && is(a[1], "OFF")) {
    ioOff();
    out.print(F("RSP,IO,OFF\n"));
    return;
  }
  if (!io_) {
    out.print(F("ERR,IO,OFF\n"));
    return;
  }
  ioLast_ = now;

  if (n == 4 && is(a[1], "LED") && strchr("RYGryg", a[2][0]) && a[2][0] != '\0') {
    char c = toupper(a[2][0]);
    led_[c == 'R' ? 0 : (c == 'Y' ? 1 : 2)] = atoi(a[3]) != 0;
  } else if (n == 3 && is(a[1], "BUZ")) {
    buz_ = atoi(a[2]) != 0;
  } else if (n >= 3 && is(a[1], "LCD")) {
    // strtok split the text at commas; put them back. Lines are separated by '|'.
    for (uint8_t i = 3; i < n; i++) *(a[i] - 1) = ',';
    char *sep = strchr(a[2], '|');
    if (sep) *sep++ = '\0';
    display_->showBoot(a[2], sep ? sep : "");
  } else if (n == 2 && is(a[1], "READ")) {
    int16_t d = dist_->rawX10(3);
    float v[3];
    motion_->lastAccel(v);
    out.print(F("RSP,IO,READ,"));
    out.print(d);
    for (uint8_t k = 0; k < 3; k++) {
      out.print(',');
      out.print((int)(v[k] * 1000.0f));
    }
    out.print(',');
    out.print(*motionOk_ ? 1 : 0);
    out.print(',');
    out.print(probe(hw.lcdAddr) ? 1 : 0);
    out.print('\n');
    return;
  } else {
    err(out, F("IO"));
    return;
  }
  alerts_->test(led_[0], led_[1], led_[2], buz_);
  out.print(F("RSP,IO,OK\n"));
}
