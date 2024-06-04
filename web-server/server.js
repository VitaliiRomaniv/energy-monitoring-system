const express = require('express');
const firebaseAdmin = require('firebase-admin');
const path = require('path');

const app = express();

// Initialize Firebase Admin SDK
const serviceAccount = require('./energymonitoringsystemproject-firebase-adminsdk-hptqt-fdc7329fd0.json');
firebaseAdmin.initializeApp({
  credential: firebaseAdmin.credential.cert(serviceAccount),
  databaseURL: 'https://energymonitoringsystemproject-default-rtdb.europe-west1.firebasedatabase.app/'
});

// Middleware to serve static files from the 'public' directory
app.use(express.static(path.join(__dirname, 'public')));

// API endpoint to fetch last sensor data
app.get('/api/sensorData', async (req, res) => {
  try {
    const snapshot = await firebaseAdmin.database().ref('SensorData').orderByKey().limitToLast(1).once('value');
    const lastData = snapshot.val();
    res.json(lastData);
  } catch (error) {
    console.error('Error fetching data:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// API endpoint to fetch Firebase config
app.get('/firebase-config', (req, res) => {
  res.json({
    apiKey: "AIzaSyA5KJTztw4ZpqWyWZqgZh2cH8bnmPffeLU",
    authDomain: "energymonitoringsystemproject.firebaseapp.com",
    databaseURL: "https://energymonitoringsystemproject-default-rtdb.europe-west1.firebasedatabase.app",
    projectId: "energymonitoringsystemproject",
    storageBucket: "energymonitoringsystemproject.appspot.com",
    messagingSenderId: "300683444406",
    appId: "1:300683444406:web:e32c11eeeb944ec0c3e617"
  });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});