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

import { WebSerial, pw_hdlc, pw_rpc, pw_status } from "pigweedjs";
import { ProtoCollection } from "../../protos/collection/collection";
import { BlinkRequest } from "../../protos/collection/blinky/blinky_pb";
import { OnboardTempStreamRequest, OnboardTempResponse } from "../../protos/collection/board/board_pb";
import {MeasureStreamRequest, Measurement} from "../../protos/collection/air_sensor/air_sensor_pb";
import {State} from "../../protos/collection/state_manager/state_manager_pb";
class RPCService {
  transport;
  decoder;
  encoder;
  rpcAddress;
  channels;
  client;
  blinkService;
  boardTempService;
  measureService;
  stateService;
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
      .methodStub("board.Board.OnboardTempStream");
    this.measureService = this.client
      .channel()
      .methodStub("air_sensor.AirSensor.MeasureStream");
    this.stateService = this.client
      .channel()
      .methodStub("state_manager.StateManager.GetState");
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

  async disconnect(){
    await this.transport.disconnect();
  }

  async blink(times: number, intervalMs: number = 300) {
    const req = new BlinkRequest();
    req.setBlinkCount(times);
    req.setIntervalMs(intervalMs);
    const [status, response] = await this.blinkService.call(req);
  }

  async streamTemp(onTemp?: (temp: number) => void) {
    const req = new OnboardTempStreamRequest();
    req.setSampleIntervalMs(500);
    await this.boardTempService.invoke(req, (m: OnboardTempResponse)=>{
      console.log(m.toObject());
      if (onTemp) onTemp(m.getTemp());
    }, undefined, (err)=>{
      console.error(err);
    });
  }

  async streamMeasure(onMeasure?: (measurement: Measurement) => void){
    // We check if this RPC exists on device, 
    // if not we fallback to just onboardTemp.
    return new Promise(async (resolve, reject)=>{
      let resolveCalled = false;
      const req = new MeasureStreamRequest();
      req.setSampleIntervalMs(500);
      await this.measureService.invoke(req, (m: Measurement)=>{
        if (!resolveCalled) {
          resolveCalled = true;
          resolve(true);
        }
        if (onMeasure) onMeasure(m);
      }, undefined, (status: pw_status.Status)=>{
        if (status = pw_status.Status.NOT_FOUND){
          reject(status);
        }
      });
    });
  }

  async getState(): Promise<State>{
    const [status, response] = await this.stateService.call();
    return response;
  }
}


// We keep a singleton of this service.
const rpc = new RPCService();

export function getRpcService(): RPCService {
  return rpc;
}