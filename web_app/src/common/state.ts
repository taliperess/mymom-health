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

import { create } from "zustand";

type AlarmState = {
  alarmActive: boolean;
  aqDescription:
    | "superb"
    | "excellent"
    | "very good"
    | "good"
    | "okay"
    | "mediocre"
    | "bad"
    | "terrible"
    | "invalid";
  alarmThreshold: number;
};

type Reading = {
  temperature: number;
  score: number;
  humidity: number;
  alarmState: AlarmState;
};

export type HistoricReading = {
  timestamp: Date;
  data: Reading;
};

export type AppState = {
  connected: boolean;
  basicMode: boolean;
  currentReading: Reading;
  historicReadings: HistoricReading[];
  setConnected: (connected: boolean) => void;
  addReading: (reading: Reading) => void;
  setBasicMode: (basicMode: boolean) => void;
};

const getTimeMinusMs = (ms: number) =>
  new Date(
    new Date(new Date().getTime() - 60000)
      .toISOString()
      .replace("T", " ")
      .substring(0, 19),
  );
const dummyHistory: HistoricReading[] = [
  {
    timestamp: getTimeMinusMs(160000),
    data: {
      temperature: 50,
      score: 50,
      humidity: 50,
      alarmState: {
        alarmActive: false,
        aqDescription: "okay",
        alarmThreshold: 0,
      },
    },
  },
  {
    timestamp: getTimeMinusMs(120000),
    data: {
      temperature: 50,
      score: 99,
      humidity: 50,
      alarmState: {
        alarmActive: false,
        aqDescription: "superb",
        alarmThreshold: 0,
      },
    },
  },
  {
    timestamp: getTimeMinusMs(60000),
    data: {
      temperature: 50,
      score: 1,
      humidity: 50,
      alarmState: {
        alarmActive: false,
        aqDescription: "bad",
        alarmThreshold: 0,
      },
    },
  },
  {
    timestamp: getTimeMinusMs(30000),
    data: {
      temperature: 50,
      score: 79,
      humidity: 50,
      alarmState: {
        alarmActive: false,
        aqDescription: "very good",
        alarmThreshold: 0,
      },
    },
  },
  {
    timestamp: getTimeMinusMs(0),
    data: {
      temperature: 50,
      score: 99,
      humidity: 50,
      alarmState: {
        alarmActive: false,
        aqDescription: "superb",
        alarmThreshold: 0,
      },
    },
  },
];

export const useAppState = create<AppState>()((set) => ({
  connected: false,
  basicMode: false,
  currentReading: {
    temperature: 50,
    score: 99,
    humidity: 50,
    alarmState: {
      alarmActive: false,
      aqDescription: "superb",
      alarmThreshold: 0,
    },
  },
  historicReadings: [],
  setConnected: (connected: boolean) => set({ connected }),
  setBasicMode: (basicMode: boolean) => set({ basicMode }),
  addReading: (reading: Reading) =>
    set((state) => {
      console.log("new reading", reading);
      return {
        currentReading: reading,
        historicReadings: [
          ...state.historicReadings,
          { timestamp: new Date(), data: reading },
        ],
      };
    }),
}));
