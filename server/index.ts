const express = require('express');
const mqtt = require('mqtt');
const fs = require('fs');
const https = require('https');   // <-- added
const { createServer } = require('http');
const WebSocket = require('ws');



const app = express();
const port = 3000;

const server = createServer(app);
const wss = new WebSocket.Server({ server });  // ← same server, no extra port

const broadcast = (data) => wss.clients.forEach(client => {
  if (client.readyState === WebSocket.OPEN) client.send(JSON.stringify(data));
});
  
// MQTT
// const mqttClient = mqtt.connect('mqtt://broker.hivemq.com:1883');
const mqttClient = mqtt.connect('mqtts://ad245d63a3cb48ffbfc8a82d5def699e.s1.eu.hivemq.cloud:8883', {
  username: 'esp32-sensor',     
  password: 'Kartik2004',
  protocol: 'mqtts',
  rejectUnauthorized: false,
  servername: 'ad245d63a3cb48ffbfc8a82d5def699e.s1.eu.hivemq.cloud'
});
const topic = 'sensors/esp32/dht';

// ThingSpeak (create a free channel with 3 fields: Temp, Humidity, Gas)
const thingspeakApiKey = process.env.THINKSPEAK_WRITE_APIKEY;   // ← put your key here
let lastThingSpeakUpdate = 0;

// Log storage
let logs = [];
const logFile = 'logs.txt';

mqttClient.on('error', (err) => {
  console.error('❌ MQTT ERROR:', err);
});

mqttClient.on('close', () => {
  console.log('⚠️ MQTT connection closed');
});

mqttClient.on('offline', () => {
  console.log('⚠️ MQTT went offline');
});

mqttClient.on('reconnect', () => {
  console.log('🔄 MQTT reconnecting...');
});

mqttClient.on('connect', () => {
  console.log('Connected to MQTT broker');
  
  mqttClient.subscribe(topic, (err) => {
    if (!err) console.log(`Subscribed to ${topic}`);
  });

  // ← ADD THIS LINE
  mqttClient.subscribe('alerts/fire', (err) => {
    if (!err) console.log('Subscribed to alerts/fire for emergency notifications');
  });

    mqttClient.subscribe('alerts/theft', (err) => {
    if (!err) console.log('Subscribed to alerts/theft for theft notifications');
  });
  mqttClient.subscribe('alerts/emergency', (err) => {
    if (!err) console.log('Subscribed to alerts/emergency for emergency button');
  });
});

mqttClient.on('message', (topic, message) => {
  if (topic === 'sensors/esp32/dht') {
    try {
      const data = JSON.parse(message.toString());
      const timestamp = new Date().toISOString();
      const logEntry = `${timestamp} - Temp: ${data.temp}°C, Humidity: ${data.humidity}%, CO2: ${data.co2_ppm} ppm, HR: ${data.bpm} BPM, SpO2: ${data.spo2}%`;

      console.log(logEntry);
      fs.appendFileSync(logFile, logEntry + '\n');
      logs.push(logEntry);
      if (logs.length > 20) logs.shift();       

      // === THINGSPEAK UPDATE (throttled to ~20s) ===
      if (Date.now() - lastThingSpeakUpdate > 20000) {
        const url = `https://api.thingspeak.com/update?api_key=${thingspeakApiKey}` +
                    `&field1=${data.temp}` +
                    `&field2=${data.humidity}` +
                    `&field3=${data.co2_ppm}`;

        https.get(url, (res) => {
          console.log('ThingSpeak update status:', res.statusCode);
        }).on('error', (err) => console.error('ThingSpeak error:', err));

        lastThingSpeakUpdate = Date.now();
      }
    } catch (err) {
      console.error('Invalid JSON:', message.toString());
    }

  } else if(topic === 'alerts/fire'){
    // ================== NEW: FIRE ALERT HANDLER ==================
    console.log('🚨 FIRE ALERT RECEIVED:', message.toString());

    // TODO: Send notification to your React app here
    // Examples:
    // 1. Firebase (recommended - already in your project description)
    //    admin.messaging().sendToTopic('fire_alerts', { notification: { title: "🔥 Fire Detected!", body: "Check kiosk immediately" }})
    //
    // 2. If your React app listens via WebSocket/SSE, emit here
    // 3. Or just log for now and poll /logs in React
    broadcast({ type: 'fire', message: message.toString() });

    // For now (minimal):
    console.log("📲 Notification would be sent to React app / Firebase right now");
  } else if (topic === 'alerts/theft') {
    console.log('🚨 THEFT ALERT RECEIVED:', message.toString());
    console.log("📲 Notification would be sent to React app / Firebase right now (THEFT)");
    // TODO: send push notification exactly like fire

    broadcast({ type: 'theft', message: message.toString() });

  } else if (topic === 'alerts/emergency') {
    console.log('🚨 EMERGENCY ALERT RECEIVED:', message.toString());
    console.log("📲 Notification would be sent to React app / Firebase right now (EMERGENCY)");
    // TODO: send push notification exactly like fire

    broadcast({ type: 'emergency', message: message.toString() });
  }
});

app.get('/logs', (req, res) => res.json({ logs }));

// app.listen(port, () => console.log(`Server running at http://localhost:${port}`));
server.listen(port, () => console.log(`Server running at http://localhost:${port}`));