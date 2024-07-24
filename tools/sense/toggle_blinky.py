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
"""Toggle the Blink RPC."""

import logging

from sense.device import (
    Device,
    get_device_connection,
)

_LOG = logging.getLogger(__file__)


def main():
    device_connection = get_device_connection()

    # Open the connection to the device.
    with device_connection as device:
        # Toggle blinking.
        blinky_service = device.rpcs.blinky.Blinky

        _LOG.info('Calling Blinky.IsIdle()')
        status, response = blinky_service.IsIdle()
        if not status.ok():
            _LOG.error('Blinky.IsIdle() request failed.')

        if response.is_idle:
            _LOG.info('Blinking is idle, start blinking.')
            # Start blinking once every 1000ms forever.
            _LOG.info('Calling Blinky.Blink()')
            status, response = blinky_service.Blink(
                interval_ms=1000, blink_count=0
            )
            if not status.ok():
                _LOG.error('Blinky.Blink() request failed.')
        else:
            _LOG.info('Blinking is active, stop blinking.')
            # Stop blinking by requesting a single blink.
            _LOG.info('Calling Blinky.Blink()')
            status, response = blinky_service.Blink(
                interval_ms=0, blink_count=1
            )
            if not status.ok():
                _LOG.error('Blinky.Blink() request failed.')


if __name__ == '__main__':
    main()
