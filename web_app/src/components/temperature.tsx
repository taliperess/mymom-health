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

import React from 'react'
import {LineChart} from '@mui/x-charts/LineChart';
import { Gauge, gaugeClasses } from '@mui/x-charts/Gauge';
import Card from '@mui/material/Card';
import {formatDistance} from 'date-fns/formatDistance';
import CardContent from '@mui/material/CardContent';
import { HistoricReading, useAppState } from '../common/state';
import { StatusData } from '../common/status';

export function TemperaturePage() {
    function generateChartData(historicReadings: HistoricReading[]){
      const now = new Date();
      const last40Readings = [...historicReadings].slice(-40);
      const xAxis = last40Readings.map(reading=>(
        formatDistance(reading.timestamp, now, { addSuffix: true })
      ));
      const data = last40Readings.map(reading=>
        reading.data.temperature
      );
      return {xAxis, data};
    }
    const state = useAppState();
    const status = StatusData[state.currentReading.alarmState.aqDescription];
    const chartData = generateChartData(state.historicReadings);
    return (
        <div className='cards-container'>
            <Card>
                <CardContent>
                    <h3>Temperature</h3>
                    <Gauge
                        width={300}
                        height={250}
                        valueMin={-40}
                        valueMax={100}
                        value={state.currentReading.temperature}
                        startAngle={-90}
                        innerRadius="95%"
                        endAngle={90}
                        text={`${state.currentReading.temperature.toFixed(2)}°C`}
                        sx={{
                            [`& .${gaugeClasses.valueText}`]: {
                                fontSize: 40,
                                transform: 'translate(0px, 0px)',
                            },
                            [`& .${gaugeClasses.valueArc}`]: {
                                fill: status.color,
                            },
                        }}
                    />
                </CardContent>
            </Card>
            <Card sx={{ flex: 1}}>
                <CardContent>
                    <h3>Live data</h3>
                    <LineChart
                      skipAnimation
                      grid={{ vertical: true, horizontal: true }}
                      yAxis={[{
                        colorMap: {
                          type: 'piecewise',
                          thresholds: [-40, 0, 28, 40],
                          colors: [StatusData.superb.color, StatusData.good.color, StatusData.excellent.color, StatusData.mediocre.color]
                        }
                      }]}
                        series={[
                            {
                            data: chartData.data,
                            },
                        ]}
                        height={250}
                        />
                </CardContent>
            </Card>
        </div>
    )
  }