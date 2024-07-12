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

import pw_cli.log
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
from modules.board import board_pb2
from pw_protobuf_protos import common_pb2
from pw_rpc import echo_pb2


# Airmaranth specific device classes, new functions can be added here
# similar to ones on the parent pw_system.device.Device class:
# https://cs.opensource.google/pigweed/pigweed/+/main:pw_system/py/pw_system/device.py?q=%22def%20run_tests(%22
class Device(PwSystemDevice):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


class DeviceWithTracing(PwSystemDeviceWithTracing):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


def get_device_connection() -> DeviceConnection:
    pw_cli.log.install(level=logging.INFO)

    parser = argparse.ArgumentParser(
        prog='airmaranth',
        description=__doc__,
    )
    parser = add_device_args(parser)
    args = parser.parse_args()

    compiled_protos = [
        common_pb2,
        echo_pb2,
        board_pb2,
        blinky_pb2,
    ]

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
        device_tracing=args.device_tracing,
        device_class=Device,
        device_tracing_class=DeviceWithTracing,
    )

    return device_context
