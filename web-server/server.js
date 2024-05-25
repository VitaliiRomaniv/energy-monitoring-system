const express = require('express');
const firebaseAdmin = require('firebase-admin');
const path = require('path'); // Import the 'path' module

const app = express();

// Initialize Firebase Admin SDK
const serviceAccount = require('./energymonitoringsystemproject-firebase-adminsdk-hptqt-fdc7329fd0.json');
firebaseAdmin.initializeApp({
  credential: firebaseAdmin.credential.cert(serviceAccount),
  databaseURL: 'https://energymonitoringsystemproject-default-rtdb.europe-west1.firebasedatabase.app/'
});

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

// Middleware to serve static files from the 'public' directory
app.use(express.static(path.join(__dirname, 'public')));

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});
