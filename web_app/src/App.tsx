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
import { RPCService } from './common/rpcService'
import { Line } from 'react-chartjs-2';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from 'chart.js';
import './App.css'

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
);

const options = {
  animation: false,
  plugins: {
    legend: {
      position: 'top' as const,
    },
    title: {
      display: true,
      text: 'Onboard Temp',
    },
  },
};

type Reading = {
  time: number;
  value: number;
}


function formatData(data: Reading[]){
  return {
    labels: data.map((_d,i)=>i),
    datasets: [{
      label: "Temp",
      borderColor: 'rgb(53, 162, 235)',
      data: data.map(d=>d.value)
    }
    ]
  }
}

function App() {
  const [service,] = useState<RPCService>(new RPCService());
  const [connecting, setConnecting] = useState(false);
  const [tempIsCharting, setTempIsCharting] = useState(false);
  // const [tempInterval, setTempInterval] = useState(0);
  const [data, setData] = useState<Reading[]>([]);
  useEffect(()=>{
    if (tempIsCharting){
      const t = setInterval(async ()=>{
        const temp = (await service.getTemp()).getTemp()
        setData((curData)=>{
          // only keep last 40 readings
          const arr = [...curData].slice(-40);
          arr.push({time: Date.now(), value: temp})
          return arr;
        })
      }, 500)

      return ()=>{
        clearInterval(t)
      }
    }
  }, [tempIsCharting, service])
  return (
    <>
      <h1>Airmaranth</h1>
      <div className="card">
        <button onClick={async () => {
            setConnecting(true);
            try{
              await service.connect();
            }
            catch(e){}
            setConnecting(false);
          }} disabled={connecting}>
          {connecting ? "Connecting" : "Connect"}
        </button>
        <button onClick={() => service.blink(5)}>
          Blink 5 times
        </button>

        <button onClick={async () => {
          setTempIsCharting(!tempIsCharting)
        }}>
          Chart Temperature
        </button>
        <br/>
        {data.length>2 && <Line width={600} height={400} data={formatData(data)} options={options}/>}
      </div>
    </>
  )
}

export default App
