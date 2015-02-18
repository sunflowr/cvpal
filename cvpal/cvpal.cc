// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <avr/interrupt.h>
#include <avr/io.h>
#include "avrlib/base.h"

#include "cvpal/dac.h"
#include "cvpal/74hc595.h"
#include "cvpal/midi_handler.h"
#include "cvpal/usb_handler.h"

using namespace cvpal;

Dac<PA7> dac1;
Dac<PA6> dac2;
IC74HC595 ic74hc595;
UsbHandler usb_handler;
MidiHandler midi_handler;

volatile uint8_t control_clock_tick;

ISR(TIM0_COMPA_vect) {
  // 1kHz clock for timing trigger pulses.
  ++control_clock_tick;
}

void Init() {
  dac1.Init();
  dac2.Init();
  ic74hc595.Init();
  midi_handler.Init();
  usb_handler.Init(&midi_handler);
  
  // 1kHz timer for timing trigger pulses.
  TCCR0A = _BV(WGM01);
  TCCR0B = 4;
  OCR0A = F_CPU / 256000 - 1;
  TIMSK0 |= _BV(1);
  
  // DCO timer.
  TCCR1B = _BV(WGM13) | 2;
}

int main(void) {
  Init();
  while (1) {
    usb_handler.Poll();
    if (midi_handler.needs_refresh()) {
      midi_handler.Render();
      const State& state = midi_handler.state();

      dac1.Write(state.cv[0], state.cv[1]);
      dac2.Write(state.cv[2], state.cv[3]);
      // Combine gates.
      uint8_t gate = (uint8_t)(state.gates[0]);
      gate |= (uint8_t)(state.gates[1]) << 1;
      gate |= (uint8_t)(state.gates[2]) << 2;
      gate |= (uint8_t)(state.gates[3]) << 3;
      gate |= (uint8_t)(state.gates[4]) << 4;
      gate |= (uint8_t)(state.gates[5]) << 5;
      gate |= (uint8_t)(state.gates[6]) << 6;
      gate |= (uint8_t)(state.gates[7]) << 7;
      ic74hc595.Write(gate);
    }
    if (control_clock_tick) {
      --control_clock_tick;
      midi_handler.Tick();
    }
  }
}
