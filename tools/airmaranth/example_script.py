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
"""Simple example script to call device RPCs."""

import logging

from airmaranth.device import (
    Device,
    get_device_connection,
)

_LOG = logging.getLogger(__file__)


def example_script(device: Device) -> None:
    # Make a shortcut to the EchoService.
    echo_service = device.rpcs.pw.rpc.EchoService

    # Call some RPCs and check the results.
    _LOG.info('Calling Echo(msg="Hello")')
    result = echo_service.Echo(msg='Hello')

    if result.status.ok():
        print('The status was', result.status)
        print('The message was', result.response.msg)
    else:
        print('Uh oh, this RPC returned', result.status)

    # The result object can be split into status and response when assigned.
    _LOG.info('Calling Echo(msg="Goodbye!")')
    status, response = echo_service.Echo(msg='Goodbye!')

    print(f'{status}: {response}')


def main():
    device_connection = get_device_connection()

    # Open the connection to the device.
    with device_connection as device:
        # Call RPCs.
        example_script(device)


if __name__ == '__main__':
    main()
