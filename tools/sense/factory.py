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
"""Runs a connected Pico Enviro+ Pack through a series of hardware tests."""

import abc
import argparse
from dataclasses import dataclass
from datetime import datetime
import logging
from pathlib import Path
import sys
import threading
import time
from typing import Callable, Tuple

from factory_pb import factory_pb2
from pubsub_pb import pubsub_pb2
from pw_cli.color import colors
from pw_rpc.callback_client import RpcError
import pw_cli.log
from pw_status import Status

from sense.device import (
    Device,
    get_device_connection,
)

_LOG = logging.getLogger(__file__)


class TestTimeoutError(Exception):
    def __init__(self, timeout: float):
        super().__init__(f'No device response detected within {timeout}s')

@dataclass
class Color:
    r: int
    g: int
    b: int
    name: str = ''
    format_fn: Callable[[str], str] = lambda s: s

    @property
    def hex(self) -> int:
        return self.r << 16 | self.g << 8 | self.b

    @property
    def formatted_name(self) -> str:
        return self.format_fn(self.name)


class Test(abc.ABC):
    def __init__(self, name: str, rpcs):
        self.name = name
        self._notification = threading.Event()
        self._rpcs = rpcs
        self._blinky_service = rpcs.blinky.Blinky
        self.passed_tests = []
        self.failed_tests = []

    @abc.abstractmethod
    def run(self) -> bool:
        """Runs the hardware test."""
        raise NotImplementedError(self.name)

    def prompt(self, message: str) -> None:
        """Prints a prompt for a user action."""
        print(f'{colors().yellow(">>>")} {message}')

    def prompt_enter(self, message: str) -> None:
        """Prints a prompt for a user action."""
        input(f'{colors().yellow(">>>")} {message}\nPress Enter to continue...')

    def prompt_yn(self, message: str) -> bool:
        """Prints a prompt for a yes/no response from the user."""
        result = input(f'{colors().yellow(">>>")} {message} [Y/n] ')
        if not result or result.lower() == 'y':
            return True
        return False

    def pass_test(self, name: str) -> None:
        """Records a test as passed."""
        self.passed_tests.append(name)
        print(f'{colors().green("PASS:")} {name}')
        self.blink_led(Color(0, 255, 0))

    def fail_test(self, name: str) -> None:
        """Records a test as failed."""
        self.failed_tests.append(name)
        print(f'{colors().bold_red("FAIL:")} {name}')
        self.blink_led(Color(255, 0, 0))

    def set_led(self, color: Color) -> Status:
        """Sets the device's RGB LED to the specified color."""
        status, _ = self._rpcs.blinky.Blinky.SetRgb(hex=color.hex, brightness=255)
        return status

    def unset_led(self) -> Status:
        """Turns off the device's LED."""
        status, _ = self._rpcs.blinky.Blinky.SetRgb(hex=0, brightness=0)
        return status

    def blink_led(self, color: Color) -> None:
        self.unset_led()
        time.sleep(0.05)
        self.set_led(color)
        time.sleep(0.05)
        self.unset_led()

class ButtonsTest(Test):
    _TEST_TIMEOUT_S = 10.0

    @dataclass
    class Button:
        button: str

        @property
        def name(self) -> str:
            return f'Button {self.button.upper()}'

        @property
        def proto_field(self) -> str:
            return f'button_{self.button.lower()}_pressed'

    def __init__(self, rpcs):
        super().__init__('ButtonsTest', rpcs)
        self._factory_service = rpcs.factory.Factory
        self._expected_event: Tuple[str, bool] | None = None

    def run(self) -> bool:
        pubsub_call = self._rpcs.pubsub.PubSub.Subscribe.invoke(on_next=self._listen_for_buttons)

        status, _ = self._factory_service.StartTest(test=factory_pb2.Test.Type.BUTTONS)
        assert status is Status.OK

        self._test_button_press(ButtonsTest.Button('A'))
        self._test_button_press(ButtonsTest.Button('B'))
        self._test_button_press(ButtonsTest.Button('X'))
        self._test_button_press(ButtonsTest.Button('Y'))

        status, _ = self._factory_service.EndTest(test=factory_pb2.Test.Type.BUTTONS)
        assert status is Status.OK

        pubsub_call.cancel()
        return len(self.failed_tests) == 0

    def _test_button_press(self, button: Button) -> None:
        test_name = button.name.lower().replace(' ', '_')
        self.prompt(f'Press {button.name}')

        try:
            # Wait for a button press event followed by a release event.
            self._expected_event = (button.proto_field, True)
            self._notification.clear()
            if not self._notification.wait(timeout=ButtonsTest._TEST_TIMEOUT_S):
                raise TestTimeoutError(ButtonsTest._TEST_TIMEOUT_S)

            self._expected_event = (button.proto_field, False)
            self._notification.clear()
            if not self._notification.wait(timeout=ButtonsTest._TEST_TIMEOUT_S):
                raise TestTimeoutError(ButtonsTest._TEST_TIMEOUT_S)
        except TestTimeoutError as e:
            self.fail_test(f'{test_name}: {e}')
            return

        self.pass_test(f'{test_name}')

    def _listen_for_buttons(self, _: Status, event: pubsub_pb2.Event) -> None:
        if self._expected_event is None:
            self._notification.set()

        field, value = self._expected_event
        if event.HasField(field):
            if getattr(event, field) == value:
                self._notification.set()


class LedTest(Test):
    def __init__(self, rpcs):
        super().__init__('LedTest', rpcs)

    def run(self) -> bool:
        status = self.unset_led()
        assert status is Status.OK

        self._test_led(Color(255, 255, 255, 'white', colors().bold_white))
        self._test_led(Color(255, 0, 0, 'red', colors().red))
        self._test_led(Color(0, 255, 0, 'green', colors().green))
        self._test_led(Color(0, 0, 255, 'blue', colors().blue))
        self._test_led(Color(0, 0, 0, 'off'))

        status = self.unset_led()
        assert status is Status.OK

        return len(self.failed_tests) == 0

    def _test_led(self, color: Color) -> bool:
        test_name = f'led_{color.name}'

        self.set_led(color)

        if self.prompt_yn(f'Is the LED {color.formatted_name}?'):
            self.pass_test(test_name)
        else:
            self.fail_test(test_name)

        return True


class Ltr559Test(Test):
    _SENSOR_DARK_THRESHOLD = 20000

    def __init__(self, rpcs):
        super().__init__('Ltr559Test', rpcs)
        self._factory_service = rpcs.factory.Factory

    def run(self) -> bool:
        status, _ = self._factory_service.StartTest(test=factory_pb2.Test.Type.LTR559)
        assert status is Status.OK

        result = self._test_ltr559()

        status, _ = self._factory_service.EndTest(test=factory_pb2.Test.Type.LTR559)
        assert status is Status.OK

        return result

    def _test_ltr559(self) -> bool:
        self.prompt_enter('Place your Enviro+ pack in a lit area')

        print('Getting initial sensor readings', end='', flush=True)

        initial_value = 0
        num_samples = 0

        while num_samples < 5:
            status, response = self._factory_service.SampleLtr559()
            if status is not Status.OK:
                return False

            initial_value += response.value
            num_samples += 1
            print('.', end='', flush=True)

        initial_value //= num_samples

        print(colors().green(' DONE'))

        self.prompt('Cover the light sensor')

        if not self._sample_until(30, lambda value: value > Ltr559Test._SENSOR_DARK_THRESHOLD):
            self.fail_test('ltr559_dark')
            return False

        self.pass_test('ltr559_dark')
        self.prompt('Uncover the light sensor')

        if not self._sample_until(30, lambda value: abs(value - initial_value) < 100):
            self.fail_test('ltr559_bright')
            return False

        self.pass_test('ltr559_bright')
        return True

    def _sample_until(self, max_samples: int, predicate: Callable[[int], bool]) -> bool:
        for _ in range(max_samples):
            status, response = self._factory_service.SampleLtr559()
            if status is not Status.OK:
                return False

            if predicate(response.value):
                return True

            time.sleep(0.1)

        return False


def _run_tests(rpcs) -> bool:
    print(colors().green('\nStarting hardware tests.'))

    tests_to_run = [
        LedTest(rpcs),
        ButtonsTest(rpcs),
        Ltr559Test(rpcs),
    ]

    all_passes = []
    all_failures = []

    for test in tests_to_run:
        start_msg = f'Running test {test.name}'
        print('')
        print('=' * len(start_msg))
        print(start_msg)
        print('=' * len(start_msg))

        if not test.run():
            all_failures.extend(test.failed_tests)
        all_passes.extend(test.passed_tests)

    if all_failures:
        print(f'\n{len(all_passes)} tests passed, {len(all_failures)} tests failed')
        return False

    print('\nAll tests passed')
    return True


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--log-file',
        type=Path,
        help='File to which to write device logs. Must be an absolute path.',
    )
    return parser.parse_args()

def main(log_file: Path | None) -> int:
    if log_file is None:
        time = datetime.now().strftime('%Y%m%d%H%M%S')
        log_file = Path(f'factory-logs-{time}.txt')

    pw_cli.log.install(
        level=logging.DEBUG,
        use_color=False,
        log_file=log_file,
    )

    device_connection = get_device_connection(log=False)

    exit_code = 0

    # Open the connection to the device.
    with device_connection as device:
        print('Connected to attached device')

        try:
            _, device_info = device.rpcs.factory.Factory.GetDeviceInfo()
        except RpcError as e:
            if e.status is Status.NOT_FOUND:
                print(colors().red('No factory service exists on the connected device.'), file=sys.stderr)
                print('', file=sys.stderr)
                print('Make sure that your Pico device is running an up-to-date factory app:', file=sys.stderr)
                print('', file=sys.stderr)
                print('  bazelisk run //apps/factory:flash', file=sys.stderr)
                print('', file=sys.stderr)
            else:
                print(f'Failed to query device info: {e}', file=sys.stderr)
            return 1

        print(f'Device flash ID: {colors().bold_white(f"{device_info.flash_id:x}")}')

        try:
            if not _run_tests(device.rpcs):
                exit_code = 1
        except KeyboardInterrupt:
            # Turn off the LED if it was on when tests were interrupted.
            device.rpcs.blinky.Blinky.SetRgb(hex=0, brightness=0)
            print('\nCtrl-C detected, exiting.')

    print(f'Device logs written to {log_file.resolve()}')
    return exit_code

if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
