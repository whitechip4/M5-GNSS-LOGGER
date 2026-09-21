#include "storage.h"
#include "util.h"

#define UNUPLOADED_LIST_FILENAME "/unuploaded.txt"
#define MAX_FILENAME_LENGTH 128

StorageModule::StorageModule() {
}

// ============================================================================
// Private Helper Methods
// ============================================================================

bool StorageModule::_readLineFromFile(File& file, char* buffer, size_t bufferSize) {
  if (!file.available()) {
    return false;
  }

  int index = 0;
  while (file.available() && index < bufferSize - 1) {
    int c = file.read();
    if (c == '\n' || c == '\r') {
      if (c == '\r' && file.available() && file.peek() == '\n') {
        file.read();
      }
      break;
    }
    buffer[index++] = (char)c;
  }
  buffer[index] = '\0';
  return true;
}

// ============================================================================
// Public Methods
// ============================================================================

void StorageModule::initializeUploadQueue() {
  if (!isReady()) {
    return;
  }

  if (!SD.exists(UNUPLOADED_LIST_FILENAME)) {
    File file = SD.open(UNUPLOADED_LIST_FILENAME, FILE_WRITE);
    if (file) {
      debug_print("STORAGE", "Created upload queue file");
      file.close();
    }
  }
}

bool StorageModule::addFileToUploadList(const char* filename) {
  if (!isReady() || filename == nullptr || strlen(filename) == 0) {
    return false;
  }

  // 重複チェック
  File readFile = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);
  if (readFile) {
    char lineBuffer[MAX_FILENAME_LENGTH];
    while (_readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer))) {
      if (strcmp(lineBuffer, filename) == 0) {
        readFile.close();
        return true;  // 既に存在
      }
    }
    readFile.close();
  }

  // 追記モードでファイル名を追加
  File writeFile = SD.open(UNUPLOADED_LIST_FILENAME, FILE_APPEND);
  if (!writeFile) {
    return false;
  }

  writeFile.println(filename);
  writeFile.close();

  debug_print("STORAGE", "Added to upload queue: %s", filename);
  return true;
}

bool StorageModule::removeFileFromUploadList(const char* filename) {
  if (!isReady() || filename == nullptr) {
    return false;
  }

  // 先頭行を読み飛ばし（削除対象の確認）
  File readFile = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);
  if (!readFile) {
    return false;
  }

  char lineBuffer[MAX_FILENAME_LENGTH];
  bool firstLine = _readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer));

  readFile.close();

  if (!firstLine || strcmp(lineBuffer, filename) != 0) {
    return false;  // 先頭行が一致しない
  }

  // 残りの行を一時ファイルに書き出し
  File tempFile = SD.open("/unuploaded.tmp", FILE_WRITE);
  readFile = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);

  if (tempFile && readFile) {
    // 先頭行を読み飛ばす
    _readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer));

    // 残りを一時ファイルにコピー
    while (_readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer))) {
      if (strlen(lineBuffer) > 0) {
        tempFile.println(lineBuffer);
      }
    }
  }

  if (tempFile)
    tempFile.close();
  if (readFile)
    readFile.close();

  // 一時ファイルを本来のファイルに置換
  SD.remove(UNUPLOADED_LIST_FILENAME);
  SD.rename("/unuploaded.tmp", UNUPLOADED_LIST_FILENAME);

  debug_print("STORAGE", "Removed from queue: %s", filename);
  return true;
}

bool StorageModule::skipFileInUploadList(const char* filename) {
  if (!isReady() || filename == nullptr) {
    return false;
  }

  // 先頭行を読み飛ばし（削除対象の確認）
  File readFile = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);
  if (!readFile) {
    return false;
  }

  char lineBuffer[MAX_FILENAME_LENGTH];
  bool firstLine = _readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer));
  readFile.close();

  // 先頭行が一致しない場合はfalse（スキップ不可）
  if (!firstLine || strcmp(lineBuffer, filename) != 0) {
    return false;
  }

  // 先頭行をスキップして残りを一時ファイルに書き出し
  File tempFile = SD.open("/unuploaded.tmp", FILE_WRITE);
  readFile = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);

  // 先頭行を読み飛ばす
  _readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer));

  // 残りをコピー
  while (_readLineFromFile(readFile, lineBuffer, sizeof(lineBuffer))) {
    if (strlen(lineBuffer) > 0) {
      tempFile.println(lineBuffer);
    }
  }

  if (tempFile)
    tempFile.close();
  if (readFile)
    readFile.close();

  // 一時ファイルを本来のファイルに置き換え
  SD.remove(UNUPLOADED_LIST_FILENAME);
  SD.rename("/unuploaded.tmp", UNUPLOADED_LIST_FILENAME);

  debug_print("STORAGE", "Skipped in queue: %s", filename);
  return true;
}

bool StorageModule::getNextFileToUpload(char* filenameBuffer, size_t bufferSize) {
  if (!isReady()) {
    return false;
  }

  File file = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);
  if (!file) {
    return false;
  }

  bool hasLine = _readLineFromFile(file, filenameBuffer, bufferSize);
  file.close();

  if (hasLine && strlen(filenameBuffer) > 0) {
    // ファイルが実在するか確認
    if (!SD.exists(filenameBuffer)) {
      debug_print("STORAGE", "WARNING: File doesn't exist: %s", filenameBuffer);
      // 存在しないファイルをキューから削除して再帰呼び出し
      removeFileFromUploadList(filenameBuffer);
      return getNextFileToUpload(filenameBuffer, bufferSize);
    }
    return true;
  }

  return false;
}

bool StorageModule::isUploadQueueEmpty() {
  if (!isReady()) {
    return true;
  }

  File file = SD.open(UNUPLOADED_LIST_FILENAME, FILE_READ);
  if (!file) {
    return true;  // ファイルがなければ空とみなす
  }

  char lineBuffer[MAX_FILENAME_LENGTH];
  bool hasLine = _readLineFromFile(file, lineBuffer, sizeof(lineBuffer));
  file.close();

  return !hasLine || strlen(lineBuffer) == 0;
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

  char lineStr[160];
  sprintf(lineStr,
          "%s,%lf,%lf,%.1f,%.1f,%d,%.2f,%.1f,%.1f",
          datetime,
          data.lat,
          data.lng,
          data.alt,
          data.vel,
          data.siv,
          data.hdop,
          data.hacc,
          data.vacc);

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
  file.println("date,time,lat,lng,alt,spd,siv,hdop,hacc,vacc");
}
