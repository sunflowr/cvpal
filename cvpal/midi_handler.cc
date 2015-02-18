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

#include "cvpal/midi_handler.h"

#include <avr/pgmspace.h>

#include "avrlib/op.h"

namespace cvpal {

using namespace avrlib;

static const uint16_t kOctave = 12 << 7;

/* static */
const MidiHandler::RenderFn MidiHandler::fn_table_[] PROGMEM = {
  &MidiHandler::RenderMonoCvGate,
  &MidiHandler::RenderQuadCvGate,
  &MidiHandler::RenderQuadCvGate,
  &MidiHandler::RenderQuadCvGate,
  &MidiHandler::RenderQuadCvGate,
  &MidiHandler::RenderPolyCv,
  &MidiHandler::RenderCcConversion,
  &MidiHandler::RenderMonoCvGateCc,
  &MidiHandler::RenderMonoCvGateCc,
  &MidiHandler::RenderDrumVelocity,
  &MidiHandler::RenderDrumTrigger,
  &MidiHandler::RenderDrumGate,
  &MidiHandler::RenderCalibration,
  &MidiHandler::RenderCalibration,
  &MidiHandler::RenderCalibration,
  &MidiHandler::RenderCalibration,
};

void MidiHandler::Init() {
  most_recent_channel_ = 0;

  poly_allocator_.Init();
  poly_allocator_.set_size(kNumVoices);  

  calibration_table_[0].Init(0);
  calibration_table_[1].Init(1);
  calibration_table_[2].Init(2);
  calibration_table_[3].Init(3);
  
  for (uint8_t i = 0; i < kNumDrumChannels; ++i) {
    drum_channel_[i].Stop();
  }
  
  needs_refresh_ = true;
  
  calibrated_note_ = 60;
  Reset();
}

void MidiHandler::Reset() {
  mono_allocator_[0].Clear();
  mono_allocator_[1].Clear();
  mono_allocator_[2].Clear();
  mono_allocator_[3].Clear();
  for (uint8_t i = 0; i < kNumDrumChannels; ++i) {
    drum_channel_[i].Stop();
  }
  pitch_bend_[0] = 0;
  pitch_bend_[1] = 0;
  clock_counter_ = 0;
  legato_[0] = false;
  legato_[1] = false;
  control_change_[0] = control_change_[1] = 0;
  control_change_[2] = control_change_[3] = 0;
}

void MidiHandler::Parse(const uint8_t* data, uint8_t size) {
  while (size) {
    // uint8_t cable_number = data[0] >> 4;
    uint8_t code_index = data[0] & 0xf;
    uint8_t channel = data[1] & 0xf;
    
    if (code_index != 0x0f && channel != most_recent_channel_) {
      if (!((most_recent_channel_ == 2 && channel == 3) ||
            (most_recent_channel_ == 3 && channel == 2))) {
        Reset();
      }
    }
    
    switch (code_index) {
      case 0x08:
        NoteOff(channel, data[2]);
        most_recent_channel_ = channel;
        break;
      case 0x09:
        NoteOn(channel, data[2], data[3]);
        most_recent_channel_ = channel;
        break;
      case 0x0b:
        ControlChange(channel, data[2], data[3]);
        most_recent_channel_ = channel;
        break;
      case 0x0e:
        PitchBend(channel, data[2], data[3]);
        most_recent_channel_ = channel;
        break;
      case 0x0f:
        RealtimeMessage(data[1]);
        break;
    }
    size -= 4;
    data += 4;
    needs_refresh_ = true;
  }
}

void MidiHandler::RealtimeMessage(uint8_t byte) {
  //if (most_recent_channel_ >= 0x0b && most_recent_channel_ <= 0x0d) {
    if (byte == 0xf8) {
      // Clock
      /*if (clock_counter_ == 0) {
        drum_channel_[0].Trigger(0);
      }*/
      drum_channel_[4].Trigger(0);
      ++clock_counter_;
      if(clock_counter_ == 3)
      {
        drum_channel_[5].Trigger(0);
      }
      if(clock_counter_ == 6)
      {
        drum_channel_[5].Trigger(0);
        drum_channel_[6].Trigger(0);
        clock_counter_ = 0;
      }
      /*if (most_recent_channel_ == 0x0b) {
        clock_counter_ = 0;
      } else if (most_recent_channel_ == 0x0c && clock_counter_ >= 3) {
        clock_counter_ = 0;
      } else if (most_recent_channel_ == 0x0d && clock_counter_ >= 6) {
        clock_counter_ = 0;
      }*/
    } else if (byte == 0xfa) {
      // Start
      clock_counter_ = 0;
      drum_channel_[7].Trigger(0);
    }
  //}
}

void MidiHandler::NoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  if (velocity == 0) {
    NoteOff(channel, note);
  } else {
    switch (channel) {
      case 0x00:
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x06:
      case 0x07:
      case 0x0b:
        {
          uint8_t voice = channel == 0x04 ? 3 : (channel == 0x03 ? 2 : (channel == 0x02 ? 1 : 0));
          force_retrigger_[voice] = mono_allocator_[voice].size()
              && !legato_[voice] ? kRetriggerDuration : 0;
          mono_allocator_[voice].NoteOn(note, velocity);
        }
        break;

      case 0x05:
        {
          uint8_t voice = poly_allocator_.NoteOn(note);
          force_retrigger_[voice] = active_note_[voice] != 0xff
              ? kRetriggerDuration : 0;
          active_note_[voice] = note;
        }
        break;
      
      case 0x08:
      case 0x09:
      case 0x0a:
        {
          if (note == 36) {
            drum_channel_[0].Trigger(velocity);
          } else if (note == 38) {
            drum_channel_[1].Trigger(velocity);
          } else if (note == 40) {
            drum_channel_[2].Trigger(velocity);
          } else if (note == 46) {
            drum_channel_[3].Trigger(velocity);
          }
        }
        break;
      
      case 0x0c:
      case 0x0d:
      case 0x0e:
      case 0x0f:
        {
          uint8_t calibrated_note = 42;
          for (uint8_t i = 1; i <= 8; ++i) {
            if (note == calibrated_note - 1) {
              calibrated_note_ = calibrated_note;
              calibration_table_[channel - 0x0c].Adjust(i, -1);
            } else if (note == calibrated_note + 1) {
              calibrated_note_ = calibrated_note;
              calibration_table_[channel - 0x0c].Adjust(i, +1);
            } else if (note == calibrated_note) {
              calibrated_note_ = calibrated_note;
            }
            calibrated_note += 6;
          }
        }
        break;
    }
  }
}

void MidiHandler::NoteOff(uint8_t channel, uint8_t note) {
  switch (channel) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x06:
    case 0x07:
    case 0x0b:
      {
        uint8_t voice = channel == 0x04 ? 3 : (channel == 0x03 ? 2 : (channel == 0x02 ? 1 : 0));
        uint8_t top_note = mono_allocator_[voice].most_recent_note().note;
        mono_allocator_[voice].NoteOff(note);
        if (mono_allocator_[voice].size() && 
            mono_allocator_[voice].most_recent_note().note != top_note) {
          force_retrigger_[voice] = !legato_[voice] ? kRetriggerDuration : 0;
        }
      }
      break;
      
    case 0x05:
      {
        uint8_t voice_index = poly_allocator_.NoteOff(note);
        if (voice_index < kNumVoices) {
          active_note_[voice_index] = 0xff;
        }
      }
      break;
      
    case 0x08:
    case 0x09:
    case 0x0a:
      {
        if (note == 36) {
          drum_channel_[0].Stop();
        } else if (note == 38) {
          drum_channel_[1].Stop();
        } else if (note == 40) {
          drum_channel_[2].Stop();
        } else if (note == 46) {
          drum_channel_[3].Stop();
        }
      }
      break;
      
  }
  most_recent_channel_ = channel;
}

void MidiHandler::Tick() {
  //if (most_recent_channel_ >= 0x08 && most_recent_channel_ <= 0x0d) {
    for (uint8_t i = 0; i < kNumDrumChannels; ++i) {
      drum_channel_[i].Tick();
    }
    needs_refresh_ = true;
  //}
  if (force_retrigger_[0]) {
    --force_retrigger_[0];
    needs_refresh_ = true;
  }
  if (force_retrigger_[1]) {
    --force_retrigger_[1];
    needs_refresh_ = true;
  }
}

void MidiHandler::PitchBend(uint8_t channel, uint8_t lsb, uint8_t msb) {
  int16_t value = (static_cast<uint16_t>(msb) << 7) + lsb;
  value -= 8192;
  uint8_t voice = channel == 0x04 ? 3 : (channel == 0x03 ? 2 : (channel == 0x02 ? 1 : 0));
  pitch_bend_[voice] = value;
}

void MidiHandler::ControlChange(
    uint8_t channel,
    uint8_t number,
    uint8_t value) {
  if (number >= 1 && number <= 4) {
    control_change_[number - 1] = value;
  }
  else if (channel <= 0x04) {
    if (number == 68) {
      uint8_t voice = channel == 0x04 ? 3 : (channel == 0x03 ? 2 : (channel == 0x02 ? 1 : 0));
      legato_[voice] = value >= 64;
    }
  }
}

void MidiHandler::Render() {
  RenderFn fn;
  memcpy_P(&fn, fn_table_ + most_recent_channel_, sizeof(RenderFn));
  (this->*fn)();
  needs_refresh_ = false;
}

void MidiHandler::RenderMonoCvGate() {
  if (mono_allocator_[0].size()) {
    int16_t note = mono_allocator_[0].most_recent_note().note;
    state_.cv[0] = NoteToCv(note, pitch_bend_[0], 0);
    state_.cv[1] = mono_allocator_[0].most_recent_note().velocity << 5;
    state_.cv[2] = control_change_[0] << 5;
    state_.cv[3] = control_change_[1] << 5; 
    state_.gates[0] = !force_retrigger_[0] || !state_.gates[0];
  } else {
    state_.gates[0] = false;
  }

  state_.gates[1] = state_.gates[0];
  state_.gates[2] = state_.gates[0];
  state_.gates[3] = state_.gates[0];
  state_.gates[4] = drum_channel_[4].trigger();
  state_.gates[5] = drum_channel_[5].trigger();
  state_.gates[6] = drum_channel_[6].trigger();
  state_.gates[7] = drum_channel_[7].trigger();
}

void MidiHandler::RenderQuadCvGate() {
  for (uint8_t i = 0; i < kNumVoices; ++i) {
    if (mono_allocator_[i].size()) {
      state_.cv[i] = NoteToCv(
          mono_allocator_[i].most_recent_note().note,
          pitch_bend_[i],
          i);
      state_.gates[i] = !force_retrigger_[i] || !state_.gates[i];
    } else {
      state_.gates[i] = false;
    }
  }
  state_.gates[4] = drum_channel_[4].trigger();
  state_.gates[5] = drum_channel_[5].trigger();
  state_.gates[6] = drum_channel_[6].trigger();
  state_.gates[7] = drum_channel_[7].trigger();
}

void MidiHandler::RenderPolyCv() {
  for (uint8_t i = 0; i < kNumVoices; ++i) {
    if (active_note_[i] != 0xff) {
      state_.cv[i] = NoteToCv(active_note_[i], pitch_bend_[0], i);
      state_.gates[i] = !force_retrigger_[i];
    } else {
      state_.gates[i] = false;
    }
  }
  state_.gates[4] = drum_channel_[4].trigger();
  state_.gates[5] = drum_channel_[5].trigger();
  state_.gates[6] = drum_channel_[6].trigger();
  state_.gates[7] = drum_channel_[7].trigger();
}

void MidiHandler::RenderCcConversion() {
  /*state_.cv[0] = control_change_[0] << 5;
  state_.cv[1] = control_change_[1] << 5;
  state_.gate[0] = control_change_[2] >= 64;
  state_.gate[1] = control_change_[3] >= 64;*/
}

void MidiHandler::RenderMonoCvGateCc() {
  RenderMonoCvGate();
  state_.cv[1] = control_change_[most_recent_channel_ - 6] << 5;
}

void MidiHandler::RenderDrumVelocity() {
  state_.cv[0] = drum_channel_[0].velocity() << 5;
  state_.cv[1] = drum_channel_[1].velocity() << 5;
  state_.cv[2] = drum_channel_[2].velocity() << 5;
  state_.cv[3] = drum_channel_[3].velocity() << 5;
  state_.gates[0] = drum_channel_[0].gate();
  state_.gates[1] = drum_channel_[1].gate();
  state_.gates[2] = drum_channel_[2].gate();
  state_.gates[3] = drum_channel_[3].gate();
  state_.gates[4] = drum_channel_[4].trigger();
  state_.gates[5] = drum_channel_[5].trigger();
  state_.gates[6] = drum_channel_[6].trigger();
  state_.gates[7] = drum_channel_[7].trigger();
}

void MidiHandler::RenderDrumTrigger() {
  state_.gates[0] = drum_channel_[0].trigger();
  state_.gates[1] = drum_channel_[1].trigger();
  state_.gates[2] = drum_channel_[2].trigger();
  state_.gates[3] = drum_channel_[3].trigger();
  state_.gates[4] = drum_channel_[4].trigger();
  state_.gates[5] = drum_channel_[5].trigger();
  state_.gates[6] = drum_channel_[6].trigger();
  state_.gates[7] = drum_channel_[7].trigger();
}

void MidiHandler::RenderDrumGate() {
  state_.gates[0] = drum_channel_[0].gate();
  state_.gates[1] = drum_channel_[1].gate();
  state_.gates[2] = drum_channel_[2].gate();
  state_.gates[3] = drum_channel_[3].gate();
  state_.gates[4] = drum_channel_[4].gate();
  state_.gates[5] = drum_channel_[5].gate();
  state_.gates[6] = drum_channel_[6].gate();
  state_.gates[7] = drum_channel_[7].gate();
}

void MidiHandler::RenderCalibration() {
  state_.gates[0] = true;
  state_.gates[1] = true;
  state_.gates[2] = true;
  state_.gates[3] = true;
  state_.cv[0] = NoteToCv(calibrated_note_, 0, 0);
  state_.cv[1] = NoteToCv(calibrated_note_, 0, 1);
  state_.cv[2] = NoteToCv(calibrated_note_, 0, 2);
  state_.cv[3] = NoteToCv(calibrated_note_, 0, 3);
}

}  // namespace cvpal
