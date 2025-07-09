// Tambahkan dalam WebConfig.h
#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "FS.h"
#include "FFat.h"
#include <ESPmDNS.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

#define RESET_BUTTON_PIN 0
#define MAX_NETWORKS 5  // Maksimal jumlah jaringan yang bisa disimpan

// Struktur untuk menyimpan pesan
struct Message {
  String fromSerialNumber;
  String content;
  String timestamp;
  bool isRead;
};

struct MogiConfig {
  String name;
  String serialNumber;
  int eyeWidth;
  int eyeHeight;
  int borderRadius;
  int spaceBetween;
  bool autoBlinker;
  int autoBlinkerTime;
  int autoBlinkerVariation;
  bool breathing;
  float breathingSpeed;
  float breathingAmount;
  uint16_t textColor;
  uint16_t eyeColor;
};

class WebConfig {
  private:
    // Array untuk menyimpan beberapa jaringan WiFi
    String ssids[MAX_NETWORKS];
    String passwords[MAX_NETWORKS];
    int networkCount = 0;
    
    String data1;
    String data2;
    WebServer server;
    const char* _hostname;
    bool statusWifi;

    MogiConfig mogiConfig;
    
    // Array untuk menyimpan pesan
    const int MAX_MESSAGES = 50;
    Message messages[50];
    int messageCount = 0;

    void saveMogiConfig();
    void loadMogiConfig();
    void saveFriendsToFile();
    void readFriendsFromFile();
    void saveMessagesToFile();
    void readMessagesFromFile();

  public:
    WebConfig(const char* hostname);
    void handleFileUpload();
    void handleFileManager();
    void handleDeleteFile();
    void handleDebugFiles();
    void begin();
    void handleClient();
    void getFileList();
    bool cekStatusWifi();
    String getServerUrl();
    String getServerUrl2();
    bool addNetwork(String ssid, String password);
    bool removeNetwork(int index);
    void listNetworks();

    void handleMogiConfig();
    void handleSaveMogiConfig();
    MogiConfig getMogiConfig();

    // Friend and message handlers
    void handleFriendSettings();
    void handleAddFriend();
    void handleRemoveFriend();
    void handleMarkAsRead();
    void handleDeleteMessage();
    
    // Fungsi untuk mengirim pesan
    bool sendMessage(String toSerialNumber, String content);
    void handleSendMessage();
    void handleGetMessages();
    void handleGetFriends();
    void checkNewMessagesFromServer();

    // Getter untuk pesan
    int getMessageCount() const { return messageCount; }
    const Message& getMessage(int index) const { return messages[index]; }

  private:
    void saveDataToFile();
    void readDataFromFile();
    void handleRoot();
    void handleSave();
    void handleNetworks();
    void handleAddNetwork();
    void handleRemoveNetwork();
    void handleReset();
    bool connectWiFi();  // Return type changed to bool
    void setupAP();
};

#endif // WEBCONFIG_H
