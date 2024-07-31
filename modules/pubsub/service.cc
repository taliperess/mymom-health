
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

#include "modules/pubsub/service.h"

#include "pw_log/log.h"

namespace sense {
namespace {

pubsub_LedValue LedValueToProto(const LedValue& value) {
  pubsub_LedValue proto;
  proto.r = value.r();
  proto.g = value.g();
  proto.b = value.b();
  return proto;
}

LedValue LedValueFromProto(const pubsub_LedValue& proto) {
  return LedValue(proto.r, proto.g, proto.b);
}

pubsub_Event EventToProto(const Event& event) {
  pubsub_Event proto = pubsub_Event_init_default;

  if (std::holds_alternative<AlarmStateChange>(event)) {
    proto.which_type = pubsub_Event_alarm_tag;
    proto.type.alarm = std::get<AlarmStateChange>(event).alarm;
  } else if (std::holds_alternative<ButtonA>(event)) {
    proto.which_type = pubsub_Event_button_a_pressed_tag;
    proto.type.button_a_pressed = std::get<ButtonA>(event).pressed();
  } else if (std::holds_alternative<ButtonB>(event)) {
    proto.which_type = pubsub_Event_button_b_pressed_tag;
    proto.type.button_b_pressed = std::get<ButtonB>(event).pressed();
  } else if (std::holds_alternative<ButtonX>(event)) {
    proto.which_type = pubsub_Event_button_x_pressed_tag;
    proto.type.button_x_pressed = std::get<ButtonX>(event).pressed();
  } else if (std::holds_alternative<ButtonY>(event)) {
    proto.which_type = pubsub_Event_button_y_pressed_tag;
    proto.type.button_y_pressed = std::get<ButtonY>(event).pressed();
  } else if (std::holds_alternative<LedValueAirQualityMode>(event)) {
    proto.which_type = pubsub_Event_led_value_air_quality_tag;
    proto.type.led_value_air_quality =
        LedValueToProto(std::get<LedValueAirQualityMode>(event));
  } else if (std::holds_alternative<TimerRequest>(event)) {
    proto.which_type = pubsub_Event_timer_request_tag;
    auto& timer_request = std::get<TimerRequest>(event);
    proto.type.timer_request.token = timer_request.token;
    proto.type.timer_request.timeout_s = timer_request.timeout_s;
  } else if (std::holds_alternative<TimerExpired>(event)) {
    proto.which_type = pubsub_Event_timer_expired_tag;
    auto& timer_expired = std::get<TimerExpired>(event);
    proto.type.timer_expired.token = timer_expired.token;
  } else if (std::holds_alternative<ProximityStateChange>(event)) {
    proto.which_type = pubsub_Event_proximity_tag;
    proto.type.proximity = std::get<ProximityStateChange>(event).proximity;
  } else if (std::holds_alternative<ProximitySample>(event)) {
    proto.which_type = pubsub_Event_proximity_level_tag;
    proto.type.proximity_level = std::get<ProximitySample>(event).sample;
  } else if (std::holds_alternative<AmbientLightSample>(event)) {
    proto.which_type = pubsub_Event_ambient_light_lux_tag;
    proto.type.ambient_light_lux =
        std::get<AmbientLightSample>(event).sample_lux;
  } else if (std::holds_alternative<AirQuality>(event)) {
    proto.which_type = pubsub_Event_air_quality_tag;
    proto.type.air_quality = std::get<AirQuality>(event).score;
  } else if (std::holds_alternative<AirQualityThreshold>(event)) {
    proto.which_type = pubsub_Event_air_quality_threshold_tag;
    auto& air_quality_threshold = std::get<AirQualityThreshold>(event);
    proto.type.air_quality_threshold.alarm = air_quality_threshold.alarm;
    proto.type.air_quality_threshold.silence = air_quality_threshold.silence;
  } else if (std::holds_alternative<AlarmSilenceRequest>(event)) {
    proto.which_type = pubsub_Event_alarm_silence_tag;
    proto.type.alarm_silence = std::get<AlarmSilenceRequest>(event).seconds;
  } else if (std::holds_alternative<MorseEncodeRequest>(event)) {
    proto.which_type = pubsub_Event_morse_encode_request_tag;
    const auto& morse = std::get<MorseEncodeRequest>(event);
    auto& msg = proto.type.morse_encode_request.msg;
    msg[morse.message.copy(proto.type.morse_encode_request.msg,
                           sizeof(msg) - 1)] = '\0';
    proto.type.morse_encode_request.repeat = morse.repeat;
  } else if (std::holds_alternative<MorseCodeValue>(event)) {
    proto.which_type = pubsub_Event_morse_code_value_tag;
    const auto& morse = std::get<MorseCodeValue>(event);
    proto.type.morse_code_value.turn_on = morse.turn_on;
    proto.type.morse_code_value.message_finished = morse.message_finished;
  } else {
    PW_LOG_WARN("Unimplemented pubsub service event");
  }
  return proto;
}

pw::Result<Event> ProtoToEvent(const pubsub_Event& proto) {
  switch (proto.which_type) {
    case pubsub_Event_alarm_tag:
      return AlarmStateChange{.alarm = proto.type.alarm};
    case pubsub_Event_button_a_pressed_tag:
      return ButtonA(proto.type.button_a_pressed);
    case pubsub_Event_button_b_pressed_tag:
      return ButtonB(proto.type.button_b_pressed);
    case pubsub_Event_button_x_pressed_tag:
      return ButtonX(proto.type.button_x_pressed);
    case pubsub_Event_button_y_pressed_tag:
      return ButtonY(proto.type.button_y_pressed);
    case pubsub_Event_timer_request_tag:
      return TimerRequest{
          .token = proto.type.timer_request.token,
          .timeout_s =
              static_cast<uint16_t>(proto.type.timer_request.timeout_s),
      };
    case pubsub_Event_timer_expired_tag:
      return TimerExpired{
          .token = proto.type.timer_expired.token,
      };
    case pubsub_Event_morse_code_value_tag:
      return MorseCodeValue{
          .turn_on = proto.type.morse_code_value.turn_on,
          .message_finished = proto.type.morse_code_value.message_finished,
      };
    case pubsub_Event_led_value_air_quality_tag:
      return LedValueAirQualityMode(
          LedValueFromProto(proto.type.led_value_air_quality));
    case pubsub_Event_proximity_tag:
      return ProximityStateChange{.proximity = proto.type.proximity};
    case pubsub_Event_air_quality_tag:
      return AirQuality{.score = static_cast<uint16_t>(proto.type.air_quality)};
    case pubsub_Event_alarm_silence_tag:
      return AlarmSilenceRequest{
          .seconds = static_cast<uint16_t>(proto.type.alarm_silence)};
    default:
      return pw::Status::Unimplemented();
  }
}

}  // namespace

void PubSubService::Init(PubSub& pubsub) {
  pubsub_ = &pubsub;

  pubsub_->Subscribe(
      [this](Event event) { stream_.Write(EventToProto(event)); });
}

pw::Status PubSubService::Publish(const pubsub_Event& request,
                                  pw_protobuf_Empty& /*response*/) {
  pw::Result<Event> maybe_event = ProtoToEvent(request);
  if (!maybe_event.ok()) {
    return maybe_event.status();
  }

  if (pubsub_ != nullptr) {
    bool published = pubsub_->Publish(*maybe_event);
    PW_LOG_INFO("%s event to pubsub system",
                published ? "Published" : "Failed to publish");
  }

  return pw::OkStatus();
}

void PubSubService::Subscribe(const pw_protobuf_Empty&,
                              ServerWriter<pubsub_Event>& writer) {
  PW_LOG_INFO("Streaming pubsub events over RPC channel %u",
              writer.channel_id());
  stream_ = std::move(writer);
}

}  // namespace sense
