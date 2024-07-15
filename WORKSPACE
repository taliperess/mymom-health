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
workspace(name = "showcase-rp2")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "pigweed",
    # ROLL: Warning: this entry is automatically updated.
    # ROLL: Last updated 2024-07-15.
    # ROLL: By https://cr-buildbucket.appspot.com/build/8742319408131654545.
    commit = "7de8022941470f5aeb5f30540bad6669b78db5b3",
    remote = "https://pigweed-internal.googlesource.com/pigweed/pigweed",
)

# Load Pigweed's own dependencies that we'll need.
#
# TODO: b/274148833 - Don't require users to spell these out.
http_archive(
    name = "platforms",
    sha256 = "5308fc1d8865406a49427ba24a9ab53087f17f5266a7aabbfc28823f3916e1ca",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
        "https://github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
    ],
)

http_archive(
    name = "rules_cc",
    integrity = "sha256-NddP6xi6LzsIHT8bMSVJ2NtoURbN+l3xpjvmIgB6aSg=",
    strip_prefix = "rules_cc-1acf5213b6170f1f0133e273cb85ede0e732048f",
    urls = [
        "https://github.com/bazelbuild/rules_cc/archive/1acf5213b6170f1f0133e273cb85ede0e732048f.zip",
    ],
)

git_repository(
    name = "pw_toolchain",
    # ROLL: Warning: this entry is automatically updated.
    # ROLL: Last updated 2024-07-15.
    # ROLL: By https://cr-buildbucket.appspot.com/build/8742319408131654545.
    commit = "7aa0ab7ecfae509fdf6173b4903960d3567d4508",
    remote = "https://pigweed-internal.googlesource.com/pigweed/pigweed",
    strip_prefix = "pw_toolchain_bazel",
)

# Get ready to grab CIPD dependencies. For this minimal example, the only
# dependencies will be the toolchains and OpenOCD (used for flashing).
load(
    "@pigweed//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl",
    "cipd_client_repository",
    "cipd_repository",
)

cipd_client_repository()

load("@pigweed//pw_toolchain:register_toolchains.bzl", "register_pigweed_cxx_toolchains")

register_pigweed_cxx_toolchains()

# Get the OpenOCD binary (we'll use it for flashing).
cipd_repository(
    name = "openocd",
    path = "infra/3pp/tools/openocd/${os}-${arch}",
    tag = "version:2@0.11.0-3",
)

http_archive(
    name = "bazel_skylib",
    sha256 = "aede1b60709ac12b3461ee0bb3fa097b58a86fbfdb88ef7e9f90424a69043167",
    strip_prefix = "bazel-skylib-1.6.1",  # 2024-04-24
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/refs/tags/1.6.1.tar.gz"],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

# Used in modules: //pw_grpc
#
# TODO: b/345806988 - remove this fork and update to upstream HEAD.
git_repository(
    name = "io_bazel_rules_go",
    commit = "d5ba42f3ca0b8510526ed5df2cf5807bdba43856",
    remote = "https://github.com/cramertj/rules_go.git",
)

# Used in modules: //pw_grpc
http_archive(
    name = "bazel_gazelle",
    sha256 = "32938bda16e6700063035479063d9d24c60eda8d79fd4739563f50d331cb3209",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
    ],
)

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(version = "1.21.5")

gazelle_dependencies()

# load("@pigweed//pw_grpc:deps.bzl", "pw_grpc_deps")

# DISABLE gazelle:repository_macro @pigweed//pw_grpc/deps.bzl%pw_grpc_deps
# pw_grpc_deps()

http_archive(
    name = "rules_proto",
    sha256 = "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
    strip_prefix = "rules_proto-5.3.0-21.7",
    urls = [
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/5.3.0-21.7.tar.gz",
    ],
)

http_archive(
    name = "rules_python",
    sha256 = "9acc0944c94adb23fba1c9988b48768b1bacc6583b52a2586895c5b7491e2e31",
    strip_prefix = "rules_python-0.27.0",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.27.0/rules_python-0.27.0.tar.gz",
)

load("@rules_python//python:repositories.bzl", "py_repositories", "python_register_toolchains")

py_repositories()

# Set up the Python interpreter we'll need.
python_register_toolchains(
    name = "python3",
    python_version = "3.11",
)

load("@python3//:defs.bzl", "interpreter")
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "python_packages",
    python_interpreter_target = interpreter,
    requirements_darwin = "@pigweed//pw_env_setup/py/pw_env_setup/virtualenv_setup:upstream_requirements_darwin_lock.txt",
    requirements_linux = "@pigweed//pw_env_setup/py/pw_env_setup/virtualenv_setup:upstream_requirements_linux_lock.txt",
    requirements_windows = "@pigweed//pw_env_setup/py/pw_env_setup/virtualenv_setup:upstream_requirements_windows_lock.txt",
)

load("@python_packages//:requirements.bzl", "install_deps")

install_deps()

http_archive(
    name = "com_google_protobuf",
    sha256 = "616bb3536ac1fff3fb1a141450fa28b875e985712170ea7f1bfe5e5fc41e2cd8",
    strip_prefix = "protobuf-24.4",
    url = "https://github.com/protocolbuffers/protobuf/releases/download/v24.4/protobuf-24.4.tar.gz",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# Setup Nanopb protoc plugin.
# Required by: Pigweed.
# Used in modules: pw_protobuf.
http_archive(
    name = "com_github_nanopb_nanopb",
    sha256 = "3f78bf63722a810edb6da5ab5f0e76c7db13a961c2aad4ab49296e3095d0d830",
    strip_prefix = "nanopb-0.4.8",
    url = "https://github.com/nanopb/nanopb/archive/refs/tags/0.4.8.tar.gz",
)

load("@com_github_nanopb_nanopb//extra/bazel:nanopb_deps.bzl", "nanopb_deps")

nanopb_deps()

load("@com_github_nanopb_nanopb//extra/bazel:python_deps.bzl", "nanopb_python_deps")

nanopb_python_deps(interpreter)

load("@com_github_nanopb_nanopb//extra/bazel:nanopb_workspace.bzl", "nanopb_workspace")

nanopb_workspace()

http_archive(
    name = "rules_fuzzing",
    sha256 = "d9002dd3cd6437017f08593124fdd1b13b3473c7b929ceb0e60d317cb9346118",
    strip_prefix = "rules_fuzzing-0.3.2",
    urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.3.2.zip"],
)

load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")

rules_fuzzing_dependencies()

http_archive(
    name = "freertos",
    build_file = "@pigweed//third_party/freertos:freertos.BUILD.bazel",
    sha256 = "89af32b7568c504624f712c21fe97f7311c55fccb7ae6163cda7adde1cde7f62",
    strip_prefix = "FreeRTOS-Kernel-10.5.1",
    urls = ["https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V10.5.1.tar.gz"],
)

http_archive(
    name = "hal_driver",
    build_file = "@pigweed//third_party/stm32cube:stm32_hal_driver.BUILD.bazel",
    sha256 = "c8741e184555abcd153f7bdddc65e4b0103b51470d39ee0056ce2f8296b4e835",
    strip_prefix = "stm32f4xx_hal_driver-1.8.0",
    urls = ["https://github.com/STMicroelectronics/stm32f4xx_hal_driver/archive/refs/tags/v1.8.0.tar.gz"],
)

http_archive(
    name = "cmsis_device",
    build_file = "@pigweed//third_party/stm32cube:cmsis_device.BUILD.bazel",
    sha256 = "6390baf3ea44aff09d0327a3c112c6ca44418806bfdfe1c5c2803941c391fdce",
    strip_prefix = "cmsis_device_f4-2.6.8",
    urls = ["https://github.com/STMicroelectronics/cmsis_device_f4/archive/refs/tags/v2.6.8.tar.gz"],
)

http_archive(
    name = "cmsis_core",
    build_file = "@pigweed//third_party/stm32cube:cmsis_core.BUILD.bazel",
    sha256 = "f711074a546bce04426c35e681446d69bc177435cd8f2f1395a52db64f52d100",
    strip_prefix = "cmsis_core-5.4.0_cm4",
    urls = ["https://github.com/STMicroelectronics/cmsis_core/archive/refs/tags/v5.4.0_cm4.tar.gz"],
)

git_repository(
    name = "mbedtls",
    build_file = "@pigweed//:third_party/mbedtls/mbedtls.BUILD.bazel",
    # TODO: b/352601694 - This pin is dictated by picotool.
    commit = "36e6bd6926560933583dc04a7f92f69bdbafe8bd",
    remote = "https://github.com/Mbed-TLS/mbedtls.git",
)

load("@pigweed//targets/rp2040:deps.bzl", "pigweed_rp2_deps")

pigweed_rp2_deps()

load("@pigweed//pw_ide:deps.bzl", "pw_ide_deps")

pw_ide_deps()

load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")

hedron_compile_commands_setup()

http_archive(
    name = "bme68x_sensor_api",
    build_file = "@showcase-rp2//device:bme68x.BUILD.bazel",
    sha256 = "af505a1021989ad619ac9a422cbb55110629fdf5625e96b89e2257d7a089d151",
    strip_prefix = "BME68x_SensorAPI-4.4.8",
    urls = ["https://github.com/boschsensortec/BME68x_SensorAPI/archive/refs/tags/v4.4.8.tar.gz"],
)
