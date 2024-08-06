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
from enum import Enum
import functools
import logging
import math
import os
from pathlib import Path
import pwd
import re
import sys
import threading
import time
from typing import Callable, Iterable, List, Tuple

from prompt_toolkit import prompt
from prompt_toolkit.patch_stdout import patch_stdout
from prompt_toolkit.formatted_text import ANSI, to_plain_text
from prompt_toolkit.shortcuts import ProgressBar

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

_LOG = logging.getLogger(__package__)
_DEVICE_LOG = logging.getLogger('rpc_device')
_FACTORY_LOG = logging.getLogger('sense_factory')

_COLOR = colors()

_ANSI_SEQUENCE_REGEX = re.compile(r'\x1b[^m]*m')


def strip_ansi(text: str):
    """Strip out ANSI escape sequences."""
    return _ANSI_SEQUENCE_REGEX.sub('', text)


def print_message(*args, **kwargs) -> None:
    """Print text normally and log the same message to _FACTORY_LOG."""
    print(*args, **kwargs)
    text = ''
    if args:
        text = strip_ansi(' '.join(args))
    _FACTORY_LOG.info(text)


def prompt_user_action(message: str) -> None:
    """Prints a prompt for a user action."""
    print_message(f'{_COLOR.yellow(">>>")} {message}')


def prompt_enter(
    message: str,
    key_prompt: str = 'Press Enter to continue...',
) -> None:
    """Prints a prompt for a user action."""

    with patch_stdout():
        prompt_fragments = [
            ('', '\n'),
            ('ansiyellow', '>>> '),
        ]
        prompt_fragments.extend(ANSI(message).__pt_formatted_text__())
        if key_prompt:
            prompt_fragments.extend(
                [
                    ('', f'\n{key_prompt}'),
                ]
            )
        for line in to_plain_text(prompt_fragments).splitlines():
            _FACTORY_LOG.info(line)
        prompt(prompt_fragments)


def prompt_yn(message: str) -> bool:
    """Prints a prompt for a yes/no response from the user."""
    with patch_stdout():
        prompt_fragments = [
            ('', '\n'),
            ('ansiyellow', '>>> '),
        ]
        prompt_fragments.extend(ANSI(message).__pt_formatted_text__())
        prompt_fragments.extend(
            [
                ('', ' [Y/n] '),
            ]
        )
        for line in to_plain_text(prompt_fragments).splitlines():
            _FACTORY_LOG.info(line)
        result = prompt(prompt_fragments)
        _FACTORY_LOG.info('%s', result)

    if not result or result.lower() == 'y':
        return True
    return False


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
    class Status(Enum):
        PASS = 0
        FAIL = 1
        SKIP = 2

    def __init__(self, name: str, rpcs):
        self.name = name
        self._notification = threading.Event()
        self._rpcs = rpcs
        self._blinky_service = rpcs.blinky.Blinky
        self.executed_tests: List[Tuple[str, Test.Status]] = []
        self.passed_tests = 0
        self.failed_tests = 0
        self.skipped_tests = 0

    @abc.abstractmethod
    def run(self) -> bool:
        """Runs the hardware test."""
        raise NotImplementedError(self.name)

    def pass_test(self, name: str) -> None:
        """Records a test as passed."""
        self.executed_tests.append((name, Test.Status.PASS))
        self.passed_tests += 1
        print_message(f'{_COLOR.green("PASS:")} {name}')
        self.blink_led(Color(0, 255, 0))

    def fail_test(self, name: str) -> None:
        """Records a test as failed."""
        self.executed_tests.append((name, Test.Status.FAIL))
        self.failed_tests += 1
        print_message(f'{_COLOR.bold_red("FAIL:")} {name}')
        self.blink_led(Color(255, 0, 0))

    def skip_test(self, name: str) -> None:
        """Records a test as skipped."""
        self.executed_tests.append((name, Test.Status.SKIP))
        self.skipped_tests += 1
        print_message(f'{_COLOR.yellow("SKIP:")} {name}')

    def set_led(self, color: Color) -> Status:
        """Sets the device's RGB LED to the specified color."""
        status, _ = self._rpcs.blinky.Blinky.SetRgb(
            hex=color.hex, brightness=255
        )
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
        pubsub_call = self._rpcs.pubsub.PubSub.Subscribe.invoke(
            on_next=self._listen_for_buttons
        )

        status, _ = self._factory_service.StartTest(
            test=factory_pb2.Test.Type.BUTTONS
        )
        assert status is Status.OK

        self._test_button_press(ButtonsTest.Button('A'))
        self._test_button_press(ButtonsTest.Button('B'))
        self._test_button_press(ButtonsTest.Button('X'))
        self._test_button_press(ButtonsTest.Button('Y'))

        status, _ = self._factory_service.EndTest(
            test=factory_pb2.Test.Type.BUTTONS
        )
        assert status is Status.OK

        pubsub_call.cancel()
        return self.failed_tests == 0

    def _test_button_press(self, button: Button) -> None:
        test_name = button.name.lower().replace(' ', '_')
        prompt_user_action(f'Press {button.name}')

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

        self._test_led(Color(255, 255, 255, 'white', _COLOR.bold_white))
        self._test_led(Color(255, 0, 0, 'red', _COLOR.red))
        self._test_led(Color(0, 255, 0, 'green', _COLOR.green))
        self._test_led(Color(0, 0, 255, 'blue', _COLOR.blue))
        self._test_led(Color(0, 0, 0, 'off'))

        status = self.unset_led()
        assert status is Status.OK

        return self.failed_tests == 0

    def _test_led(self, color: Color) -> bool:
        test_name = f'led_{color.name}'

        self.set_led(color)

        if prompt_yn(f'Is the Enviro+ LED {color.formatted_name}?'):
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
        return (
            f'{self.count} total samples; min: {self.min_value}, '
            f'max: {self.max_value}, mean: {self.mean_value}'
        )

    def print_formatted(
        self, indent: int = 0, units: str | None = None
    ) -> None:
        suffix = units if units is not None else ''
        print_message(f'{" " * indent}Samples   {self.count}')
        print_message(f'{" " * indent}Min       {self.min_value:.2f}{suffix}')
        print_message(f'{" " * indent}Max       {self.max_value:.2f}{suffix}')
        print_message(f'{" " * indent}Mean      {self.max_value:.2f}{suffix}')


def _sample_until(
    max_samples: int,
    sample_rpc_method,
    predicate: Callable[[int], bool],
    field: str = 'value',
    delay: float = 0.1,
    message: str | None = None,
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

    def _get_title(message, recent_samples) -> str:
        """Dynamic progress bar title with the last 5 sample readings."""
        text = 'Reading'
        if message:
            text = message
        if any(recent_samples):
            text += ' - '
            text += ', '.join(
                f'{i:.2f}' for i in recent_samples if i is not None
            )
        return text

    with patch_stdout():
        with ProgressBar(
            title=functools.partial(_get_title, message, recent_samples)
        ) as pb:
            for i in pb(range(max_samples)):
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
    _LIGHT_DARK_THRESHOLD = 2.5
    _LIGHT_BRIGHT_THRESHOLD = 4000

    def __init__(self, rpcs):
        super().__init__('Ltr559Test', rpcs)
        self._factory_service = rpcs.factory.Factory

    def run(self) -> bool:
        status, _ = self._factory_service.StartTest(
            test=factory_pb2.Test.Type.LTR559_PROX
        )
        if status is not Status.OK:
            return False

        print_message(
            _COLOR.bold_white('\nSetting LTR559 sensor to proximity mode.')
        )
        result = self._test_prox()

        status, _ = self._factory_service.EndTest(
            test=factory_pb2.Test.Type.LTR559_PROX
        )
        if status is not Status.OK:
            return False

        if not result:
            return False

        status, _ = self._factory_service.StartTest(
            test=factory_pb2.Test.Type.LTR559_LIGHT
        )
        if status is not Status.OK:
            return False

        print_message(
            _COLOR.bold_white('\nSetting LTR559 sensor to ambient mode.')
        )
        result = self._test_light()

        status, _ = self._factory_service.EndTest(
            test=factory_pb2.Test.Type.LTR559_LIGHT
        )
        if status is not Status.OK:
            return False

        return result

    def _test_prox(self) -> bool:
        prompt_enter('Place your Enviro+ pack in a well-lit area')

        with patch_stdout():
            with ProgressBar(title='Getting initial sensor readings') as pb:
                baseline_samples = Samples()
                for i in pb(range(5)):
                    status, response = self._factory_service.SampleLtr559Prox()
                    if status is not Status.OK:
                        return False

                    baseline_samples.update(response.value)

        print_message(_COLOR.green(' DONE'))
        baseline_samples.print_formatted(indent=4)

        print_message()
        prompt_enter('Fully cover the LIGHT sensor')

        success, samples = _sample_until(
            50,
            self._factory_service.SampleLtr559Prox,
            lambda value: value > Ltr559Test._PROX_NEAR_THRESHOLD,
            message='Reading sensor',
        )
        samples.print_formatted(indent=4)
        if not success:
            self.fail_test('ltr559_prox_near')
            return False

        self.pass_test('ltr559_prox_near')
        print_message()
        prompt_enter('Fully uncover the LIGHT sensor')

        success, samples = _sample_until(
            50,
            self._factory_service.SampleLtr559Prox,
            lambda value: abs(value - baseline_samples.mean_value) < 200,
        )
        samples.print_formatted(indent=4)
        if not success:
            self.fail_test('ltr559_prox_far')
            return False

        self.pass_test('ltr559_prox_far')
        return True

    def _test_light(self) -> bool:
        prompt_enter('Place your Enviro+ pack in an area with neutral light')
        with patch_stdout():
            with ProgressBar(title='Getting initial sensor readings') as pb:
                baseline_samples = Samples()

                for i in pb(range(5)):
                    status, response = self._factory_service.SampleLtr559Light()
                    if status is not Status.OK:
                        return False

                    baseline_samples.update(response.lux)

        print_message(_COLOR.green(' DONE'))
        baseline_samples.print_formatted(indent=4, units='lux')

        print_message()
        prompt_enter('Cover the LIGHT sensor with your finger')

        success, samples = _sample_until(
            100,
            self._factory_service.SampleLtr559Light,
            lambda value: value < Ltr559Test._LIGHT_DARK_THRESHOLD,
            field='lux',
        )
        samples.print_formatted(indent=4, units='lux')
        if not success:
            self.fail_test('ltr559_light_dark')
            return False

        self.pass_test('ltr559_light_dark')

        print_message()
        prompt_enter('Shine a light directly at the LIGHT sensor')

        success, samples = _sample_until(
            100,
            self._factory_service.SampleLtr559Light,
            lambda value: abs(value - baseline_samples.mean_value)
            > Ltr559Test._LIGHT_BRIGHT_THRESHOLD,
            field='lux',
        )
        samples.print_formatted(indent=4, units='lux')
        if not success:
            self.fail_test('ltr559_light_bright')
            return False

        self.pass_test('ltr559_light_bright')

        print_message()
        prompt_enter('Return the LIGHT sensor to its original position')

        success, samples = _sample_until(
            100,
            self._factory_service.SampleLtr559Light,
            lambda value: abs(value - baseline_samples.mean_value) < 2,
            field='lux',
        )
        samples.print_formatted(indent=4, units='lux')
        if not success:
            self.fail_test('ltr559_light_neutral')
            return False

        self.pass_test('ltr559_light_neutral')
        return True


class Bme688Test(Test):
    _POOR_GAS_RESISTANCE_THRESHOLD = 10000
    _NORMAL_GAS_RESISTANCE_THRESHOLD = 40000
    # Putting your finger on the temperature sensor for a few
    # seconds should increase the temperature past this delta.
    _HOT_TEMPERATURE_DELTA_C = 3

    def __init__(self, rpcs):
        super().__init__('Bme688Test', rpcs)
        self._factory_service = rpcs.factory.Factory
        self._air_sensor_service = rpcs.air_sensor.AirSensor

    def run(self) -> bool:
        status, _ = self._factory_service.StartTest(
            test=factory_pb2.Test.Type.BME688
        )
        if status is not Status.OK:
            return False

        gas_result = self._test_gas_sensor()
        temp_result = self._test_temperature()

        status, _ = self._factory_service.EndTest(
            test=factory_pb2.Test.Type.BME688
        )
        if status is not Status.OK:
            return False

        return gas_result and temp_result

    def _test_gas_sensor(self) -> bool:
        print_message(
            _COLOR.bold_white('\nTesting gas resistance in the BME688 sensor.')
        )
        print_message(
            "To test the BME688's gas sensor, you need an alcohol-based "
            '\nsolution. E.g. dip a cotton swab in rubbing alcohol.'
        )
        if not prompt_yn('Are you able to continue this test?'):
            self.skip_test('bme688_gas_resistance')
            return True

        def log_received(_):
            return False

        _, baseline_samples = _sample_until(
            10,
            self._air_sensor_service.Measure,
            log_received,
            field='gas_resistance',
            delay=0.25,
            message='Getting initial sensor readings',
        )
        print_message(_COLOR.green(' DONE'))
        baseline_samples.print_formatted(indent=4)

        prompt_enter(
            'Move the alcohol close to the BME688 sensor.',
            key_prompt='Press Enter to begin measuring...',
        )
        success, samples = _sample_until(
            50,
            self._air_sensor_service.Measure,
            lambda v: v < Bme688Test._POOR_GAS_RESISTANCE_THRESHOLD,
            field='gas_resistance',
            delay=0.25,
            message='Reading sensor',
        )
        samples.print_formatted(indent=4)

        if not success:
            self.fail_test('bme688_gas_resistance_poor')
            return False

        self.pass_test('bme688_gas_resistance_poor')

        prompt_enter('Move the alcohol away from the BME688 sensor')
        success, samples = _sample_until(
            50,
            self._air_sensor_service.Measure,
            lambda v: v > Bme688Test._NORMAL_GAS_RESISTANCE_THRESHOLD,
            field='gas_resistance',
            delay=0.25,
            message='Reading sensor',
        )
        samples.print_formatted(indent=4)

        if not success:
            self.fail_test('bme688_gas_resistance_normal')
            return False

        self.pass_test('bme688_gas_resistance_normal')
        return True

    def _test_temperature(self) -> bool:
        print_message(
            _COLOR.bold_white("\nTesting BME688's temperature sensor.")
        )

        def log_received(_):
            return False

        _, baseline_samples = _sample_until(
            10,
            self._air_sensor_service.Measure,
            log_received,
            field='temperature',
            delay=0.25,
            message='Getting initial sensor readings',
        )
        print_message(_COLOR.green(' DONE'))
        baseline_samples.print_formatted(indent=4, units='C')

        prompt_enter(
            'Put your finger on the BME688 sensor to increase its temperature',
            key_prompt='Press Enter to begin measuring...',
        )
        success, samples = _sample_until(
            50,
            self._air_sensor_service.Measure,
            lambda v: v
            > baseline_samples.mean_value + Bme688Test._HOT_TEMPERATURE_DELTA_C,
            field='temperature',
            delay=0.25,
            message='Reading sensor',
        )
        samples.print_formatted(indent=4, units='C')

        if not success:
            self.fail_test('bme688_temperature_hot')
            return False

        self.pass_test('bme688_temperature_hot')

        prompt_enter(
            'Remove your finger from the BME688 sensor',
            key_prompt='Press Enter to begin measuring...',
        )
        success, samples = _sample_until(
            100,
            self._air_sensor_service.Measure,
            lambda v: abs(v - baseline_samples.mean_value) < 7.0,
            field='temperature',
            delay=0.25,
            message='Reading sensor',
        )
        samples.print_formatted(indent=4, units='C')

        if not success:
            self.fail_test('bme688_temperature_normal')
            return False

        self.pass_test('bme688_temperature_normal')
        return True


@dataclass
class FactoryRunMetadata:
    operator: str
    device_id: str
    time: datetime

    def print_formatted(self):
        print_message(f'Operator: {_COLOR.bold_white(self.operator)}')
        print_message(
            f'Date: {_COLOR.bold_white(self.time.strftime("%Y/%m/%d %H:%M:%S"))}'
        )
        print_message(
            f'Device flash ID: {_COLOR.bold_white(f"{self.device_id:x}")}'
        )


def _run_tests(run_metadata: FactoryRunMetadata, rpcs) -> bool:
    print_message()
    print_message('===========================')
    print_message('Pigweed Sense Factory Tests')
    print_message('===========================')
    run_metadata.print_formatted()

    tests_to_run = [
        LedTest(rpcs),
        ButtonsTest(rpcs),
        Ltr559Test(rpcs),
        Bme688Test(rpcs),
    ]

    print_message()
    print_message(f'{len(tests_to_run)} tests will be performed:')
    for test in tests_to_run:
        print_message(f'  - {test.name}')

    prompt_enter('Press Enter when you are ready to begin ', key_prompt='')
    print_message(_COLOR.green('Starting hardware tests.'))

    success = True

    for num, test in enumerate(tests_to_run):
        start_msg = f'[{num + 1}/{len(tests_to_run)}] Running test {test.name}'
        print_message()
        print_message('=' * len(start_msg))
        print_message(start_msg)
        print_message('=' * len(start_msg))

        if not test.run():
            success = False

    _print_report_card(run_metadata, tests_to_run)
    return success


def _print_report_card(
    run_metadata: FactoryRunMetadata,
    executed_test_suites: Iterable[Test],
) -> None:
    passed = 0
    failed = 0
    skipped = 0

    print_message()
    print_message('============')
    print_message('Test Summary')
    print_message('============')

    run_metadata.print_formatted()

    for test_suite in executed_test_suites:
        passed += test_suite.passed_tests
        failed += test_suite.failed_tests
        skipped += test_suite.skipped_tests

        print_message()
        print_message(_COLOR.bold_white(test_suite.name))
        for name, status in test_suite.executed_tests:
            if status is Test.Status.PASS:
                print_message(f'  {_COLOR.green("PASS")} | {name}')
            elif status is Test.Status.FAIL:
                print_message(f'  {_COLOR.red("FAIL")} | {name}')
            elif status is Test.Status.SKIP:
                print_message(f'  {_COLOR.yellow("SKIP")} | {name}')

    test_result_message = f'\n{passed} tests passed, {failed} tests failed'
    if skipped > 0:
        test_result_message += f', {skipped} tests skipped'
    test_result_message += '.'
    print_message(test_result_message)
    print_message('=' * 40)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--log-file',
        type=Path,
        help='File to which to write device logs. Must be an absolute path.',
    )
    args, _remaining_args = parser.parse_known_args()
    return args


def main(log_file: Path | None = None) -> int:
    now = datetime.now()
    if log_file is None:
        run_time = now.strftime('%Y%m%d%H%M%S')
        log_directory = Path(os.environ.get('BUILD_WORKSPACE_DIRECTORY', '.'))
        device_log_file = log_directory / f'factory-logs-{run_time}-device.txt'
        factory_log_file = (
            log_directory / f'factory-logs-{run_time}-operator.txt'
        )
    else:
        device_log_file = Path(f'{log_file.stem}-device{log_file.suffix}')
        factory_log_file = Path(f'{log_file.stem}-operator{log_file.suffix}')

    _DEVICE_LOG.propagate = False
    pw_cli.log.install(
        level=logging.DEBUG,
        use_color=False,
        log_file=device_log_file,
        logger=_DEVICE_LOG,
    )

    pw_cli.log.install(
        level=logging.DEBUG,
        use_color=False,
        hide_timestamp=False,
        message_format='%(message)s',
        log_file=factory_log_file,
        logger=_FACTORY_LOG,
    )

    device_connection = get_device_connection(setup_logging=False)

    exit_code = 0

    # Open the connection to the device.
    with device_connection as device:
        print_message('Connected to attached device')

        print_message(f'Device logs: {device_log_file.resolve()}')
        print_message(f'Factory logs: {factory_log_file.resolve()}')

        try:
            _, device_info = device.rpcs.factory.Factory.GetDeviceInfo()
        except RpcError as e:
            if e.status is Status.NOT_FOUND:
                print_message(
                    _COLOR.red(
                        'No factory service exists on the connected device.'
                    ),
                    file=sys.stderr,
                )
                print_message('', file=sys.stderr)
                print_message(
                    'Make sure that your Pico device is running an up-to-date factory app:',
                    file=sys.stderr,
                )
                print_message('', file=sys.stderr)
                print_message(
                    '  bazelisk run //apps/factory:flash', file=sys.stderr
                )
                print_message('', file=sys.stderr)
            else:
                print_message(
                    f'Failed to query device info: {e}', file=sys.stderr
                )
            return 1
        except RpcTimeout as e:
            print_message(
                f'Timed out while trying to query device info: {e}\n\n'
                'Please re-flash your device and try again:\n'
                '  bazelisk run //apps/factory:flash',
                file=sys.stderr,
            )
            return 1

        username = pwd.getpwuid(os.getuid()).pw_name
        run_metadata = FactoryRunMetadata(username, device_info.flash_id, now)

        try:
            if not _run_tests(run_metadata, device.rpcs):
                exit_code = 1
        except KeyboardInterrupt:
            # Turn off the LED if it was on when tests were interrupted.
            device.rpcs.blinky.Blinky.SetRgb(hex=0, brightness=0)
            print_message('\nCtrl-C detected, exiting.')

    print_message(f'Device logs written to {device_log_file.resolve()}')
    print_message(f'Factory logs written to {factory_log_file.resolve()}')
    return exit_code


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
