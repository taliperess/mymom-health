// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <variant>

#include "modules/pubsub/pubsub.h"
#include "pw_preprocessor/arguments.h"

namespace sense {

// Base for button state changes.
class ButtonStateChange {
 public:
  explicit constexpr ButtonStateChange(bool is_pressed)
      : pressed_(is_pressed) {}

  bool pressed() const { return pressed_; }

 private:
  bool pressed_;
};

// Button state changes for specific buttons.
class ButtonA : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};
class ButtonB : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};
class ButtonX : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};
class ButtonY : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};

/// Proximity sensor state change.
struct ProximityStateChange {
  bool proximity;
};

/// New proximity sample.
struct ProximitySample {
  /// Unspecified proximity units where 0 is the minimum (farthest) and 65535 is
  /// the maximum (nearest) value reported by the sensor.
  uint16_t sample;
};

/// New ambient light sample in lux.
struct AmbientLightSample {
  float sample_lux;
};

/// Air quality score that combines relative humidity and gas resistance values.
struct AirQuality {
  /// 10 bit value ranging from 0 (very poor) to 1023 (excellent).
  uint16_t score;
};

class LedValue {
 public:
  explicit constexpr LedValue(uint8_t r, uint8_t g, uint8_t b)
      : r_(r), g_(g), b_(b) {}
  explicit constexpr LedValue() : r_(0), g_(0), b_(0) {}

  constexpr uint8_t r() const { return r_; }
  constexpr uint8_t g() const { return g_; }
  constexpr uint8_t b() const { return b_; }

  constexpr bool is_off() const { return r_ == 0 && g_ == 0 && b_ == 0; }

 private:
  uint8_t r_;
  uint8_t g_;
  uint8_t b_;
};

struct TimerRequest {
  uint32_t token;
  uint16_t timeout_s;
};

struct TimerExpired {
  uint32_t token;
};

struct MorseEncodeRequest {
  std::string_view message;
  uint32_t repeat;
};

struct MorseCodeValue {
  bool turn_on;
  bool message_finished;
};

struct SenseState {
  bool alarm;
  uint16_t alarm_threshold;
  uint16_t air_quality;
  const char* air_quality_description;
};

struct StateManagerControl {
  enum Action {
    kIncrementThreshold,
    kDecrementThreshold,
    kSilenceAlarms,
  } action;

  explicit constexpr StateManagerControl(Action a) : action(a) {}
};

// This definition must be kept up to date with modules/pubsub/pubsub.proto and
// the EventType enum.
using Event = std::variant<ButtonA,
                           ButtonB,
                           ButtonX,
                           ButtonY,
                           TimerRequest,
                           TimerExpired,
                           ProximityStateChange,
                           ProximitySample,
                           AmbientLightSample,
                           AirQuality,
                           MorseEncodeRequest,
                           MorseCodeValue,
                           SenseState,
                           StateManagerControl>;

// Index versions of Event variants, to support finding the event
enum EventType : size_t {
  kButtonA,
  kButtonB,
  kButtonX,
  kButtonY,
  kTimerRequest,
  kTimerExpired,
  kProximityStateChange,
  kProximitySample,
  kAmbientLightSample,
  kAirQuality,
  kMorseEncodeRequest,
  kMorseCodeValue,
  kSenseState,
  kStateManagerControl,
  kLastEventType = kStateManagerControl,
};

static_assert(kLastEventType + 1 == std::variant_size_v<Event>,
              "The EventTypes enum must match the Event variant");

// PubSub using Sense events.
using PubSub = GenericPubSub<Event>;

}  // namespace sense
