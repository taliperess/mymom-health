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
"""Wraps pw_system's console to inject additional RPC protos."""

import argparse
import sys
from typing import Optional

from blinky_pb import blinky_pb2
from pw_protobuf_protos import common_pb2
import pw_system.console
from modules.rpc import system_pb2
from pw_rpc import echo_pb2


def main(args: Optional[argparse.Namespace] = None) -> int:
    compiled_protos = [
        common_pb2,
        echo_pb2,
        system_pb2,
        blinky_pb2,
    ]
    return pw_system.console.main_with_compiled_protos(compiled_protos, args)


if __name__ == '__main__':
    sys.exit(main())
