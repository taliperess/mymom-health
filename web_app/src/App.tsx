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
import Tabs from '@mui/material/Tabs';
import Tab from '@mui/material/Tab';
import Box from '@mui/material/Box';
import useMediaQuery from '@mui/material/useMediaQuery';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import CssBaseline from '@mui/material/CssBaseline';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import { useAppState } from './common/state';
import { HumidityPage } from './components/humidity';
import { Hero } from './components/hero';
import { Header } from './components/header';
import { AirQualityPage } from './components/airquality';
import { TemperaturePage } from './components/temperature';
interface TabPanelProps {
    children?: React.ReactNode;
    index: number;
    value: number;
}


function TabPanel(props: TabPanelProps) {
    const { children, value, index, ...other } = props;

    return (
        <div
            role="tabpanel"
            hidden={value !== index}
            id={`tabpanel-${index}`}
            aria-labelledby={`tab-${index}`}
            {...other}
        >
            {value === index && <Box sx={{ p: 3 }}>{children}</Box>}
        </div>
    );
}

export default function App() {
    const prefersDarkMode = useMediaQuery('(prefers-color-scheme: dark)');
    const theme = React.useMemo(
        () =>
            createTheme({
                palette: {
                    mode: prefersDarkMode ? 'dark' : 'light',
                },
            }),
        [prefersDarkMode],
    );

    return (
        <ThemeProvider theme={theme}>
            <CssBaseline />
            <Home />
        </ThemeProvider>
    );
}

function Home() {
    const [tab, setTab] = useState(0);
    const connected = useAppState(state => state.connected);
    const basicMode = useAppState(state => state.basicMode);

    const handleChange = (_event: React.SyntheticEvent, newValue: number) => {
        setTab(newValue);
    };

    useEffect(()=>{
      // Can only see temperature without enviro board.
      if (basicMode) setTab(1); 
    }, [basicMode]);

    if (!connected){
      return (
        <Box sx={{ width: '100%', display: 'flex', gap: '1rem', flexDirection: 'column' }}>
          <Header />
          <Card sx={{marginTop: "10rem"}}>
            <CardContent>
                <center style={{padding: "3rem 0rem"}}>To begin using the Pigweed Sense app, click <b>Connect</b> and choose your device.</center>
            </CardContent>
          </Card>
        </Box>
      )
    }

    return (
        <Box sx={{ width: '100%', display: 'flex', gap: '1rem', flexDirection: 'column' }}>
            <Header />
            <Hero />
            <Box sx={{ borderBottom: 1, borderColor: 'divider' }}>
                <Tabs value={tab} onChange={handleChange}>
                    <Tab label="Air Quality" disabled={basicMode} />
                    <Tab label="Temperature" />
                    <Tab label="Humidity" disabled={basicMode} />
                </Tabs>
            </Box>
            <TabPanel value={tab} index={0}>
                <AirQualityPage />
            </TabPanel>
            <TabPanel value={tab} index={1}>
                <TemperaturePage />
            </TabPanel>
            <TabPanel value={tab} index={2}>
                <HumidityPage/>
            </TabPanel>
        </Box>
    );
}
