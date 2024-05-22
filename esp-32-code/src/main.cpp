#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "constants.hpp"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

uint32_t sendDataPrevMillis = 0;

String uid;

// Initialize NTP client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_UA_SERVER_NAME,
                NTP_SERVER_TIME_OFFSET, NTP_UPDATE_INTERVAL);

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

void wifiSetup()
{
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

    Serial.println("=== Connected to the WiFi ===");
    Serial.println(WiFi.localIP());

    timeClient.begin();
    while (!timeClient.update())
    {
        timeClient.forceUpdate();
    }
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

    while ((auth.token.uid) == "") {
        Serial.print('.');
        delay(1000);
    }

    uid = auth.token.uid.c_str();
    Serial.print("User UID: ");
    Serial.print(uid);
}

void getFormattedDateTime(String &date, String &time)
{
    timeClient.update();
    unsigned long rawTime = timeClient.getEpochTime();
    time_t timeT = static_cast<time_t>(rawTime);
    struct tm *ti = localtime(&timeT);

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
    TransferData data;
    getFormattedDateTime(data.m_date, data.m_time);

    if (beHere)
    {
        beHere = false;
        data.m_id = 1;
        data.m_name = "iPhone";
        data.m_pzemData.m_voltage = 0.5f;
        data.m_pzemData.m_current = 0.02f;
        data.m_pzemData.m_power = 2.0f;
        data.m_pzemData.m_energy = 2.0f;
    }
    else
    {
        beHere = true;
        data.m_id = 1;
        data.m_name = "Phone";
        data.m_pzemData.m_voltage = 0.10f;
        data.m_pzemData.m_current = 0.2f;
        data.m_pzemData.m_power = 2.3f;
        data.m_pzemData.m_energy = 2.4f;
    }

    String rawDataPath = "SensorData/" + data.m_date + "/" + data.m_time;
    String dailySummaryPath = "DailySummaries/" + data.m_date;
    String hourlySummaryPath = "HourlySummaries/" + data.m_date + "/" + data.m_time.substring(0, 2);

    // Store raw data
    if (!Firebase.RTDB.setInt(&fbdo, rawDataPath + "/id", data.m_id))
    {
        Serial.println("ERROR: failed to save id = " + String(data.m_id) + " - " + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setString(&fbdo, rawDataPath + "/name", data.m_name.c_str()))
    {
        Serial.println("ERROR: failed to save name = " + String(data.m_name.c_str()) + " - " + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(&fbdo, rawDataPath + "/voltage", data.m_pzemData.m_voltage))
    {
        Serial.println("ERROR: failed to save voltage = " + String(data.m_pzemData.m_voltage) + " - " + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(&fbdo, rawDataPath + "/current", data.m_pzemData.m_current))
    {
        Serial.println("ERROR: failed to save current = " + String(data.m_pzemData.m_current) + " - " + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(&fbdo, rawDataPath + "/power", data.m_pzemData.m_power))
    {
        Serial.println("ERROR: failed to save power = " + String(data.m_pzemData.m_power) + " - " + fbdo.errorReason());
        return false;
    }
    if (!Firebase.RTDB.setFloat(&fbdo, rawDataPath + "/energy", data.m_pzemData.m_energy))
    {
        Serial.println("ERROR: failed to save energy = " + String(data.m_pzemData.m_energy) + " - " + fbdo.errorReason());
        return false;
    }

    // Update daily summary
    float currentDailyTotalEnergy = 0;
    if (Firebase.RTDB.getFloat(&fbdo, dailySummaryPath + "/total_energy"))
    {
        currentDailyTotalEnergy = fbdo.floatData();
    }
    else
    {
        Serial.println("ERROR: failed to get daily total energy - " + fbdo.errorReason());
    }

    currentDailyTotalEnergy += data.m_pzemData.m_energy;
    if (!Firebase.RTDB.setFloat(&fbdo, dailySummaryPath + "/total_energy", currentDailyTotalEnergy))
    {
        Serial.println("ERROR: failed to update daily total energy - " + fbdo.errorReason());
        return false;
    }

    if (!Firebase.RTDB.setString(&fbdo, dailySummaryPath + "/last_updated", data.m_date + " " + data.m_time))
    {
        Serial.println("ERROR: failed to update daily last updated - " + fbdo.errorReason());
        return false;
    }

    // Update hourly summary
    float currentHourlyTotalEnergy = 0;
    if (Firebase.RTDB.getFloat(&fbdo, hourlySummaryPath + "/total_energy"))
    {
        currentHourlyTotalEnergy = fbdo.floatData();
    }
    else
    {
        Serial.println("ERROR: failed to get hourly total energy - " + fbdo.errorReason());
    }

    currentHourlyTotalEnergy += data.m_pzemData.m_energy;
    if (!Firebase.RTDB.setFloat(&fbdo, hourlySummaryPath + "/total_energy", currentHourlyTotalEnergy))
    {
        Serial.println("ERROR: failed to update hourly total energy - " + fbdo.errorReason());
        return false;
    }

    if (!Firebase.RTDB.setString(&fbdo, hourlySummaryPath + "/last_updated", data.m_date + " " + data.m_time))
    {
        Serial.println("ERROR: failed to update hourly last updated - " + fbdo.errorReason());
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
        Serial.println("ERROR: failed to get daily max power - " + fbdo.errorReason());
    }

    if (data.m_pzemData.m_power > currentMaxPower)
    {
        if (!Firebase.RTDB.setFloat(&fbdo, dailySummaryPath + "/max_power", data.m_pzemData.m_power))
        {
            Serial.println("ERROR: failed to update max power - " + fbdo.errorReason());
            return false;
        }

        if (!Firebase.RTDB.setString(&fbdo, dailySummaryPath + "/max_power_time", data.m_time))
        {
            Serial.println("ERROR: failed to update max power time - " + fbdo.errorReason());
            return false;
        }
    }

    Serial.println("All data is saved successfully");
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    wifiSetup();
    setupFireBase();
}

void loop()
{
    if (Firebase.ready() && (((millis() - sendDataPrevMillis) > ONE_MINUTE)
            || (sendDataPrevMillis == 0)))
    {
        sendDataPrevMillis = millis();
        if (!storeToDataBase())
        {
            Serial.println("ERROR: " + fbdo.errorReason());
        }
    }
}
