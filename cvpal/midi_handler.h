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
//
// -----------------------------------------------------------------------------
// 
// Summary of conversion modes
// 
// CH1       Mono CV/Velocity.
// CH2-5     Quad CV.
// CH6       Poly CV.
// CH7       CC conversion.
// CH8       Mono CV/Gate with CC0 (undocumented)
// CH9       Mono CV/Gate with CC1 (undocumented)
// CH10      Drums 36/38 with velocity (undocumented)
// CH11      Drums
// CH12      Drums with gate
// CH13      Channel 1 calibration
// CH14      Channel 2 calibration
// CH15      Channel 3 calibration
// CH16      Channel 4 calibration

#ifndef CVPAL_MIDI_HANDLER_H_
#define CVPAL_MIDI_HANDLER_H_

#include "avrlib/base.h"
#include "cvpal/calibration_table.h"
#include "cvpal/drum_channel.h"
#include "cvpal/note_stack.h"
#include "cvpal/voice_allocator.h"

namespace cvpal {

const uint8_t kNumVoices = 4;
const uint8_t kRetriggerDuration = 2;
const uint8_t kNumDrumChannels = 8;

struct State {
  uint16_t cv[kNumVoices];
  //uint8_t gate;
  bool gates[8];
};

class MidiHandler {
 public:
  typedef void (MidiHandler::*RenderFn)();
   
  MidiHandler() { }
  ~MidiHandler() { }
  
  void Init();
  void Parse(const uint8_t* data, uint8_t size);
  void Render();
  void Tick();
  
  inline bool needs_refresh() const { return needs_refresh_; }
  inline const State& state() const { return state_; }

 private:
  void Reset();
   
  void NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
  void NoteOff(uint8_t channel, uint8_t note);
  void ControlChange(uint8_t channel, uint8_t number, uint8_t value);
  void PitchBend(uint8_t channel, uint8_t lsb, uint8_t msb);
  void RealtimeMessage(uint8_t byte);

  void RenderMonoCvGate();
  void RenderMonoCvGateWithClock();
  void RenderQuadCvGate();
  void RenderPolyCv();
  void RenderCcConversion();
  void RenderMonoCvGateCc();
  void RenderDrumVelocity();
  void RenderDrumTrigger();
  void RenderDrumGate();
  void RenderCalibration();

  inline uint16_t NoteToCv(uint8_t base, int16_t bend, uint8_t output_channel) {
    int16_t pitch = base << 7;
    pitch += bend >> 5;
    return calibration_table_[output_channel].Calibrate(pitch);
  }
  
  bool needs_refresh_;
  uint8_t most_recent_channel_;
  
  CalibrationTable calibration_table_[kNumVoices];
  uint8_t calibrated_note_;
  
  NoteStack<6> mono_allocator_[kNumVoices];
  int16_t pitch_bend_[kNumVoices];
  uint8_t control_change_[4];
  uint16_t random_value_[kNumVoices];
  uint8_t force_retrigger_[kNumVoices];
  bool legato_[kNumVoices];

  VoiceAllocator poly_allocator_;
  uint8_t active_note_[kNumVoices];
  
  DrumChannel drum_channel_[kNumDrumChannels];
  
  State state_;
  
  uint8_t clock_counter_;
  
  static const RenderFn fn_table_[];
  
  DISALLOW_COPY_AND_ASSIGN(MidiHandler);
};

}  // namespace

#endif  // CVPAL_MIDI_HANDLER_H_
