#include "FileHandler.h"

FileHandler::FileHandler() {
    // Constructor, can initialize variables if needed
    responweb = 6;
    uploadStatus = false;
    downloadStatus = false;
    isServerOn = false;
    in_quiz = false;
    quiz_type = "";
}

void FileHandler::setMogiConfig(const MogiConfig& config) {
  mogiConfig = config;
}


void FileHandler::upload(String namafile, String url) {
  uploadStatus = true;
  File file = FFat.open("/"+namafile, FILE_READ);
  if (!file) {
    Serial.println("FILE IS NOT AVAILABLE!");
    return;
  }

  Serial.println("upload "+namafile+" to "+url);

  HTTPClient client;
  client.setTimeout(6000);
  client.begin(url+"/uploadAudio");
  client.addHeader("Content-Type", "audio/wav");
  client.addHeader("Serial-Number", mogiConfig.serialNumber);
  client.addHeader("Device-Name", mogiConfig.name);

  int httpResponseCode = client.sendRequest("POST", &file, file.size());
  delay(600);
  Serial.print("httpResponseCode: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode == 200) {
    String response = client.getString();
    Serial.println("==================== esp_user ====================");
    Serial.println(response);
    Serial.println("====================      End      ====================");
    parseResponse(response);
    file.close();
    client.end();
    uploadStatus = false;
  } else {
    Serial.println("Error in HTTP request");
    file.close();
    client.end();
    uploadStatus = true;
  }
  responweb = httpResponseCode;
}

bool FileHandler::isUpload(){
  return uploadStatus;
}
bool FileHandler::isDownload(){
  return downloadStatus;
}
void FileHandler::download(String namafile, String url) {
  downloadStatus = true;
  if (FFat.exists("/"+namafile)) {
    FFat.remove("/"+namafile);
    delay(100);
  }
  HTTPClient client;
  client.setTimeout(6000);

  client.begin(url+"/downloadAudio/"+namafile); 
  client.addHeader("Serial-Number", mogiConfig.serialNumber);
  
  int httpCode = client.GET();

  if (httpCode == 200) { 
    int file_size = client.getSize();  
    Serial.printf("Ukuran file yang diunduh: %d bytes\n", file_size);
    File file = FFat.open("/"+namafile, FILE_WRITE);
    if (!file) {
      Serial.println("Gagal membuka file untuk menulis");
      return;
    }
    WiFiClient* stream = client.getStreamPtr();
    uint8_t buff[512] = { 0 }; // 16 normal sarannya 512
    int bytesRead = 0;
    while (file_size) {
      bytesRead = stream->read(buff, sizeof(buff));
      if (bytesRead > 0) {
        file.write(buff, bytesRead);
        file_size++;
      }
      file_size--;
      Serial.println(file_size);
    }
    file.close();
    Serial.println("download sukses "+namafile);
  }
  else if(httpCode==404){
    Serial.println(client.getString());
  }
  else{
    Serial.println("Bermasalah ^^ cek server");
  }
  client.end();
  downloadStatus = false;
}

void FileHandler::parseResponse(String response) {
  // Menggunakan ArduinoJson untuk memparse respons JSON
  DynamicJsonDocument doc(1024);  // Buffer untuk memparse JSON (1024 bytes)
  
  // Parsing JSON dari string response
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.f_str());
    return;
  }

  // Menyimpan nilai "server_user" dan "esp_user" ke dalam variabel
  server_user = doc["server_user"].as<String>();
  esp_user = doc["esp_user"].as<String>();
  in_quiz = doc["in_quiz"].as<bool>();
  
  // Get quiz type if available
  if (doc.containsKey("quiz_type")) {
    quiz_type = doc["quiz_type"].as<String>();
  } else {
    quiz_type = "";
  }

  // Menampilkan hasil di serial monitor
  Serial.print("server_user: ");
  Serial.println(server_user);
  Serial.print("esp_user: ");
  Serial.println(esp_user);
  Serial.print("in_quiz: ");
  Serial.println(in_quiz);
  Serial.print("quiz_type: ");
  Serial.println(quiz_type);
}

bool FileHandler::ServerStatus(String serverurl_){
    String ServerURL = serverurl_;
    ServerURL = ServerURL+"/checkStatus";
    Serial.println(ServerURL);
    HTTPClient client;
    client.begin(ServerURL);
    int httpResponseCode = client.GET();
    if(httpResponseCode == 200){
        isServerOn = true;
        Serial.println("");
        Serial.print("Server Status: ");
        Serial.println("on ");
    }
    else{
        isServerOn = false;
        Serial.println("");
        Serial.print("Server Status: ");
        Serial.println("off");
    }
    // Menutup koneksi HTTP
    client.end();
    return isServerOn;
}

String FileHandler::getserver_user(){
  return server_user;
}

String FileHandler::getesp_user(){
  return esp_user;
}

bool FileHandler::isInQuiz(){
  return in_quiz;
}

String FileHandler::getQuizType(){
  return quiz_type;
}
