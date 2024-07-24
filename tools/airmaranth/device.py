# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Airmaranth device connection."""

import argparse
import logging
from types import ModuleType
from typing import Any

import pw_cli.log
from pw_protobuf_protos import common_pb2
from pw_rpc import echo_pb2
from pw_system.device import Device as PwSystemDevice
from pw_system.device_tracing import (
    DeviceWithTracing as PwSystemDeviceWithTracing,
)
from pw_system.device_connection import (
    add_device_args,
    DeviceConnection,
    create_device_serial_or_socket_connection,
)

from blinky_pb import blinky_pb2
from modules.air_sensor import air_sensor_pb2
from modules.board import board_pb2
from factory_pb import factory_pb2
from pubsub_pb import pubsub_pb2
import morse_code_pb2


_LOG = logging.getLogger(__file__)
_PUBSUB_LOG = logging.getLogger('device.pubsub_events')


def _led_indicator(led_value: pubsub_pb2.LedValue):
    r = led_value.r
    g = led_value.g
    b = led_value.b
    set_color = f'\x1B[48;2;{r};{g};{b}m'
    unset_color = '\x1B[m '
    return set_color + ' LED ' + unset_color


# Airmaranth specific device classes, new functions can be added here
# similar to ones on the parent pw_system.device.Device class:
# https://cs.opensource.google/pigweed/pigweed/+/main:pw_system/py/pw_system/device.py?q=%22def%20run_tests(%22
class Device(PwSystemDevice):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.pubsub_call_ = None

    def log_pubsub_events(self, filter: None | str = None):
        """Logs pubsub events.

        Args:
          filter: If provided, only events containing the `filter` string will
            be logged.
        """
        self.stop_logging_pubsub_events()

        # Hide from the root logger window.
        _PUBSUB_LOG.propagate = False

        def log_event(event: pubsub_pb2.Event):
            event_str = str(event)
            if filter is not None and filter not in event_str:
                return
            event_type = event.WhichOneof('type')
            event_value = getattr(event, event_type)
            prefix = ''
            if isinstance(event_value, pubsub_pb2.LedValue):
                prefix = _led_indicator(event_value)

            _PUBSUB_LOG.info("%s %s", prefix, str(event).replace('\n', ' '))

        self.pubsub_call_ = self.rpcs.pubsub.PubSub.Subscribe.invoke(
            on_next=lambda call_, event: log_event(event)
        )

    def stop_logging_pubsub_events(self):
        """Stops a `log_pubsub_events` call if one was started."""
        if self.pubsub_call_ is not None:
            self.pubsub_call_.cancel()
            self.pubsub_call_ = None


class DeviceWithTracing(PwSystemDeviceWithTracing):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


def get_all_protos() -> list[ModuleType]:
    return [
        air_sensor_pb2,
        blinky_pb2,
        board_pb2,
        common_pb2,
        echo_pb2,
        factory_pb2,
        morse_code_pb2,
        pubsub_pb2,
    ]


def get_device_connection(log: bool = True) -> DeviceConnection:
    if log:
        pw_cli.log.install(level=logging.DEBUG)

    parser = argparse.ArgumentParser(
        prog='airmaranth',
        description=__doc__,
    )
    parser = add_device_args(parser)
    args, _remaning_args = parser.parse_known_args()

    compiled_protos = get_all_protos()

    device_context = create_device_serial_or_socket_connection(
        device=args.device,
        baudrate=args.baudrate,
        token_databases=args.token_databases,
        compiled_protos=compiled_protos,
        socket_addr=args.socket_addr,
        ticks_per_second=args.ticks_per_second,
        serial_debug=args.serial_debug,
        rpc_logging=args.rpc_logging,
        hdlc_encoding=args.hdlc_encoding,
        channel_id=args.channel_id,
        # Device tracing is not hooked up yet for Airmaranth.
        device_tracing=False,
        device_class=Device,
        device_tracing_class=DeviceWithTracing,
    )

    return device_context
