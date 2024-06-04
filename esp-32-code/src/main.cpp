#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PZEM004Tv30.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <string>
#include "constants.hpp"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

uint32_t sendDataPrevMillis = 0;

String uid;

// Initialize NTP client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,
                     NTP_UA_SERVER_NAME,
                     NTP_SERVER_TIME_OFFSET,
                     NTP_UPDATE_INTERVAL);

PZEM004Tv30 pzem(Serial1, UART_RX, UART_TX);

struct PZEM004tData
{
    float m_voltage;
    float m_current;
    float m_power;
    float m_energy;
};

struct TransferData
{
    uint32_t m_id;
    std::string m_name;

    PZEM004tData m_pzemData;

    String m_date;
    String m_time;
};

TransferData transferData;

void wifiSetup()
{
    delay(5000);
    WiFi.begin(WiFi_SSID, WiFi_PASSWORD);

    Serial.println("=== Connected to WiFi ===");
    Serial.println("> SSID = ");
    Serial.println(WiFi_SSID);

    Serial.println("> PASSWORD = ");
    Serial.println(WiFi_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(WiFi_CONNECT_DELAY);
    }
    Serial.println();
    Serial.println("=== Connected to the WiFi ===");
    Serial.println(WiFi.localIP());

    Serial.println("=== START SYNC TIME WITH NTP ===");
    timeClient.begin();
    while (!timeClient.update())
    {
        Serial.print(".");
        timeClient.forceUpdate();
    }
    Serial.println();
    Serial.println("=== END SYNC TIME WITH NTP ===");
}

void setupFireBase()
{
    config.api_key = API_KEY;
    config.database_url = DATA_BASE_URL;
    config.token_status_callback = tokenStatusCallback;
    config.max_token_generation_retry = 5;

    auth.user.email = EMAIL;
    auth.user.password = EMAIL_PASSWORD;

    Firebase.reconnectNetwork(true);
    fbdo.setResponseSize(4096);

    Firebase.begin(&config, &auth);

    while ((auth.token.uid) == "")
    {
        Serial.print('.');
        delay(ONE_SECOND);
    }

    uid = auth.token.uid.c_str();
    Serial.print("User UID: ");
    Serial.println(uid);
}

void getFormattedDateTime(String& date, String& time)
{
    timeClient.update();
    unsigned long rawTime = timeClient.getEpochTime();
    time_t timeT = static_cast<time_t>(rawTime);
    struct tm* ti = localtime(&timeT);

    char dateBuffer[11]; // YYYY-MM-DD
    char timeBuffer[9];  // HH:MM:SS

    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", ti);
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", ti);

    date = String(dateBuffer);
    time = String(timeBuffer);
}

bool storeToDataBase()
{
    static bool beHere = false;
    getFormattedDateTime(transferData.m_date, transferData.m_time);

    String rawDataPath =
        "SensorData/" + transferData.m_date + "/" + transferData.m_time;
    String dailySummaryPath = "DailySummaries/" + transferData.m_date;
    String hourlySummaryPath = "HourlySummaries/" + transferData.m_date + "/"
                               + transferData.m_time.substring(0, 2);

    if (!Firebase.RTDB.setInt(&fbdo, rawDataPath + "/id", transferData.m_id))
    {
        Serial.println("ERROR: failed to save id = " + String(transferData.m_id)
                       + " - " + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setString(
            &fbdo, rawDataPath + "/name", transferData.m_name.c_str()))
    {
        Serial.println("ERROR: failed to save name = "
                       + String(transferData.m_name.c_str()) + " - "
                       + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(
            &fbdo, rawDataPath + "/voltage", transferData.m_pzemData.m_voltage))
    {
        Serial.println("ERROR: failed to save voltage = "
                       + String(transferData.m_pzemData.m_voltage) + " - "
                       + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(
            &fbdo, rawDataPath + "/current", transferData.m_pzemData.m_current))
    {
        Serial.println("ERROR: failed to save current = "
                       + String(transferData.m_pzemData.m_current) + " - "
                       + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(
            &fbdo, rawDataPath + "/power", transferData.m_pzemData.m_power))
    {
        Serial.println("ERROR: failed to save power = "
                       + String(transferData.m_pzemData.m_power) + " - "
                       + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(
            &fbdo, rawDataPath + "/energy", transferData.m_pzemData.m_energy))
    {
        Serial.println("ERROR: failed to save energy = "
                       + String(transferData.m_pzemData.m_energy) + " - "
                       + fbdo.errorReason());
        return false;
    }

    if (!Firebase.RTDB.setFloat(
            &fbdo, dailySummaryPath + "/total_energy", transferData.m_pzemData.m_energy))
    {
        Serial.println("ERROR: failed to update daily total energy - "
                       + fbdo.errorReason());
        return false;
    }

    if (!Firebase.RTDB.setString(&fbdo,
                                 dailySummaryPath + "/last_updated",
                                 transferData.m_date + " "
                                     + transferData.m_time))
    {
        Serial.println("ERROR: failed to update daily last updated - "
                       + fbdo.errorReason());
        return false;
    }

    if (!Firebase.RTDB.setFloat(&fbdo,
                                hourlySummaryPath + "/total_energy",
                                transferData.m_pzemData.m_energy))
    {
        Serial.println("ERROR: failed to update hourly total energy - "
                       + fbdo.errorReason());
        return false;
    }

    if (!Firebase.RTDB.setString(&fbdo,
                                 hourlySummaryPath + "/last_updated",
                                 transferData.m_date + " "
                                     + transferData.m_time))
    {
        Serial.println("ERROR: failed to update hourly last updated - "
                       + fbdo.errorReason());
        return false;
    }

    // Check and update max power for the day
    float currentMaxPower = 0;
    if (Firebase.RTDB.getFloat(&fbdo, dailySummaryPath + "/max_power"))
    {
        currentMaxPower = fbdo.floatData();
    }
    else
    {
        Serial.println("ERROR: failed to get daily max power - "
                       + fbdo.errorReason());
    }

    if (transferData.m_pzemData.m_power > currentMaxPower)
    {
        if (!Firebase.RTDB.setFloat(&fbdo,
                                    dailySummaryPath + "/max_power",
                                    transferData.m_pzemData.m_power))
        {
            Serial.println("ERROR: failed to update max power - "
                           + fbdo.errorReason());
            return false;
        }

        if (!Firebase.RTDB.setString(&fbdo,
                                     dailySummaryPath + "/max_power_time",
                                     transferData.m_time))
        {
            Serial.println("ERROR: failed to update max power time - "
                           + fbdo.errorReason());
            return false;
        }
    }

    Serial.println("All transferData is saved successfully");
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    transferData.m_name = "pump";

    wifiSetup();
    setupFireBase();
}

bool getPZEMData2()
{
    static int counter = 1;

    if (counter == 1)
    {
        transferData.m_pzemData.m_current = 0.10f;      // A
        transferData.m_pzemData.m_power = 4.50f;        // W
        transferData.m_pzemData.m_energy = 7.368f;      // kWh
        transferData.m_pzemData.m_voltage = 229.60f;    // V
        counter = 2;
    }
    else if (counter == 2)
    {
        transferData.m_pzemData.m_current = 0.09f;      // A
        transferData.m_pzemData.m_power = 4.50f;        // W
        transferData.m_pzemData.m_energy = 7.368f;      // kWh
        transferData.m_pzemData.m_voltage = 229.60f;    // V
        counter = 1;
    }

    return true;
}

bool getPZEMData()
{
    transferData.m_pzemData.m_current = pzem.current();
    transferData.m_pzemData.m_power = pzem.power();
    transferData.m_pzemData.m_energy = pzem.energy();
    transferData.m_pzemData.m_voltage = pzem.energy();

    if (isnan(transferData.m_pzemData.m_voltage))
    {
        Serial.println("Error reading voltage");
        return false;
    }

    if (isnan(transferData.m_pzemData.m_current))
    {
        Serial.println("Error reading current");
        return false;
    }

    if (isnan(transferData.m_pzemData.m_power))
    {
        Serial.println("Error reading power");
        return false;
    }

    if (isnan(transferData.m_pzemData.m_energy))
    {
        Serial.println("Error reading energy");
        return false;
    }

    return true;
}

void loop()
{
    if (Firebase.ready()
        && (((millis() - sendDataPrevMillis) > ONE_MINUTE)
            || (sendDataPrevMillis == 0)) && getPZEMData2())
    {
        sendDataPrevMillis = millis();
        if (!storeToDataBase())
        {
            Serial.println("ERROR: " + fbdo.errorReason());
        }
    }
}
