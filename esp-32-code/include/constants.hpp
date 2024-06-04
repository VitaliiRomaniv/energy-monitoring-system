#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <stdint.h>

// PINOUT setup
constexpr int8_t UART_RX = 20;
constexpr int8_t UART_TX = 21;
constexpr uint32_t ESP32_ARDUINO_UNO_UART_BOUD = 9600;

// DELAYS setup
constexpr uint32_t ONE_SECOND = 1000;
constexpr uint32_t WiFi_CONNECT_DELAY = 300;
constexpr uint32_t ONE_MINUTE = ONE_SECOND * 60;

// CONFIG setup
constexpr const char CONFIG_NAME[] = "default.cfg";

// Wi-Fi setup
constexpr const char WiFi_SSID[] = "RomanivNetwork";
constexpr const char WiFi_PASSWORD[] = "";

// NTP setup

constexpr const char NTP_UA_SERVER_NAME[] = "pool.ntp.org";
constexpr long NTP_SERVER_TIME_OFFSET = 10800; // 3 Hours
constexpr unsigned long NTP_UPDATE_INTERVAL = 60000; // 60 sec

// // FireBase setup
constexpr const char API_KEY[] = "";
constexpr const char DATA_BASE_URL[] = "";
constexpr const char EMAIL[] = "gremxzwhynot@gmail.com";
constexpr const char EMAIL_PASSWORD[] = "examplePassword123";

#endif // CONSTANTS_H_