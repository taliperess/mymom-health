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

"""Bazel transitions for Rp2040_System.

TODO:  b/301334234 - Use platform-based flags and retire these transitions.
"""

def _rp2040_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = settings, attr

    return {
        "//command_line_option:platforms": "//targets/rp2040:platform",
        "//libraries/pw_board_led:backend": "//libraries/pw_board_led_rp2040",
        "@freertos//:freertos_config": "//targets/rp2040:freertos_config",
        "@pigweed//pw_interrupt:backend": "@pigweed//pw_interrupt_cortex_m:context",
        "@pigweed//pw_log:backend": "@pigweed//pw_log_tokenized",
        "@pigweed//pw_log:backend_impl": "@pigweed//pw_log_tokenized:impl",
        "@pigweed//pw_log_tokenized:handler_backend": "@pigweed//pw_system:log_backend",
        "@pigweed//pw_sys_io:backend": "@pigweed//pw_sys_io_rp2040",
        "@pigweed//pw_system:extra_platform_libs": "//targets/rp2040:extra_platform_libs",
    }

_rp2040_transition = transition(
    implementation = _rp2040_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
        "//libraries/pw_board_led:backend",
        "@freertos//:freertos_config",
        "@pigweed//pw_interrupt:backend",
        "@pigweed//pw_log:backend",
        "@pigweed//pw_log:backend_impl",
        "@pigweed//pw_log_tokenized:handler_backend",
        "@pigweed//pw_sys_io:backend",
        "@pigweed//pw_system:extra_platform_libs",
    ],
)

def _rp2040_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(output = out, target_file = ctx.executable.binary)
    return [DefaultInfo(files = depset([out]), executable = out)]

rp2040_binary = rule(
    _rp2040_binary_impl,
    attrs = {
        "binary": attr.label(
            doc = "cc_binary to build for rp2040 using pico-sdk.",
            cfg = _rp2040_transition,
            executable = True,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    doc = "Builds the specified binary for the pico-sdk rp2040 platform.",
)
