#ifndef FileHandler_h
#define FileHandler_h

#include <FFat.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "WebConfig.h"

class FileHandler {
  public:
    FileHandler();
    void upload(String namafile, String url);
    void download(String namafile, String url);
    bool ServerStatus(String serverurl_);
    void parseResponse(String response);
    void setMogiConfig(const MogiConfig& config);
    String getserver_user();
    String getesp_user();
    bool isUpload();
    bool isDownload();
    bool isInQuiz();
    String getQuizType();
    
    int responweb;

  private:
    bool uploadStatus;
    bool downloadStatus;
    bool isServerOn;
    bool in_quiz;
    String server_user;
    String esp_user;
    String quiz_type;
    MogiConfig mogiConfig;
};

#endif
