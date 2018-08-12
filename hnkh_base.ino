/*

  EXAMPLE HANUKIAH SKETCH 

  Operation:
   - "on" is tied to reset. It resets the processor and the sketch begins.
   - "off" is tied to the input pin (8). Each press advances the night
     by one.
   - A push past the 8th night turns the hanukiah off.

   Note that the hanukiah will stay on until the batteries run out if
   the user doesn't turn it off. This sketch is just to show you how
   the board works -- it's not really for general use!

   Copyright (C) 2018 David Jacobowitz // South Berkeley Electronics

   This code is licensed under the Creative Commons Attribution-ShareAlike 
   license. (CC BY-SA). 
   
   License here: https://creativecommons.org/licenses/by-sa/4.0/ 

*/

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>

/* 
   the orientation of bits in the shift registers is opposite of how I
   think of it, so this routine reverses the bits so that the physical
   arrangement on the board matches the normal arrangement in computers,
   with the LSB on the right.
*/
uint8_t flip8(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

/* 
   a class that implements two 8-bit shifters that work in parallel. That
   is, it will shift out two separate 8b values to separate chains
   simultaneously.  We need this because the hanukiah is wired so that
   the left four nights and the right four nights are controlled by
   separate 8b shift registers.
*/
template < uint8_t SHIFT_CLOCK, uint8_t DATA0,
  uint8_t DATA1, uint8_t LATCH_CLOCK,
  uint8_t OENB, uint16_t SHIFTDELAY >
  class shifter_c {
    public:
      shifter_c() {
        pinMode(SHIFT_CLOCK, OUTPUT);
        pinMode(DATA0, OUTPUT);
        pinMode(DATA1, OUTPUT);
        pinMode(LATCH_CLOCK, OUTPUT);
        pinMode(OENB, OUTPUT);
        digitalWrite(SHIFT_CLOCK, LOW);
        digitalWrite(DATA0, LOW);
        digitalWrite(DATA1, LOW);
        digitalWrite(LATCH_CLOCK, LOW);
        digitalWrite(OENB, HIGH);
      }
    void shift2x8(uint8_t a, uint8_t b) {
      for (uint8_t i = 0; i < 8; i++) {
        bool d0 = a & 0x1;
        bool d1 = b & 0x1;
        digitalWrite(DATA0, d0);
        digitalWrite(DATA1, d1);
        delayMicroseconds(SHIFTDELAY);
        digitalWrite(SHIFT_CLOCK, HIGH);
        delayMicroseconds(SHIFTDELAY);
        digitalWrite(SHIFT_CLOCK, LOW);
        a >>= 1;
        b >>= 1;
      }
      delayMicroseconds(SHIFTDELAY);
      digitalWrite(LATCH_CLOCK, HIGH);
      delayMicroseconds(SHIFTDELAY);
      digitalWrite(LATCH_CLOCK, LOW);
    }
    void turnOn(bool on) {
      digitalWrite(OENB, !on);
    }
  };

// a class to manage the shamash
template < uint8_t SHM_PIN1, uint8_t SHM_PIN0 >
  class shamash_c {
    public:
      shamash_c() {
        pinMode(SHM_PIN1, OUTPUT);
        pinMode(SHM_PIN0, OUTPUT);
      }
    void set(uint8_t c) {
      digitalWrite(SHM_PIN0, c & 0x1);
      digitalWrite(SHM_PIN1, c & 0x2);
    }
  };

// a class to make a debounced button
template < uint8_t PIN >
  class bpress_c {
    private:
      uint8_t bvals;
    public:
      bpress_c(): bvals(0xff) {
        pinMode(PIN, INPUT);
      };
    bool pressed() {
      bvals <<= 1;
      bvals |= digitalRead(PIN) ? 1 : 0;
      if ((bvals & 0x0f) == 0x1) {
        return true;
      }
      return false;
    }
  };

// instantiate the shifter, shamash and button as described above
shifter_c < 4, 1, 0, 3, 2, 50 > shifter;
shamash_c < 9, 10 > shamash;
bpress_c < 8 > button;

// keep track of the night of Hanukkah
uint8_t night;

void powerdown() {
  shamash.set(0);
  // 595's in low power
  shifter.shift2x8(0, 0);
  shifter.turnOn(false);
  // processor as "off" as we can get it
  ADCSRA = 0;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();
  power_adc_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_usi_disable();
  sleep_cpu();
  sleep_disable();
}

void showNight(uint8_t night) {
  /*
    think of the lightmask as 16 bits representing the 16 lights of
    the days of the menorah (not including the shamash). Bits 0 and 1
    control the lights for the first day, bits 14 and 15 control the
    lights for the eighth day. To make a mask that represent all the
    lights for one day we can start with a mask for all days and then
    shift it right by the number of days remaining in hanuakkah
  */
  uint32_t lightmask = 0xffff >> (2 * (7 - night));
  /* 
    then we break that mask into two halves, reverse the bits to match
    the order of the physical lines on the board, and shift them out.
  */
  shifter.shift2x8(flip8(lightmask & 0xff),
    flip8((lightmask >> 8) & 0xff));
};

void setup() {
  night = 0;
  shifter.turnOn(true);
  showNight(night);
  shamash.set(0x3);
}

void loop() {
  if (button.pressed()) {
    night += 1;
    if (night == 8) powerdown();
    else showNight(night);
  }
  delay(100);
}
