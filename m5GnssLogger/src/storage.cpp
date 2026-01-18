#include "storage.h"

StorageModule::StorageModule() {
}

bool StorageModule::begin() {
  return SD.begin();
}

bool StorageModule::isReady() {
  sdcard_type_t type = SD.cardType();
  if (type == CARD_UNKNOWN || type == CARD_NONE) {
    return false;
  }
  return true;
}

bool StorageModule::writeHeader(const char* fileName) {
  if (!isReady()) {
    return false;
  }

  _file = SD.open(fileName, FILE_APPEND);
  if (!_file) {
    return false;
  }

  _writeHeader(_file);
  _file.close();

  return true;
}

bool StorageModule::writeData(const GNSS_DATA& data, const char* fileName) {
  if (!isReady()) {
    return false;
  }

  _file = SD.open(fileName, FILE_APPEND);
  if (!_file) {
    return false;
  }

  char datetime[32];
  sprintf(datetime,
          "%04d/%02d/%02d,%02d:%02d:%02d",
          data.year,
          data.month,
          data.day,
          data.hour,
          data.minute,
          data.second);

  char lineStr[128];
  sprintf(lineStr,
          "%s,%lf,%lf,%.1f,%.1f,%d,%.2f",
          datetime,
          data.lat,
          data.lng,
          data.alt,
          data.vel,
          data.siv,
          data.hdop);

  _file.println(lineStr);
  _file.close();

  return true;
}

bool StorageModule::writeRawData(const GNSS_DATA& data, const char* fileName) {
  return writeData(data, fileName);
}

void StorageModule::generateFileName(const char* baseName,
                                     char* buffer,
                                     size_t bufferSize,
                                     const GNSS_DATA& data,
                                     bool isRaw) {
  if (isRaw) {
    snprintf(buffer,
             bufferSize,
             "/%s_%04d%02d%02d_%02d%02d%02d_raw.csv",
             baseName,
             data.year,
             data.month,
             data.day,
             data.hour,
             data.minute,
             data.second);
  } else {
    snprintf(buffer,
             bufferSize,
             "/%s_%04d%02d%02d_%02d%02d%02d.csv",
             baseName,
             data.year,
             data.month,
             data.day,
             data.hour,
             data.minute,
             data.second);
  }
}

void StorageModule::_writeHeader(File& file) {
  file.println("date,time,lat,lng,alt,spd,siv,hdop");
}
