
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

namespace am {
namespace {

pubsub_Event EventToProto(const Event& event) {
  pubsub_Event proto = pubsub_Event_init_default;

  if (std::holds_alternative<am::AlarmStateChange>(event)) {
    proto.which_type = pubsub_Event_alarm_tag;
    proto.type.alarm = std::get<am::AlarmStateChange>(event).alarm;
  } else if (std::holds_alternative<am::VocSample>(event)) {
    proto.which_type = pubsub_Event_voc_level_tag;
    proto.type.voc_level = std::get<am::VocSample>(event).voc_level;
  } else if (std::holds_alternative<am::ProximityStateChange>(event)) {
    proto.which_type = pubsub_Event_proximity_tag;
    proto.type.proximity = std::get<am::ProximityStateChange>(event).proximity;
  } else if (std::holds_alternative<am::ButtonA>(event)) {
    proto.which_type = pubsub_Event_button_a_pressed_tag;
    proto.type.button_a_pressed = std::get<am::ButtonA>(event).pressed();
  } else if (std::holds_alternative<am::ButtonB>(event)) {
    proto.which_type = pubsub_Event_button_b_pressed_tag;
    proto.type.button_b_pressed = std::get<am::ButtonB>(event).pressed();
  } else if (std::holds_alternative<am::ButtonX>(event)) {
    proto.which_type = pubsub_Event_button_x_pressed_tag;
    proto.type.button_x_pressed = std::get<am::ButtonX>(event).pressed();
  } else if (std::holds_alternative<am::ButtonY>(event)) {
    proto.which_type = pubsub_Event_button_y_pressed_tag;
    proto.type.button_y_pressed = std::get<am::ButtonY>(event).pressed();
  } else {
    PW_LOG_WARN("Unimplemented pubsub service event");
  }

  return proto;
}

pw::Result<Event> ProtoToEvent(const pubsub_Event& proto) {
  switch (proto.which_type) {
    case pubsub_Event_alarm_tag:
      return am::AlarmStateChange{.alarm = proto.type.alarm};
    case pubsub_Event_voc_level_tag:
      return am::VocSample{.voc_level = proto.type.voc_level};
    case pubsub_Event_proximity_tag:
      return am::ProximityStateChange{.proximity = proto.type.proximity};
    case pubsub_Event_button_a_pressed_tag:
      return am::ButtonA(proto.type.button_a_pressed);
    case pubsub_Event_button_b_pressed_tag:
      return am::ButtonB(proto.type.button_b_pressed);
    case pubsub_Event_button_x_pressed_tag:
      return am::ButtonX(proto.type.button_x_pressed);
    case pubsub_Event_button_y_pressed_tag:
      return am::ButtonY(proto.type.button_y_pressed);
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

}  // namespace am
