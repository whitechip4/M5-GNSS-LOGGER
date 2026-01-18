#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief GNSSデータ構造体
 */
typedef struct
{
  uint8_t siv; // sattellites number in view
  int32_t latRaw;
  int32_t lngRaw;
  double lat; // doubleはM5Stack Core2では32bit...
  double lng;

  double alt; // [m]
  float vel;  // [km/h]

  bool dateValid;
  uint16_t year;
  uint16_t month;
  uint16_t day;

  bool timeValid;
  uint16_t hour;
  uint16_t minute;
  uint16_t second;
  uint16_t millisecond;

  uint8_t fixType; // 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
  float hdop;
  float pdop;

  bool isFixOk;

} GNSS_DATA;

/**
 * @brief GNSS thresholds
 */
#define GNSS_HDOP_THRESHOLD 6.0f
#define GNSS_MIN_SATELLITES 5
#define GNSS_RECOVERY_BUFFER_MS 5000
#define GNSS_POSITION_THRESHOLD 0.001f

/**
 * @brief battery voltage threshold for recording
 */
#define BATTERY_VOLTAGE_THRESHOLD 3.6f

/**
 * @brief Display modes
 */
typedef enum {
  DISPLAY_MODE_DETAIL = 0,
  DISPLAY_MODE_SIMPLE = 1
} DISPLAY_MODE;

#endif // CONFIG_H
