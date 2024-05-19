#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "constants.hpp"

FirebaseData fdbo;
FirebaseAuth auth;
FirebaseConfig config;

uint32_t sendDataPrevMillis = 0;
bool signUpOK = false;

struct PZEM004tData
{
    uint32_t m_id;
    std::string m_name;
    float m_voltage;
    float m_current;
    float m_power;
    float m_energy;
};

HardwareSerial mySerial(1);

void wifiSetup()
{
    WiFi.begin(WiFi_SSID, WiFi_PASSWORD);
    mySerial.printf("Connected to WiFi SSID = %s with PASSWORD = %s\n",
                    WiFi_SSID,
                    WiFi_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        mySerial.println(".");
        delay(WiFi_CONNECT_DELAY);
    }

    mySerial.println("Connected to the WiFi");
    mySerial.println(WiFi.localIP());
}

void setupFireBase()
{
    config.api_key = API_KEY;
    config.database_url = DATA_BASE_URL;

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        mySerial.println("signUp OK");
        signUpOK = true;
    }
    else
    {
        mySerial.printf("%s\n", config.signer.signupError.message.c_str());
    }

    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}

bool storeToDataBase()
{
    static bool beHere = false;
    PZEM004tData data;
    if (beHere)
    {
        beHere = false;
        data.m_id = 1,
        data.m_name = "Phone",
        data.m_voltage = 0.5f,
        data.m_current = 0.02f,
        data.m_power = 2.0f,
        data.m_energy = 2.0f;
    }
    else
    {
        beHere = true;
        data.m_id = 1,
        data.m_name = "Phone",
        data.m_voltage = 0.10f,
        data.m_current = 0.2f,
        data.m_power = 2.3f,
        data.m_energy = 2.4f;
    }

    if (!Firebase.RTDB.setInt(&fdbo, "Sensor/id", data.m_id))
    {
        mySerial.printf("ERROR: failed to save id = %d\n", data.m_id);
        return false;
    }

    if (!Firebase.RTDB.setString(&fdbo, "Sensor/name", data.m_name.c_str()))
    {
        mySerial.printf("ERROR: failed to save name = %s\n",
                        data.m_name.c_str());
        return false;
    }

    if (!Firebase.RTDB.setFloat(&fdbo, "Sensor/voltage", data.m_voltage))
    {
        mySerial.printf("ERROR: failed to save m_voltage = %f\n",
                        data.m_voltage);
        return false;
    }

    if (!Firebase.RTDB.setFloat(&fdbo, "Sensor/current", data.m_current))
    {
        mySerial.printf("ERROR: failed to save current = %f\n",
                        data.m_current);
        return false;
    }

    if (!Firebase.RTDB.setFloat(&fdbo, "Sensor/power", data.m_power))
    {
        mySerial.printf("ERROR: failed to save power = %f\n",
                        data.m_power);
        return false;
    }

    if (!Firebase.RTDB.setFloat(&fdbo, "Sensor/energy", data.m_energy))
    {
        mySerial.printf("ERROR: failed to save energy = %f\n",
                        data.m_energy);
        return false;
    }

    mySerial.println("All data is saved successfully");
    return true;
}

void setup()
{
    // Start serial communication with Arduino Uno
    mySerial.begin(ESP32_ARDUINO_UNO_UART_BOUD, SERIAL_8N1, UART_RX, UART_TX);
    delay(ONE_SECOND / 2);

    wifiSetup();
    setupFireBase();
}

void loop()
{
    if (Firebase.ready() && signUpOK
        && (((millis() - sendDataPrevMillis) > ONE_MINUTE)
            || (sendDataPrevMillis == 0)))
    {
        sendDataPrevMillis = millis();
        if (!storeToDataBase())
        {
            mySerial.println("ERROR: " + fdbo.errorReason());
        }
    }
}
