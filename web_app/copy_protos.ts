// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

import { mkdirSync, symlinkSync, unlinkSync } from "node:fs";
import path from "path";
interface ProtoSet {
  prefix: string;
  protos: string[];
}
const protos: ProtoSet[] = [
  {
    prefix: "air_sensor",
    protos: ["../modules/air_sensor/air_sensor.proto"],
  },
  {
    prefix: "blinky",
    protos: ["../modules/blinky/blinky.proto"],
  },
  {
    prefix: "pw_protobuf_protos",
    protos: [
      "./node_modules/pigweedjs/dist/protos/pw_protobuf/pw_protobuf_protos/common.proto",
    ],
  },
  {
    prefix: "board",
    protos: ["../modules/board/board.proto"],
  },
  {
    prefix: "state_manager",
    protos: ["../modules/state_manager/state_manager.proto"],
  },
];

/**
 * Creates symbolic links for protobuf files.
 *
 * @param {!Array} protos - An array of objects representing sets of protobuf
 *     files.
 * @return {!Array} - An array of paths to the created symbolic links.
 * @throws {Error} - Throws an error if the symbolic link creation fails.
 */
function symlinkProtos(protos: ProtoSet[]): void {
  const allProtos: string[] = [];

  for (const protoSet of protos) {
    mkdirSync("protos/" + protoSet.prefix, { recursive: true });
    for (const proto of protoSet.protos) {
      const baseName = path.basename(proto);
      const link = `protos/${protoSet.prefix}/${baseName}`;
      try {
        unlinkSync(link);
        // Ignore if the file doesn't exist.
        // tslint:disable-next-line:no-any
      } catch (e: any) {
        if (e.code !== "ENOENT") {
          throw e;
        }
      }
      symlinkSync(`${path.relative(path.dirname(link), proto)}`, link);
      allProtos.push(link);
    }
  }
}

symlinkProtos(protos);
