const express = require('express');
const mqtt = require('mqtt');
const fs = require('fs');
const https = require('https');   // <-- added
const { createServer } = require('http');
const WebSocket = require('ws');

const path = require('path');

const imagesDir = path.join(__dirname, 'images');
if (!fs.existsSync(imagesDir)) {
  fs.mkdirSync(imagesDir);
}


const app = express();
const port = 3000;

const server = createServer(app);
const wss = new WebSocket.Server({ server }); 

const locations = [
  { lat: 30.515944, lng: 76.657806 }, // Main point
  { lat: 30.516012, lng: 76.657842 },
  { lat: 30.515881, lng: 76.657913 },
  { lat: 30.516105, lng: 76.657701 },
  { lat: 30.515792, lng: 76.657665 },
  { lat: 30.516188, lng: 76.657954 },
  { lat: 30.515963, lng: 76.658021 },
  { lat: 30.515846, lng: 76.657742 },
  { lat: 30.516074, lng: 76.657589 },
  { lat: 30.515721, lng: 76.657988 }
];

const getRandomLocation = () => {
  return locations[Math.floor(Math.random() * locations.length)];
};

const fakeVitals = Array.from({ length: 100 }, () => ({
  bpm: Math.floor(Math.random()*30)+70,   // 70–100 BPM
  spo2: Math.floor(Math.random()*3)+97    // 97–100 %
}));


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
const thingspeakApiKey = process.env.THINKSPEAK_WRITE_APIKEY;   
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

  mqttClient.subscribe('health/scan', () => {
    console.log('Subscribed to health scan');
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

    broadcast({ type: 'fire', message: message.toString(), location: getRandomLocation() });


    console.log("📲 Notification would be sent to React app / Firebase right now");
  } else if (topic === 'alerts/theft') {
    console.log('🚨 THEFT ALERT RECEIVED:', message.toString());
    console.log("📲 Notification would be sent to React app / Firebase right now (THEFT)");

    broadcast({ type: 'theft', message: message.toString(), location: getRandomLocation() });

  } else if (topic === 'alerts/emergency') {
    console.log('🚨 EMERGENCY ALERT RECEIVED:', message.toString());
    console.log("📲 Notification would be sent to React app / Firebase right now (EMERGENCY)");

    broadcast({ type: 'emergency', message: message.toString(), location: getRandomLocation() });
  } else if(topic === 'health/scan') {

  console.log("🫀 Fake health scan triggered");

  // simulate processing delay
  setTimeout(() => {

    const result =
      fakeVitals[Math.floor(Math.random()*fakeVitals.length)];
      console.log(result)

    broadcast({
      type: 'health',
      bpm: result.bpm,
      spo2: result.spo2,
      timestamp: new Date().toISOString()
    });

  }, 5000); // fake analysis delay
}
});

// ====================== ESP32-CAM PHOTO UPLOAD ======================
app.post('/upload-image', express.raw({ type: 'image/jpeg', limit: '2mb' }), (req, res) => {
  if (!req.body || req.body.length === 0) {
    return res.status(400).send('No image received');
  }

  const filename = `motion_${Date.now()}.jpg`;
  const filePath = path.join(imagesDir, filename);

  fs.writeFileSync(filePath, req.body);
  console.log(`✅ Motion photo saved: ${filename}`);

  // Publish MQTT alert 
  const alertPayload = {
    alert: "motion",
    status: "detected",
    location: getRandomLocation(),
    image: req.body,
    timestamp: new Date().toISOString()
  };

  // mqttClient.publish('alerts/motion', alertPayload);
  // console.log('📸 Motion alert published to alerts/motion');

  broadcast({
      type: 'motion',
      alert: 'motion',
      status: 'detected',
      location: getRandomLocation(),
      image: alertPayload.image,
      imageUrl: `https://smart-kiosk-7ybc.onrender.com/images/${alertPayload.image}`,   
      timestamp: alertPayload.timestamp
    });

  res.send('Photo received');
});

app.get('/logs', (req, res) => res.json({ logs }));

// app.listen(port, () => console.log(`Server running at http://localhost:${port}`));
server.listen(port, () => console.log(`Server running at http://localhost:${port}`));