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

import pw_system.console

from sense.device import (
    get_all_protos,
    get_device_connection,
)


def main() -> int:
    return pw_system.console.main(
        compiled_protos=get_all_protos(),
        device_connection=get_device_connection(),
    )


if __name__ == '__main__':
    sys.exit(main())
