#ifndef CVPAL_74HC595_H_
#define CVPAL_74HC595_H_

#include "avrlib/base.h"

#include <avr/io.h>

namespace cvpal {

class IC74HC595 {
 public:
  IC74HC595() { }
  ~IC74HC595() { }
  
  static inline void Init() {
    // Initialize SPI
    DDRA |= _BV(PA3);  // SS
    PORTA |= _BV(PA3);
    DDRA |= _BV(PA5);  // MOSI
    DDRA |= _BV(PA4);  // SCK
  }
  
  static inline void Write(uint8_t a) {
    PORTA &= ~_BV(PA3);
    SpiSend(a);
    PORTA |= _BV(PA3);
  }
  
 private:
  enum UsiFlags {
    TICK = _BV(USIWM0) | _BV(USITC),
    TOCK = _BV(USIWM0) | _BV(USITC) | _BV(USICLK)
  };

  static inline void SpiSend(uint8_t word) {
    USIDR = word;

    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
    USICR = TICK;
    USICR = TOCK;
  }
  
  DISALLOW_COPY_AND_ASSIGN(IC74HC595);
};

}  // namespace

#endif  // CVPAL_DAC_H_
