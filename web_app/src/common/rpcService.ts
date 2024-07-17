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

import { WebSerial, pw_hdlc, pw_rpc } from "pigweedjs";
import { ProtoCollection } from "../../protos/collection/collection";
import { BlinkRequest } from "../../protos/collection/blinky/blinky_pb";
import { OnboardTempResponse } from "../../protos/collection/board/board_pb";
export class RPCService {
  transport;
  decoder;
  encoder;
  rpcAddress;
  channels;
  client;
  blinkService;
  boardTempService;
  constructor(rpcAddress = 82) {
    this.transport = new WebSerial.WebSerialTransport();
    this.decoder = new pw_hdlc.Decoder();
    this.encoder = new pw_hdlc.Encoder();
    this.rpcAddress = rpcAddress;

    this.channels = [
      new pw_rpc.Channel(1, (bytes: Uint8Array) => {
        const hdlcBytes = this.encoder.uiFrame(this.rpcAddress, bytes);
        this.transport.sendChunk(hdlcBytes);
      }),
    ];
    this.client = pw_rpc.Client.fromProtoSet(
      this.channels,
      new ProtoCollection(),
    );
    this.blinkService = this.client.channel().methodStub("blinky.Blinky.Blink");
    this.boardTempService = this.client
      .channel()
      .methodStub("board.Board.OnboardTemp");
  }

  async connect() {
    await this.transport.connect();

    this.transport.chunks.subscribe((item: any) => {
      const decoded = this.decoder.process(item);
      for (const frame of decoded) {
        if (frame.address === this.rpcAddress) {
          this.client.processPacket(frame.data);
        }
      }
    });
  }

  async blink(times: number, intervalMs: number = 300) {
    const req = new BlinkRequest();
    req.setBlinkCount(times);
    req.setIntervalMs(intervalMs);
    const [status, response] = await this.blinkService.call(req);
  }

  async getTemp(): Promise<OnboardTempResponse> {
    const [status, response] = await this.boardTempService.call();
    return response;
  }
}
