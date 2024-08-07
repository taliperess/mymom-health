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

import React, { useEffect, useState } from 'react'
import { getRpcService } from '../common/rpcService';
import { useAppState } from '../common/state';

function formatDate() {
    const date = new Date();
    const days = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
    const dayName = days[date.getDay()];

    let hours = date.getHours();
    const minutes = date.getMinutes().toString().padStart(2, '0');
    const ampm = hours >= 12 ? 'PM' : 'AM';
    hours = hours % 12;
    hours = hours ? hours : 12;

    const formattedTime = `${dayName} ${hours}:${minutes}${ampm}`;
    return formattedTime;
}

export function Header(){
    const [curDate, setCurDate] = useState(formatDate());
    const connected = useAppState(state => state.connected);
    const appState = useAppState();
    const rpc = getRpcService();

    useEffect(()=>{
        const interval = setInterval(()=>{
            setCurDate(formatDate());
        }, 10000);
        return ()=>{
            clearInterval(interval);
        }
    },[])

    return (
        <div className='header'>
            <div className="header-logo">
                <span className="material-symbols-outlined logo">
                    airwave
                </span>
                <h1>Pigweed Sense</h1>
            </div>
            <div className='header-timedate'>
                {curDate}
            </div>
            <div className='header-buttons'>
                {!connected && <button className='header-button' onClick={async ()=> {
                    await rpc.connect();
                    try{
                        await rpc.streamMeasure(async (reading)=>{
                        const state = await rpc.getState();
                        console.log("AirSensor.MeasureStream", reading.toObject())
                        console.log("StateManager.GetState", state.toObject())
                        appState.addReading({
                            temperature: reading.getTemperature(),
                            score: Math.round((reading.getScore() / 1024) * 100),
                            humidity: reading.getHumidity(),
                            alarmState: {
                            alarmActive: state.getAlarmActive(),
                            alarmThreshold: state.getAlarmThreshold(),
                            aqDescription: state.getAqDescription().toLowerCase() as any
                            }
                        })
                        });
                        appState.setConnected(true);
                    }
                    catch(e){
                        // Probably we are running blinky app on device.
                        // Fallback to just onboardTemp.
                        await rpc.streamTemp(async (temp)=>{
                            console.log("temp", temp)
                            appState.addReading({
                                temperature: temp,
                                score: 0,
                                humidity: 0,
                                alarmState: {
                                    alarmActive: false,
                                    alarmThreshold: 0,
                                    aqDescription: "invalid"
                                }
                            });
                        });
                        appState.setBasicMode(true);
                        appState.setConnected(true);
                    }
                    
                }}>Connect</button>}
                {connected && <button className='header-button' onClick={async ()=> {
                    await rpc.disconnect();
                    appState.setConnected(false);
                }}>Disconnect</button>}
            </div>
        </div>
    )
}