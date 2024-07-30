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
import math
from pathlib import Path
import sys
import threading
import time
from typing import Callable, Tuple

from factory_pb import factory_pb2
from pubsub_pb import pubsub_pb2
from pw_cli.color import colors
from pw_rpc.callback_client import RpcError, RpcTimeout
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
        self.skipped_tests = []

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

    def skip_test(self, name: str) -> None:
        """Records a test as skipped."""
        self.skipped_tests.append(name)
        print(f'{colors().yellow("SKIP:")} {name}')

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

@dataclass
class Samples:
    count: int = 0
    total_value: float = 0
    min_value: float = math.inf
    max_value: float = 0

    @property
    def mean_value(self) -> float:
        return self.total_value / self.count

    def update(self, value: float) -> None:
        self.count += 1
        self.total_value += value
        self.min_value = min(self.min_value, value)
        self.max_value = max(self.max_value, value)

    def __str__(self) -> str:
        return (f'{self.count} total samples; min: {self.min_value}, '
                f'max: {self.max_value}, mean: {self.mean_value}')

def _sample_until(
        max_samples: int,
        sample_rpc_method,
        predicate: Callable[[int], bool],
        field: str = 'value',
        delay: float = 0.1,
    ) -> Tuple[bool, Samples]:
    """Repeatedly calls an RPC until a condition is met.

    Tracks a moving average of the value of the specified RPC response field,
    invoking the provided predicate function on this average value.

    Exits as soon as the predicate returns True. If the condition is never met,
    runs until max_samples iterations, then returns False.

    Aggregate sample data is returned regardless of success.
    """

    samples = Samples()
    recent_samples = [None for _ in range(5)]

    for i in range(max_samples):
        status, response = sample_rpc_method()
        if status is not Status.OK:
            return False, samples

        value = getattr(response, field)
        recent_samples[i % len(recent_samples)] = value
        samples.update(value)

        if i >= len(recent_samples):
            moving_average = sum(recent_samples) / len(recent_samples)
            if predicate(moving_average):
                return True, samples

        time.sleep(delay)

    return False, samples


class Ltr559Test(Test):
    _PROX_NEAR_THRESHOLD = 20000
    _LIGHT_DARK_THRESHOLD = 0.5
    _LIGHT_BRIGHT_THRESHOLD = 4000

    def __init__(self, rpcs):
        super().__init__('Ltr559Test', rpcs)
        self._factory_service = rpcs.factory.Factory

    def run(self) -> bool:
        status, _ = self._factory_service.StartTest(test=factory_pb2.Test.Type.LTR559_PROX)
        if status is not Status.OK:
            return False

        print(colors().bold_white('\nSet LTR599 to proximity mode.'))
        result = self._test_prox()

        status, _ = self._factory_service.EndTest(test=factory_pb2.Test.Type.LTR559_PROX)
        if status is not Status.OK:
            return False

        if not result:
            return False

        status, _ = self._factory_service.StartTest(test=factory_pb2.Test.Type.LTR559_LIGHT)
        if status is not Status.OK:
            return False

        print(colors().bold_white('\nSet LTR599 to ambient light mode.'))
        result = self._test_light()

        status, _ = self._factory_service.EndTest(test=factory_pb2.Test.Type.LTR559_LIGHT)
        if status is not Status.OK:
            return False

        return result

    def _test_prox(self) -> bool:
        self.prompt_enter('Place your Enviro+ pack in a lit area')
        print('Getting initial sensor readings', end='', flush=True)

        baseline_samples = Samples()

        while baseline_samples.count < 5:
            status, response = self._factory_service.SampleLtr559Prox()
            if status is not Status.OK:
                return False

            baseline_samples.update(response.value)
            print('.', end='', flush=True)

        print(colors().green(' DONE'))
        print(baseline_samples)

        print()
        self.prompt('Fully cover the light sensor')

        success, samples = _sample_until(
            50,
            self._factory_service.SampleLtr559Prox,
            lambda value: value > Ltr559Test._PROX_NEAR_THRESHOLD,
        )
        print(samples)
        if not success:
            self.fail_test('ltr559_prox_near')
            return False

        self.pass_test('ltr559_prox_near')
        print()
        self.prompt('Fully uncover the light sensor')

        success, samples = _sample_until(
            50,
            self._factory_service.SampleLtr559Prox,
            lambda value: abs(value - baseline_samples.mean_value) < 200,
        )
        print(samples)
        if not success:
            self.fail_test('ltr559_prox_far')
            return False

        self.pass_test('ltr559_prox_far')
        return True

    def _test_light(self) -> bool:
        self.prompt_enter('Place your Enviro+ pack in an area with neutral light')
        print('Getting initial sensor readings', end='', flush=True)

        baseline_samples = Samples()

        while baseline_samples.count < 5:
            status, response = self._factory_service.SampleLtr559Light()
            if status is not Status.OK:
                return False

            baseline_samples.update(response.lux)
            print('.', end='', flush=True)

        print(colors().green(' DONE'))
        print(baseline_samples)

        print()
        self.prompt('Cover the light sensor')

        success, samples = _sample_until(
            100,
            self._factory_service.SampleLtr559Light,
            lambda value: value < Ltr559Test._LIGHT_DARK_THRESHOLD,
            field='lux',
        )
        print(samples)
        if not success:
            self.fail_test('ltr559_light_dark')
            return False

        self.pass_test('ltr559_light_dark')

        print()
        self.prompt('Shine a light directly at the light sensor')

        success, samples = _sample_until(
            100,
            self._factory_service.SampleLtr559Light,
            lambda value: abs(value - baseline_samples.mean_value) > Ltr559Test._LIGHT_BRIGHT_THRESHOLD,
            field='lux',
        )
        print(samples)
        if not success:
            self.fail_test('ltr559_light_bright')
            return False

        self.pass_test('ltr559_light_bright')

        print()
        self.prompt('Return the light sensor to its original position')

        success, samples = _sample_until(
            100,
            self._factory_service.SampleLtr559Light,
            lambda value: abs(value - baseline_samples.mean_value) < 2,
            field='lux',
        )
        print(samples)
        if not success:
            self.fail_test('ltr559_light_neutral')
            return False

        self.pass_test('ltr559_light_neutral')
        return True


class Bme688Test(Test):
    _POOR_GAS_RESISTANCE_THRESHOLD = 10000
    _NORMAL_GAS_RESISTANCE_THRESHOLD = 40000
    _HOT_TEMPERATURE_THRESHOLD_C = 40

    def __init__(self, rpcs):
        super().__init__('Bme688Test', rpcs)
        self._factory_service = rpcs.factory.Factory
        self._air_sensor_service = rpcs.air_sensor.AirSensor

    def run(self) -> bool:
        status, _ = self._factory_service.StartTest(test=factory_pb2.Test.Type.BME688)
        if status is not Status.OK:
            return False

        gas_result = self._test_gas_sensor()
        temp_result = self._test_temperature()

        status, _ = self._factory_service.EndTest(test=factory_pb2.Test.Type.BME688)
        if status is not Status.OK:
            return False

        return gas_result and temp_result

    def _test_gas_sensor(self) -> bool:
        print(colors().bold_white('\nTesting gas resistance sensor.'))
        print(
            'To test the gas sensor, have some type of disinfectant, '
            'rubbing alcohol, or similar solution available.'
        )
        if not self.prompt_yn('Are you able to continue this test?'):
            self.skip_test('bme688_gas_resistance')
            return True

        def log_received(_):
            print('.', end='', flush=True)
            return False

        print('Getting initial sensor readings', end='', flush=True)
        _, baseline_samples = _sample_until(
            10,
            self._air_sensor_service.Measure,
            log_received,
            field='gas_resistance',
            delay=0.25,
        )
        print(colors().green(' DONE'))
        print(baseline_samples)

        self.prompt('Hold the sensor near the alcohol source')
        success, samples = _sample_until(
            50,
            self._air_sensor_service.Measure,
            lambda v: v < Bme688Test._POOR_GAS_RESISTANCE_THRESHOLD,
            field='gas_resistance',
            delay=0.25,
        )
        print(samples)

        if not success:
            self.fail_test('bme688_gas_resistance_poor')
            return False

        self.pass_test('bme688_gas_resistance_poor')

        self.prompt('Return the sensor to its original position')
        success, samples = _sample_until(
            50,
            self._air_sensor_service.Measure,
            lambda v: v > Bme688Test._NORMAL_GAS_RESISTANCE_THRESHOLD,
            field='gas_resistance',
            delay=0.25,
        )
        print(samples)

        if not success:
            self.fail_test('bme688_gas_resistance_normal')
            return False

        self.pass_test('bme688_gas_resistance_normal')
        return True

    def _test_temperature(self) -> bool:
        print(colors().bold_white('\nTesting temperature sensor.'))
        print('Testing the temperature sensor requires some heating apparatus.')
        if not self.prompt_yn('Are you able to continue this test?'):
            self.skip_test('bme688_temperature')
            return True

        def log_received(_):
            print('.', end='', flush=True)
            return False

        print('Getting initial sensor readings', end='', flush=True)
        _, baseline_samples = _sample_until(
            10,
            self._air_sensor_service.Measure,
            log_received,
            field='temperature',
            delay=0.25,
        )
        print(colors().green(' DONE'))
        print(baseline_samples)

        self.prompt('Heat the sensor')
        success, samples = _sample_until(
            50,
            self._air_sensor_service.Measure,
            lambda v: v > Bme688Test._HOT_TEMPERATURE_THRESHOLD_C,
            field='temperature',
            delay=0.25,
        )
        print(samples)

        if not success:
            self.fail_test('bme688_temperature_hot')
            return False

        self.pass_test('bme688_temperature_hot')

        self.prompt('Allow the sensor to cool to room temperature')
        success, samples = _sample_until(
            100,
            self._air_sensor_service.Measure,
            lambda v: abs(v - baseline_samples.mean_value) < 7.0,
            field='temperature',
            delay=0.25,
        )
        print(samples)

        if not success:
            self.fail_test('bme688_temperature_normal')
            return False

        self.pass_test('bme688_temperature_normal')
        return True


def _run_tests(rpcs) -> bool:
    print(colors().green('\nStarting hardware tests.'))

    tests_to_run = [
        LedTest(rpcs),
        ButtonsTest(rpcs),
        Ltr559Test(rpcs),
        Bme688Test(rpcs),
    ]

    all_passes = []
    all_failures = []
    all_skips = []

    for num, test in enumerate(tests_to_run):
        start_msg = f'[{num + 1}/{len(tests_to_run)}] Running test {test.name}'
        print()
        print('=' * len(start_msg))
        print(start_msg)
        print('=' * len(start_msg))

        if not test.run():
            all_failures.extend(test.failed_tests)
        all_passes.extend(test.passed_tests)
        all_skips.extend(test.skipped_tests)

    if all_failures or all_skips:
        print(f'\n{len(all_passes)} tests passed, {len(all_failures)} tests failed', end='')
        if all_skips:
            print(f', {len(all_skips)} tests skipped')
        else:
            print()
        return len(all_failures) > 0

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
        except RpcTimeout as e:
            print(f'Timed out while trying to query device info: {e}', file=sys.stderr)
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
