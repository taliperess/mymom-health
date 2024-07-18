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

load("@pigweed//pw_build:merge_flags.bzl", "merge_flags_for_transition_impl", "merge_flags_for_transition_outputs")
load("@pigweed//targets/rp2040:transition.bzl", "RP2_SYSTEM_FLAGS")

_overrides = {
    "//command_line_option:platforms": "//targets/rp2040:platform",
    "//apps/production:threads": "//targets/rp2040:production_app_threads",
    "//system:system": "//targets/rp2040:system",
    "@freertos//:freertos_config": "//targets/rp2040:freertos_config",
    "@pigweed//pw_system:extra_platform_libs": "//targets/rp2040:extra_platform_libs",
    "@pigweed//pw_system:io_backend": "@pigweed//pw_system:sys_io_target_io",
    "@pigweed//pw_build:default_module_config": "//system:module_config",
    "@pico-sdk//bazel/config:PICO_CLIB": "@pigweed//targets/rp2040:pico_sdk_clib_select",
    "@pico-sdk//bazel/config:PICO_TOOLCHAIN": "clang",
    "@pigweed//pw_toolchain:cortex-m_toolchain_kind": "clang",
}

def _rp2040_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = settings, attr
    return merge_flags_for_transition_impl(base = RP2_SYSTEM_FLAGS, override = _overrides)

_rp2040_transition = transition(
    implementation = _rp2040_transition_impl,
    inputs = [],
    outputs = merge_flags_for_transition_outputs(base = RP2_SYSTEM_FLAGS, override = _overrides),
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
