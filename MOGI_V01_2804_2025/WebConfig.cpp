// Implementasi WebConfig.cpp dengan multinetwork
#include "WebConfig.h"

// Struktur untuk menyimpan data teman
struct Friend {
  String name;
  String serialNumber;
  bool isActive;
};

// Array untuk menyimpan teman
const int MAX_FRIENDS = 10;
Friend friends[MAX_FRIENDS];
int friendCount = 0;

WebConfig::WebConfig(const char* hostname) : server(80), _hostname(hostname) {}

void WebConfig::begin() {
  // Inisialisasi FFat
  if (!FFat.begin()) {
    Serial.println("Failed to mount FFat filesystem. Trying to format...");
    if (FFat.begin(true)) {
      Serial.println("Flash diformat dan siap digunakan!");
    } else {
      Serial.println("Gagal memformat flash.");
    }
  } else {
    Serial.println("FFat filesystem mounted successfully.");
  }

  // Baca data dari file
  readDataFromFile();
  loadMogiConfig();

  // Coba koneksi WiFi
  if (networkCount > 0) {
    if (!connectWiFi()) {
      setupAP();
    }
  } else {
    setupAP();
  }

  // Setup mDNS
  if (!MDNS.begin(_hostname)) {
    Serial.println("Error starting mDNS");
    return;
  }
  Serial.println("mDNS responder started");
  Serial.print("Access it at http://");
  Serial.print(_hostname);
  Serial.println(".local");

  // Setup server web
  server.on("/", std::bind(&WebConfig::handleRoot, this));
  server.on("/save", std::bind(&WebConfig::handleSave, this));
  server.on("/networks", std::bind(&WebConfig::handleNetworks, this));
  server.on("/addnetwork", std::bind(&WebConfig::handleAddNetwork, this));
  server.on("/removenetwork", std::bind(&WebConfig::handleRemoveNetwork, this));
  server.on("/filemanager", HTTP_GET, std::bind(&WebConfig::handleFileManager, this));
  server.on("/upload", HTTP_POST, 
  [this](){ server.send(200); }, 
  std::bind(&WebConfig::handleFileUpload, this));
  server.on("/debugfiles", HTTP_GET, std::bind(&WebConfig::handleDebugFiles, this));
  server.on("/deletefile", HTTP_POST, std::bind(&WebConfig::handleDeleteFile, this));
  server.on("/reset", std::bind(&WebConfig::handleReset, this));
  server.on("/mogiconfig", std::bind(&WebConfig::handleMogiConfig, this));
  server.on("/savemogiconfig", std::bind(&WebConfig::handleSaveMogiConfig, this));
  server.on("/friendsettings", std::bind(&WebConfig::handleFriendSettings, this));
  server.on("/addfriend", std::bind(&WebConfig::handleAddFriend, this));
  server.on("/removefriend", std::bind(&WebConfig::handleRemoveFriend, this));
  server.on("/markasread", std::bind(&WebConfig::handleMarkAsRead, this));
  server.on("/deletemessage", std::bind(&WebConfig::handleDeleteMessage, this));
  server.on("/sendmessage", HTTP_POST, std::bind(&WebConfig::handleSendMessage, this));
  server.on("/getmessages", HTTP_GET, std::bind(&WebConfig::handleGetMessages, this));
  server.on("/getfriends", HTTP_GET, std::bind(&WebConfig::handleGetFriends, this));
  server.begin();

  // Setup tombol reset
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Baca data teman dan pesan dari file
  readFriendsFromFile();
  readMessagesFromFile();
}

void WebConfig::handleDebugFiles() {
  Serial.println("\n--- File Debugging Report ---");
  
  fs::File root = FFat.open("/");
  if (root && root.isDirectory()) {
    fs::File file = root.openNextFile();
    while (file) {
      String fileName = String(file.name());
      Serial.print("File: '");
      Serial.print(fileName);
      Serial.print("', Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
      
      // Print character by character to spot any hidden characters
      Serial.print("Name chars: ");
      for (int i = 0; i < fileName.length(); i++) {
        Serial.print((int)fileName.charAt(i));
        Serial.print(" ");
      }
      Serial.println();
      
      file = root.openNextFile();
    }
  }
  
  Serial.println("--- End of Report ---\n");
  
  server.sendHeader("Location", "/filemanager");
  server.send(303);
}

void WebConfig::handleDeleteFile() {
  if (server.hasArg("filename")) {
    String filename = server.arg("filename");
    
    // Pastikan nama file dimulai dengan slash
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    
    Serial.println("Attempting to delete file: " + filename);
    
    if (FFat.exists(filename)) {
      if (FFat.remove(filename)) {
        Serial.println("File deleted successfully: " + filename);
        server.sendHeader("Location", "/filemanager");
        server.send(303);
      } else {
        Serial.println("Delete failed for: " + filename);
        server.send(500, "text/plain", "Delete failed");
      }
    } else {
      // Jika file tidak ditemukan, coba dengan variasi nama
      String altFilename = filename.startsWith("/") ? filename.substring(1) : "/" + filename;
      Serial.println("File not found, trying alternative name: " + altFilename);
      
      if (FFat.exists(altFilename)) {
        if (FFat.remove(altFilename)) {
          Serial.println("File deleted successfully with alternative name: " + altFilename);
          server.sendHeader("Location", "/filemanager");
          server.send(303);
        } else {
          Serial.println("Delete failed for alternative name: " + altFilename);
          server.send(500, "text/plain", "Delete failed");
        }
      } else {
        Serial.println("File not found with either name: " + filename + " or " + altFilename);
        server.send(404, "text/plain", "File not found");
      }
    }
  } else {
    server.send(400, "text/plain", "Bad request");
  }
}

void WebConfig::handleFileManager() {
  String html = "<!DOCTYPE html><html lang='en'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>File Manager</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; text-align: center; margin: 0; padding: 20px; }";
  html += "div { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); max-width: 500px; margin: 20px auto; }";
  html += "input[type='file'] { width: 100%; padding: 10px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type='submit'], button { background-color: #4CAF50; color: white; border: none; padding: 12px 20px; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 10px 5px; }";
  html += "input[type='submit']:hover, button:hover { background-color: #45a049; }";
  html += "button.back { background-color: #2196F3; }";
  html += "button.back:hover { background-color: #0b7dda; }";
  html += "button.delete { background-color: #f44336; }";
  html += "button.delete:hover { background-color: #e53935; }";
  html += "table { width: 100%; border-collapse: collapse; margin: 15px 0; }";
  html += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "th { background-color: #f2f2f2; }";
  html += "a { text-decoration: none; color: inherit; }";
  html += "progress { width: 100%; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>File Manager</h1>";
  
  // Upload form
  html += "<div>";
  html += "<h2>Upload MP3 File</h2>";
  html += "<form method='POST' action='/upload' enctype='multipart/form-data' id='upload_form'>";
  html += "<input type='file' name='file' id='file' accept='.mp3'><br>";
  html += "<input type='submit' value='Upload File'>";
  html += "</form>";
  html += "<progress id='progressBar' value='0' max='100' style='display:none;'></progress>";
  html += "<h3 id='status'></h3>";
  html += "<p id='loaded_n_total'></p>";
  html += "</div>";
  
  // File list
  html += "<div>";
  html += "<h2>Saved Files</h2>";
  html += "<table>";
  html += "<tr><th>File Name</th><th>Size</th><th>Action</th></tr>";
  html += "<p><small>Klik tombol di bawah untuk melihat detail nama file di Serial Monitor</small></p>";
  html += "<form action='/debugfiles' method='GET'>";
  html += "<input type='submit' value='Debug File Names'>";
  html += "</form>";

  
  // List files from FFat
fs::File root = FFat.open("/");
if (root && root.isDirectory()) {
  fs::File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory() && String(file.name()).endsWith(".mp3")) {
      // Simpan nama file yang sebenarnya termasuk slash di depan untuk konsistensi
      String fullFileName = String(file.name());
      // Tampilkan nama file tanpa slash untuk UI
      String displayFileName = fullFileName;
      if (displayFileName.startsWith("/")) {
        displayFileName = displayFileName.substring(1);
      }
      
      html += "<tr>";
      html += "<td>" + displayFileName + "</td>";
      html += "<td>" + String(file.size() / 1024.0, 1) + " KB</td>";
      html += "<td>";
      html += "<form style='display:inline;' method='POST' action='/deletefile'>";
      html += "<input type='hidden' name='filename' value='" + fullFileName + "'>";
      html += "<input type='submit' class='delete' value='Delete'>";
      html += "</form>";
      html += "</td>";
      html += "</tr>";
    }
    file = root.openNextFile();
  }
}
  
  html += "</table>";
  html += "</div>";
  
  // Back Button
  html += "<div>";
  html += "<a href='/'><button class='back'>Back to Main Menu</button></a>";
  html += "</div>";
  
  // Add JavaScript for upload progress
  html += "<script>";
  html += "document.getElementById('upload_form').onsubmit = function(e) {";
  html += "  e.preventDefault();";
  html += "  var form = document.getElementById('upload_form');";
  html += "  var file = document.getElementById('file').files[0];";
  html += "  var formData = new FormData();";
  html += "  formData.append('file', file);";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.upload.addEventListener('progress', progressHandler, false);";
  html += "  xhr.addEventListener('load', completeHandler, false);";
  html += "  xhr.addEventListener('error', errorHandler, false);";
  html += "  xhr.addEventListener('abort', abortHandler, false);";
  html += "  xhr.open('POST', '/upload');";
  html += "  xhr.send(formData);";
  html += "};";
  html += "function progressHandler(event) {";
  html += "  document.getElementById('progressBar').style.display = 'block';";
  html += "  var percent = (event.loaded / event.total) * 100;";
  html += "  document.getElementById('progressBar').value = Math.round(percent);";
  html += "  document.getElementById('status').innerHTML = Math.round(percent) + '% uploaded... please wait';";
  html += "  document.getElementById('loaded_n_total').innerHTML = 'Uploaded ' + Math.round(event.loaded/1024) + ' KB of ' + Math.round(event.total/1024) + ' KB';";
  html += "}";
  html += "function completeHandler(event) {";
  html += "  document.getElementById('status').innerHTML = event.target.responseText || 'Upload Complete!';";
  html += "  document.getElementById('progressBar').value = 100;";
  html += "  setTimeout(function(){ location.reload(); }, 1500);";
  html += "}";
  html += "function errorHandler(event) {";
  html += "  document.getElementById('status').innerHTML = 'Upload Failed';";
  html += "}";
  html += "function abortHandler(event) {";
  html += "  document.getElementById('status').innerHTML = 'Upload Aborted';";
  html += "}";
  html += "</script>";
  
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void WebConfig::handleFileUpload() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    
    // Pastikan nama file dimulai dengan slash
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    
    Serial.println("Upload File: " + filename);
    
    // Remove existing file with same name
    if (FFat.exists(filename)) {
      FFat.remove(filename);
    }
    
    uploadFile = FFat.open(filename, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Failed to open file for writing");
      return server.send(500, "text/plain", "Failed to open file for writing");
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.println("Upload Complete: " + String(upload.totalSize) + " bytes");
      server.sendHeader("Location", "/filemanager");
      server.send(303);
    } else {
      server.send(500, "text/plain", "Upload failed");
    }
  }
}

void WebConfig::handleClient() {
  server.handleClient();
}

void WebConfig::saveDataToFile() {
  File file = FFat.open("/config.txt", FILE_WRITE);
  if (file) {
    // Simpan jumlah jaringan
    file.println(networkCount);
    
    // Simpan setiap jaringan
    for (int i = 0; i < networkCount; i++) {
      file.println(ssids[i]);
      file.println(passwords[i]);
    }
    
    // Simpan data lainnya
    file.println(data1);
    file.println(data2);
    file.close();
    Serial.println("Data saved to file.");
  } else {
    Serial.println("Failed to open file for writing.");
  }
}

void WebConfig::readDataFromFile() {
  File file = FFat.open("/config.txt", FILE_READ);
  if (file) {
    String countStr = file.readStringUntil('\n');
    countStr.trim();
    networkCount = countStr.toInt();
    
    // Batasi jumlah jaringan ke MAX_NETWORKS
    if (networkCount > MAX_NETWORKS) networkCount = MAX_NETWORKS;
    
    // Baca data setiap jaringan
    for (int i = 0; i < networkCount; i++) {
      ssids[i] = file.readStringUntil('\n');
      ssids[i].trim();
      passwords[i] = file.readStringUntil('\n');
      passwords[i].trim();
    }
    
    // Baca data lainnya
    data1 = file.readStringUntil('\n');
    data1.trim();
    data2 = file.readStringUntil('\n');
    data2.trim();
    
    file.close();
    Serial.println("Data read from file.");
  } else {
    Serial.println("Failed to open file for reading.");
    networkCount = 0;
  }
}

void WebConfig::handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP WiFi Config</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; text-align: center; margin: 0; padding: 20px; }";
  html += "form, div { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); max-width: 400px; margin: 20px auto; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type='submit'], button { background-color: #4CAF50; color: white; border: none; padding: 12px 20px; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 10px 5px; }";
  html += "input[type='submit']:hover, button:hover { background-color: #45a049; }";
  html += "button.reset { background-color: #f44336; }";
  html += "button.reset:hover { background-color: #e53935; }";
  html += "a { text-decoration: none; color: inherit; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>ESP WiFi Config</h1>";
  
  // Server Configuration Form
  html += "<form action='/save' method='POST'>";
  html += "<h2>Server Configuration</h2>";
  html += "<label for='data1'>Server URL 1:</label><br>";
  html += "<input type='text' id='data1' name='data1' value='" + data1 + "'><br>";
  html += "<label for='data2'>Server URL 2:</label><br>";
  html += "<input type='text' id='data2' name='data2' value='" + data2 + "'><br>";
  html += "<input type='submit' value='Save Server Config'>";
  html += "</form>";
  
  // WiFi Networks Button
  html += "<div>";
  html += "<h2>WiFi Networks</h2>";
  html += "<p>Currently saved networks: " + String(networkCount) + " / " + String(MAX_NETWORKS) + "</p>";
  html += "<a href='/networks'><button>Manage WiFi Networks</button></a>";
  
  html += "</div>";
  html += "<div>";
  html += "<h2>File Manager</h2>";
  html += "<a href='/filemanager'><button>Manage MP3 Files</button></a>";
  html += "</div>";

  html += "<div>";
  html += "<h2>Mogi Configuration</h2>";
  html += "<a href='/mogiconfig'><button>Configure Mogi</button></a>";
  html += "</div>";

  html += "<div>";
  html += "<h2>Friend Settings</h2>";
  html += "<a href='/friendsettings'><button>Manage Friends</button></a>";
  html += "</div>";

  // Reset Button
  html += "<div>";
  html += "<a href='/reset'><button class='reset'>Reset ESP</button></a>";
  html += "</div>";
  
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// New methods implementation
void WebConfig::loadMogiConfig() {
  File file = FFat.open("/mogiconfig.txt", FILE_READ);
  if (file) {
    mogiConfig.name = file.readStringUntil('\n'); mogiConfig.name.trim();
    mogiConfig.serialNumber = file.readStringUntil('\n'); mogiConfig.serialNumber.trim();
    
    String value = file.readStringUntil('\n'); value.trim();
    mogiConfig.eyeWidth = value.toInt();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.eyeHeight = value.toInt();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.borderRadius = value.toInt();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.spaceBetween = value.toInt();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.autoBlinker = (value == "1");
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.autoBlinkerTime = value.toInt();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.autoBlinkerVariation = value.toInt();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.breathing = (value == "1");
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.breathingSpeed = value.toFloat();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.breathingAmount = value.toFloat();
    
    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.textColor = (uint16_t)value.toInt();

    value = file.readStringUntil('\n'); value.trim();
    mogiConfig.eyeColor = value.toInt();
    
    file.close();
    Serial.println("Mogi config loaded from file.");
  } else {
    // Default values if no config file exists
    mogiConfig.name = "Mogi";
    mogiConfig.serialNumber = "M0001";
    mogiConfig.eyeWidth = 40;
    mogiConfig.eyeHeight = 40;
    mogiConfig.borderRadius = 12;
    mogiConfig.spaceBetween = 30;
    mogiConfig.autoBlinker = true;
    mogiConfig.autoBlinkerTime = 3;
    mogiConfig.autoBlinkerVariation = 2;
    mogiConfig.breathing = true;
    mogiConfig.breathingSpeed = 2.0;
    mogiConfig.breathingAmount = 1.369;
    mogiConfig.textColor = TFT_WHITE;
    mogiConfig.eyeColor = 0x17bc;
    
    Serial.println("Created default Mogi config.");
  }
}

void WebConfig::saveMogiConfig() {
  File file = FFat.open("/mogiconfig.txt", FILE_WRITE);
  if (file) {
    file.println(mogiConfig.name);
    file.println(mogiConfig.serialNumber);
    file.println(mogiConfig.eyeWidth);
    file.println(mogiConfig.eyeHeight);
    file.println(mogiConfig.borderRadius);
    file.println(mogiConfig.spaceBetween);
    file.println(mogiConfig.autoBlinker ? "1" : "0");
    file.println(mogiConfig.autoBlinkerTime);
    file.println(mogiConfig.autoBlinkerVariation);
    file.println(mogiConfig.breathing ? "1" : "0");
    file.println(mogiConfig.breathingSpeed);
    file.println(mogiConfig.breathingAmount);
    file.println((int)mogiConfig.textColor);
    file.println((int)mogiConfig.eyeColor);
    
    file.close();
    Serial.println("Mogi config saved to file.");
  } else {
    Serial.println("Failed to open Mogi config file for writing.");
  }
}

void WebConfig::handleMogiConfig() {
  String html = "<!DOCTYPE html><html lang='en'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Mogi Configuration</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; text-align: center; margin: 0; padding: 20px; }";
  html += "form { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); max-width: 500px; margin: 20px auto; }";
  html += "label { display: block; text-align: left; margin-top: 10px; }";
  html += "input[type='text'], input[type='number'], select { width: 100%; padding: 10px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type='color'] { width: 100%; height: 40px; border: 1px solid #ddd; border-radius: 4px; }";
  html += "input[type='checkbox'] { margin-right: 10px; }";
  html += "input[type='submit'], button { background-color: #4CAF50; color: white; border: none; padding: 12px 20px; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 10px 5px; }";
  html += "input[type='submit']:hover, button:hover { background-color: #45a049; }";
  html += "button.back { background-color: #2196F3; }";
  html += "button.back:hover { background-color: #0b7dda; }";
  html += "fieldset { border: 1px solid #ddd; border-radius: 4px; padding: 15px; margin: 15px 0; }";
  html += "legend { padding: 0 10px; font-weight: bold; }";
  html += "a { text-decoration: none; color: inherit; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>Mogi Configuration</h1>";
  
  html += "<form action='/savemogiconfig' method='POST'>";
  
  // Basic Info
  html += "<fieldset>";
  html += "<legend>Basic Information</legend>";
  html += "<label for='name'>Name:</label>";
  html += "<input type='text' id='name' name='name' value='" + mogiConfig.name + "'>";
  html += "<label for='serialNumber'>Serial Number:</label>";
  html += "<input type='text' id='serialNumber' name='serialNumber' value='" + mogiConfig.serialNumber + "'>";
  html += "</fieldset>";
  
  // Eye Appearance
  html += "<fieldset>";
  html += "<legend>Eye Appearance</legend>";
  html += "<label for='eyeWidth'>Eye Width:</label>";
  html += "<input type='number' id='eyeWidth' name='eyeWidth' min='10' max='100' value='" + String(mogiConfig.eyeWidth) + "'>";
  html += "<label for='eyeHeight'>Eye Height:</label>";
  html += "<input type='number' id='eyeHeight' name='eyeHeight' min='10' max='100' value='" + String(mogiConfig.eyeHeight) + "'>";
  html += "<label for='borderRadius'>Border Radius:</label>";
  html += "<input type='number' id='borderRadius' name='borderRadius' min='0' max='50' value='" + String(mogiConfig.borderRadius) + "'>";
  html += "<label for='spaceBetween'>Space Between Eyes:</label>";
  html += "<input type='number' id='spaceBetween' name='spaceBetween' min='0' max='100' value='" + String(mogiConfig.spaceBetween) + "'>";
  
  // Color picker for text
  html += "<label for='textColor'>Text Color:</label>";
  html += "<select id='textColor' name='textColor'>";
  html += "<option value='" + String(TFT_WHITE) + "' " + (mogiConfig.textColor == TFT_WHITE ? "selected" : "") + ">White</option>";
  html += "<option value='" + String(TFT_RED) + "' " + (mogiConfig.textColor == TFT_RED ? "selected" : "") + ">Red</option>";
  html += "<option value='" + String(TFT_GREEN) + "' " + (mogiConfig.textColor == TFT_GREEN ? "selected" : "") + ">Green</option>";
  html += "<option value='" + String(TFT_BLUE) + "' " + (mogiConfig.textColor == TFT_BLUE ? "selected" : "") + ">Blue</option>";
  html += "<option value='" + String(TFT_YELLOW) + "' " + (mogiConfig.textColor == TFT_YELLOW ? "selected" : "") + ">Yellow</option>";
  html += "<option value='" + String(TFT_CYAN) + "' " + (mogiConfig.textColor == TFT_CYAN ? "selected" : "") + ">Cyan</option>";
  html += "<option value='" + String(TFT_MAGENTA) + "' " + (mogiConfig.textColor == TFT_MAGENTA ? "selected" : "") + ">Magenta</option>";
  html += "</select>";
  
  // Eye Color picker
  html += "<label for='eyeColor'>Eye Color:</label>";
  html += "<select id='eyeColor' name='eyeColor'>";
  html += "<option value='" + String(0x17bc) + "' " + (mogiConfig.eyeColor == 0x17bc ? "selected" : "") + ">Default (Blue-Green)</option>";
  html += "<option value='" + String(TFT_RED) + "' " + (mogiConfig.eyeColor == TFT_RED ? "selected" : "") + ">Red</option>";
  html += "<option value='" + String(TFT_GREEN) + "' " + (mogiConfig.eyeColor == TFT_GREEN ? "selected" : "") + ">Green</option>";
  html += "<option value='" + String(TFT_BLUE) + "' " + (mogiConfig.eyeColor == TFT_BLUE ? "selected" : "") + ">Blue</option>";
  html += "<option value='" + String(TFT_YELLOW) + "' " + (mogiConfig.eyeColor == TFT_YELLOW ? "selected" : "") + ">Yellow</option>";
  html += "<option value='" + String(TFT_CYAN) + "' " + (mogiConfig.eyeColor == TFT_CYAN ? "selected" : "") + ">Cyan</option>";
  html += "<option value='" + String(TFT_MAGENTA) + "' " + (mogiConfig.eyeColor == TFT_MAGENTA ? "selected" : "") + ">Magenta</option>";
  html += "<option value='" + String(TFT_WHITE) + "' " + (mogiConfig.eyeColor == TFT_WHITE ? "selected" : "") + ">White</option>";
  html += "</select>";

  html += "</fieldset>";
  
  // Animation
  html += "<fieldset>";
  html += "<legend>Animation Settings</legend>";
  html += "<label>";
  html += "<input type='checkbox' id='autoBlinker' name='autoBlinker' value='1' " + String(mogiConfig.autoBlinker ? "checked" : "") + ">";
  html += "Auto Blinker";
  html += "</label>";
  html += "<label for='autoBlinkerTime'>Blink Interval (seconds):</label>";
  html += "<input type='number' id='autoBlinkerTime' name='autoBlinkerTime' min='1' max='10' step='1' value='" + String(mogiConfig.autoBlinkerTime) + "'>";
  html += "<label for='autoBlinkerVariation'>Blink Variation (±seconds):</label>";
  html += "<input type='number' id='autoBlinkerVariation' name='autoBlinkerVariation' min='0' max='5' step='1' value='" + String(mogiConfig.autoBlinkerVariation) + "'>";
  
  html += "<label>";
  html += "<input type='checkbox' id='breathing' name='breathing' value='1' " + String(mogiConfig.breathing ? "checked" : "") + ">";
  html += "Breathing Animation";
  html += "</label>";
  html += "<label for='breathingSpeed'>Breathing Speed:</label>";
  html += "<input type='number' id='breathingSpeed' name='breathingSpeed' min='0.5' max='5' step='0.1' value='" + String(mogiConfig.breathingSpeed) + "'>";
  html += "<label for='breathingAmount'>Breathing Amount:</label>";
  html += "<input type='number' id='breathingAmount' name='breathingAmount' min='0.1' max='3' step='0.1' value='" + String(mogiConfig.breathingAmount) + "'>";
  html += "</fieldset>";
  
  html += "<input type='submit' value='Save Configuration'>";
  html += "</form>";
  
  // Back Button
  html += "<div style='max-width: 500px; margin: 20px auto;'>";
  html += "<a href='/'><button class='back'>Back to Main Menu</button></a>";
  html += "</div>";
  
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void WebConfig::handleSaveMogiConfig() {
  mogiConfig.name = server.arg("name");
  mogiConfig.serialNumber = server.arg("serialNumber");
  mogiConfig.eyeWidth = server.arg("eyeWidth").toInt();
  mogiConfig.eyeHeight = server.arg("eyeHeight").toInt();
  mogiConfig.borderRadius = server.arg("borderRadius").toInt();
  mogiConfig.spaceBetween = server.arg("spaceBetween").toInt();
  mogiConfig.autoBlinker = server.hasArg("autoBlinker");
  mogiConfig.autoBlinkerTime = server.arg("autoBlinkerTime").toInt();
  mogiConfig.autoBlinkerVariation = server.arg("autoBlinkerVariation").toInt();
  mogiConfig.breathing = server.hasArg("breathing");
  mogiConfig.breathingSpeed = server.arg("breathingSpeed").toFloat();
  mogiConfig.breathingAmount = server.arg("breathingAmount").toFloat();
  mogiConfig.textColor = (uint16_t)server.arg("textColor").toInt();
  mogiConfig.eyeColor = (uint16_t)server.arg("eyeColor").toInt();
  
  saveMogiConfig();
  
  server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='3;url=/mogiconfig'></head><body>Mogi configuration saved. Returning to configuration page in 3 seconds...</body></html>");
}

MogiConfig WebConfig::getMogiConfig() {
  return mogiConfig;
}

void WebConfig::handleNetworks() {
  String html = "<!DOCTYPE html><html lang='en'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>WiFi Networks</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; text-align: center; margin: 0; padding: 20px; }";
  html += "div { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); max-width: 400px; margin: 20px auto; }";
  html += "form { margin: 15px 0; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type='submit'], button { background-color: #4CAF50; color: white; border: none; padding: 12px 20px; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 10px 5px; }";
  html += "input[type='submit']:hover, button:hover { background-color: #45a049; }";
  html += "button.remove { background-color: #f44336; }";
  html += "button.remove:hover { background-color: #e53935; }";
  html += "button.back { background-color: #2196F3; }";
  html += "button.back:hover { background-color: #0b7dda; }";
  html += "table { width: 100%; border-collapse: collapse; margin: 15px 0; }";
  html += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "th { background-color: #f2f2f2; }";
  html += "a { text-decoration: none; color: inherit; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>WiFi Networks</h1>";
  
  // Daftar jaringan yang tersimpan
  html += "<div>";
  html += "<h2>Saved Networks</h2>";
  
  if (networkCount > 0) {
    html += "<table>";
    html += "<tr><th>No</th><th>SSID</th><th>Action</th></tr>";
    
    for (int i = 0; i < networkCount; i++) {
      html += "<tr>";
      html += "<td>" + String(i+1) + "</td>";
      html += "<td>" + ssids[i] + "</td>";
      html += "<td><form action='/removenetwork' method='POST'>";
      html += "<input type='hidden' name='index' value='" + String(i) + "'>";
      html += "<input type='submit' class='remove' value='Remove'>";
      html += "</form></td>";
      html += "</tr>";
    }
    
    html += "</table>";
  } else {
    html += "<p>No networks saved yet.</p>";
  }
  html += "</div>";
  
  // Form untuk menambahkan jaringan baru
  if (networkCount < MAX_NETWORKS) {
    html += "<div>";
    html += "<h2>Add New Network</h2>";
    html += "<form action='/addnetwork' method='POST'>";
    html += "<label for='ssid'>SSID:</label><br>";
    html += "<input type='text' id='ssid' name='ssid' required><br>";
    html += "<label for='password'>Password:</label><br>";
    html += "<input type='password' id='password' name='password'><br>";
    html += "<input type='submit' value='Add Network'>";
    html += "</form>";
    html += "</div>";
  }
  
  // Back Button
  html += "<div>";
  html += "<a href='/'><button class='back'>Back to Main Menu</button></a>";
  html += "</div>";
  
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void WebConfig::handleAddNetwork() {
  if (networkCount < MAX_NETWORKS) {
    String newSsid = server.arg("ssid");
    String newPassword = server.arg("password");
    
    if (newSsid.length() > 0) {
      // Check if network already exists
      bool exists = false;
      for (int i = 0; i < networkCount; i++) {
        if (ssids[i] == newSsid) {
          // Update password if network exists
          passwords[i] = newPassword;
          exists = true;
          break;
        }
      }
      
      // Add new network if it doesn't exist
      if (!exists) {
        ssids[networkCount] = newSsid;
        passwords[networkCount] = newPassword;
        networkCount++;
      }
      
      saveDataToFile();
      server.sendHeader("Location", "/networks");
      server.send(303);
    } else {
      server.send(400, "text/plain", "SSID cannot be empty");
    }
  } else {
    server.send(400, "text/plain", "Maximum number of networks reached");
  }
}

void WebConfig::handleRemoveNetwork() {
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    
    if (index >= 0 && index < networkCount) {
      // Shift all elements down to remove the network
      for (int i = index; i < networkCount - 1; i++) {
        ssids[i] = ssids[i + 1];
        passwords[i] = passwords[i + 1];
      }
      networkCount--;
      
      saveDataToFile();
    }
  }
  server.sendHeader("Location", "/networks");
  server.send(303);
}

void WebConfig::handleSave() {
  data1 = server.arg("data1");
  data2 = server.arg("data2");
  saveDataToFile();
  server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='3;url=/'></head><body>Data saved. Returning to main page in 3 seconds...</body></html>");
}

void WebConfig::handleReset() {
  server.send(200, "text/plain", "Resetting ESP...");
  delay(1000);
  ESP.restart();
}

bool WebConfig::connectWiFi() {
  if (networkCount == 0) return false;
  
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("Attempting to connect to WiFi network %d: %s\n", i+1, ssids[i].c_str());
    
    WiFi.begin(ssids[i].c_str(), passwords[i].c_str());
    
    // Try to connect for a few seconds
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nConnected to WiFi network: %s\n", ssids[i].c_str());
      Serial.println(WiFi.localIP());
      statusWifi = true;
      return true;
    }
    
    Serial.printf("\nFailed to connect to WiFi network: %s\n", ssids[i].c_str());
  }
  
  Serial.println("Failed to connect to any WiFi network.");
  statusWifi = false;
  return false;
}

void WebConfig::setupAP() {
  WiFi.softAP("mogi", "88888888");
  Serial.println("Access Point started.");
  Serial.println("SSID: mogi");
  Serial.println("Password: 88888888");
  statusWifi = false;
}

void WebConfig::getFileList(void) {
  Serial.println(F("\r\nListing FFAT files:"));
  static const char line[] PROGMEM = "=================================================";

  Serial.println(FPSTR(line));
  Serial.println(F("  File name                              Size"));
  Serial.println(FPSTR(line));

  fs::File root = FFat.open("/");
  if (!root) {
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(F("Not a directory"));
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("DIR : ");
      String fileName = file.name();
      Serial.print(fileName);
    } else {
      String fileName = file.name();
      Serial.print("  " + fileName);
      int spaces = 33 - fileName.length();
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      String fileSize = (String)file.size();
      spaces = 10 - fileSize.length();
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      Serial.println(fileSize + " bytes");
    }
    file = root.openNextFile();
  }
  Serial.println(FPSTR(line));
  Serial.println();
  delay(1000);
}

bool WebConfig::addNetwork(String ssid, String password) {
  if (networkCount < MAX_NETWORKS && ssid.length() > 0) {
    // Check if network already exists
    for (int i = 0; i < networkCount; i++) {
      if (ssids[i] == ssid) {
        // Update password if network exists
        passwords[i] = password;
        saveDataToFile();
        return true;
      }
    }
    
    // Add new network
    ssids[networkCount] = ssid;
    passwords[networkCount] = password;
    networkCount++;
    saveDataToFile();
    return true;
  }
  return false;
}

bool WebConfig::removeNetwork(int index) {
  if (index >= 0 && index < networkCount) {
    // Shift all elements down to remove the network
    for (int i = index; i < networkCount - 1; i++) {
      ssids[i] = ssids[i + 1];
      passwords[i] = passwords[i + 1];
    }
    networkCount--;
    saveDataToFile();
    return true;
  }
  return false;
}

void WebConfig::listNetworks() {
  Serial.println("Saved WiFi Networks:");
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("%d. SSID: %s\n", i+1, ssids[i].c_str());
  }
}

bool WebConfig::cekStatusWifi() {
  if (statusWifi) {
    Serial.println("Wifi Terhubung");
    Serial.println("SSID: " + WiFi.SSID());
    Serial.println(WiFi.localIP());
    Serial.println("mDNS responder started");
    Serial.print("Access it at http://");
    Serial.print(_hostname);
    Serial.println(".local");
  } else {
    Serial.println("Mode Akses Point");
    Serial.println("SSID: mogi");
    Serial.println("Password: 88888888");
    Serial.println(WiFi.softAPIP());
  }
  return statusWifi;
}

String WebConfig::getServerUrl() {
  return data1;
}

String WebConfig::getServerUrl2() {
  return data2;
}

void WebConfig::saveFriendsToFile() {
  File file = FFat.open("/friends.txt", FILE_WRITE);
  if (file) {
    file.println(friendCount);
    for (int i = 0; i < friendCount; i++) {
      file.println(friends[i].name);
      file.println(friends[i].serialNumber);
      file.println(friends[i].isActive ? "1" : "0");
    }
    file.close();
    Serial.println("Friends data saved to file.");
  } else {
    Serial.println("Failed to open friends file for writing.");
  }
}

void WebConfig::readFriendsFromFile() {
  File file = FFat.open("/friends.txt", FILE_READ);
  if (file) {
    String countStr = file.readStringUntil('\n');
    countStr.trim();
    friendCount = countStr.toInt();
    
    if (friendCount > MAX_FRIENDS) friendCount = MAX_FRIENDS;
    
    for (int i = 0; i < friendCount; i++) {
      friends[i].name = file.readStringUntil('\n');
      friends[i].name.trim();
      friends[i].serialNumber = file.readStringUntil('\n');
      friends[i].serialNumber.trim();
      String activeStr = file.readStringUntil('\n');
      activeStr.trim();
      friends[i].isActive = (activeStr == "1");
    }
    file.close();
    Serial.println("Friends data read from file.");
  } else {
    Serial.println("No friends data file found.");
    friendCount = 0;
  }
}

void WebConfig::saveMessagesToFile() {
  File file = FFat.open("/messages.txt", FILE_WRITE);
  if (file) {
    file.println(messageCount);
    for (int i = 0; i < messageCount; i++) {
      file.println(messages[i].fromSerialNumber);
      file.println(messages[i].content);
      file.println(messages[i].timestamp);
      file.println(messages[i].isRead ? "1" : "0");
    }
    file.close();
    Serial.println("Messages data saved to file.");
  } else {
    Serial.println("Failed to open messages file for writing.");
  }
}

void WebConfig::readMessagesFromFile() {
  File file = FFat.open("/messages.txt", FILE_READ);
  if (file) {
    String countStr = file.readStringUntil('\n');
    countStr.trim();
    messageCount = countStr.toInt();
    
    if (messageCount > MAX_MESSAGES) messageCount = MAX_MESSAGES;
    
    for (int i = 0; i < messageCount; i++) {
      messages[i].fromSerialNumber = file.readStringUntil('\n');
      messages[i].fromSerialNumber.trim();
      messages[i].content = file.readStringUntil('\n');
      messages[i].content.trim();
      messages[i].timestamp = file.readStringUntil('\n');
      messages[i].timestamp.trim();
      String readStr = file.readStringUntil('\n');
      readStr.trim();
      messages[i].isRead = (readStr == "1");
    }
    file.close();
    Serial.println("Messages data read from file.");
  } else {
    Serial.println("No messages data file found.");
    messageCount = 0;
  }
}

void WebConfig::handleFriendSettings() {
  String html = "<html><head>";
  html += "<title>Friend and Messages</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }";
  html += ".container { max-width: 800px; margin: 0 auto; }";
  html += ".section { margin-bottom: 20px; padding: 15px; border: 1px solid #ddd; border-radius: 5px; background-color: white; }";
  html += "button { padding: 5px 10px; margin: 5px; border: none; border-radius: 3px; cursor: pointer; }";
  html += ".message { padding: 15px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; background-color: white; }";
  html += ".unread { background-color: #e3f2fd; border-left: 4px solid #2196F3; }";
  html += ".message-header { display: flex; justify-content: space-between; margin-bottom: 10px; color: #666; }";
  html += ".message-content { color: #333; margin: 10px 0; }";
  html += ".message-time { color: #888; font-size: 0.9em; }";
  html += ".back-button { background-color: #2196F3; color: white; padding: 10px 20px; text-decoration: none; display: inline-block; margin: 20px 0; border-radius: 5px; }";
  html += "input[type='text'], select { padding: 8px; margin: 5px; border: 1px solid #ddd; border-radius: 3px; }";
  html += "button[type='submit'] { background-color: #4CAF50; color: white; }";
  html += "button.remove { background-color: #f44336; color: white; }";
  html += "button.mark-read { background-color: #2196F3; color: white; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  
  html += "<h1>Friend and Messages</h1>";
  
  // Form untuk menambah teman
  html += "<div class='section'>";
  html += "<h2>Add Friend</h2>";
  html += "<form action='/addfriend' method='post'>";
  html += "<input type='text' name='name' placeholder='Friend Name' required>";
  html += "<input type='text' name='serialNumber' placeholder='Serial Number' required>";
  html += "<button type='submit'>Add Friend</button>";
  html += "</form>";
  html += "</div>";
  
  // Daftar teman
  html += "<div class='section'>";
  html += "<h2>Friends List</h2>";
  for (int i = 0; i < friendCount; i++) {
    html += "<div>";
    html += friends[i].name + " (" + friends[i].serialNumber + ")";
    html += "<form action='/removefriend' method='post' style='display:inline;'>";
    html += "<input type='hidden' name='index' value='" + String(i) + "'>";
    html += "<button type='submit'>Remove</button>";
    html += "</form>";
    html += "</div>";
  }
  html += "</div>";
  
  // Form untuk mengirim pesan
  html += "<div class='section'>";
  html += "<h2>Send Message</h2>";
  html += "<form action='/sendmessage' method='post'>";
  html += "<select name='to' required>";
  html += "<option value='broadcast'>Broadcast to All Friends</option>";
  for (int i = 0; i < friendCount; i++) {
    html += "<option value='" + friends[i].serialNumber + "'>" + friends[i].name + "</option>";
  }
  html += "</select>";
  html += "<input type='text' name='content' placeholder='Message' required>";
  html += "<button type='submit'>Send</button>";
  html += "</form>";
  html += "</div>";
  
  // Daftar pesan
  html += "<div class='section'>";
  html += "<h2>Messages</h2>";
  for (int i = 0; i < messageCount; i++) {
    html += "<div class='message " + String(messages[i].isRead ? "" : "unread") + "'>";
    html += "<div class='message-header'>";
    html += "<strong>From: " + messages[i].fromSerialNumber + "</strong>";
    html += "<span class='message-time'>" + messages[i].timestamp + "</span>";
    html += "</div>";
    html += "<div class='message-content'>" + messages[i].content + "</div>";
    if (!messages[i].isRead) {
      html += "<form action='/markasread' method='post' style='display:inline;'>";
      html += "<input type='hidden' name='index' value='" + String(i) + "'>";
      html += "<button type='submit' class='mark-read'>Mark as Read</button>";
      html += "</form>";
    }
    html += "<form action='/deletemessage' method='post' style='display:inline;'>";
    html += "<input type='hidden' name='index' value='" + String(i) + "'>";
    html += "<button type='submit' class='remove'>Delete</button>";
    html += "</form>";
    html += "</div>";
  }
  html += "</div>";
  
  // Back Button
  html += "<div style='text-align: center; margin-top: 20px;'>";
  html += "<a href='/' class='back-button'>Back to Main Menu</a>";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void WebConfig::handleAddFriend() {
  if (friendCount < MAX_FRIENDS) {
    String name = server.arg("name");
    String serialNumber = server.arg("serialNumber");
    
    if (name.length() > 0 && serialNumber.length() > 0) {
      friends[friendCount].name = name;
      friends[friendCount].serialNumber = serialNumber;
      friends[friendCount].isActive = true;
      friendCount++;
      
      saveFriendsToFile();
    }
  }
  server.sendHeader("Location", "/friendsettings");
  server.send(303);
}

void WebConfig::handleRemoveFriend() {
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    
    if (index >= 0 && index < friendCount) {
      // Shift all elements down to remove the friend
      for (int i = index; i < friendCount - 1; i++) {
        friends[i] = friends[i + 1];
      }
      friendCount--;
      
      saveFriendsToFile();
    }
  }
  server.sendHeader("Location", "/friendsettings");
  server.send(303);
}

void WebConfig::handleMarkAsRead() {
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    
    if (index >= 0 && index < messageCount) {
      messages[index].isRead = true;
      saveMessagesToFile();
    }
  }
  server.sendHeader("Location", "/friendsettings");
  server.send(303);
}

void WebConfig::handleDeleteMessage() {
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    
    if (index >= 0 && index < messageCount) {
      // Shift all elements down to remove the message
      for (int i = index; i < messageCount - 1; i++) {
        messages[i] = messages[i + 1];
      }
      messageCount--;
      
      saveMessagesToFile();
    }
  }
  server.sendHeader("Location", "/friendsettings");
  server.send(303);
}

bool WebConfig::sendMessage(String toSerialNumber, String content) {
  if (messageCount < MAX_MESSAGES) {
    // Cek apakah penerima ada di daftar teman
    bool friendFound = false;
    for (int i = 0; i < friendCount; i++) {
      if (friends[i].serialNumber == toSerialNumber && friends[i].isActive) {
        friendFound = true;
        break;
      }
    }
    
    if (!friendFound) {
      Serial.println("Penerima tidak ditemukan atau tidak aktif");
      return false;
    }
    
    // Kirim pesan ke server
    HTTPClient http;
    String serverUrl = getServerUrl() + "/send_message";
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Serial-Number", mogiConfig.serialNumber);
    
    String jsonData = "{";
    jsonData += "\"to\":\"" + toSerialNumber + "\",";
    jsonData += "\"content\":\"" + content + "\"";
    jsonData += "}";
    
    int httpCode = http.POST(jsonData);
    if (httpCode == HTTP_CODE_OK) {
      // Simpan pesan lokal sebagai backup
      messages[messageCount].fromSerialNumber = mogiConfig.serialNumber;
      messages[messageCount].content = content;
      messages[messageCount].timestamp = String(millis());
      messages[messageCount].isRead = false;
      messageCount++;
      saveMessagesToFile();
      http.end();
      return true;
    } else {
      Serial.println("Gagal mengirim pesan ke server");
      http.end();
      return false;
    }
  }
  return false;
}

void WebConfig::handleSendMessage() {
  if (server.hasArg("to") && server.hasArg("content")) {
    String toSerialNumber = server.arg("to");
    String content = server.arg("content");
    
    if (toSerialNumber == "broadcast") {
      // Kirim ke semua teman yang aktif
      bool success = true;
      for (int i = 0; i < friendCount; i++) {
        if (friends[i].isActive) {
          if (!sendMessage(friends[i].serialNumber, content)) {
            success = false;
          }
        }
      }
      if (success) {
        server.sendHeader("Location", "/");
        server.send(303);
      } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Failed to send broadcast message\"}");
      }
    } else {
      if (sendMessage(toSerialNumber, content)) {
        server.sendHeader("Location", "/");
        server.send(303);
      } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Failed to send message\"}");
      }
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
  }
}

void WebConfig::handleGetMessages() {
  String json = "[";
  for (int i = 0; i < messageCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"from\":\"" + messages[i].fromSerialNumber + "\",";
    json += "\"content\":\"" + messages[i].content + "\",";
    json += "\"timestamp\":\"" + messages[i].timestamp + "\",";
    json += "\"isRead\":" + String(messages[i].isRead ? "true" : "false");
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

void WebConfig::handleGetFriends() {
  String json = "[";
  for (int i = 0; i < friendCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"name\":\"" + friends[i].name + "\",";
    json += "\"serialNumber\":\"" + friends[i].serialNumber + "\",";
    json += "\"isActive\":" + String(friends[i].isActive ? "true" : "false");
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

// Fungsi untuk mengecek pesan baru dari server
void WebConfig::checkNewMessagesFromServer() {
  if (!cekStatusWifi()) {
    Serial.println("Tidak ada koneksi WiFi, tidak bisa cek pesan baru");
    return;
  }
  
  HTTPClient http;
  String serverUrl = getServerUrl() + "/get_messages";
  Serial.println("Mengecek pesan baru dari: " + serverUrl);
  
  http.begin(serverUrl);
  http.addHeader("Serial-Number", mogiConfig.serialNumber);
  
  int httpCode = http.GET();
  Serial.println("HTTP Response code: " + String(httpCode));
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Response dari server: " + response);
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      Serial.println("Error parsing JSON: " + String(error.c_str()));
      http.end();
      return;
    }
    
    JsonArray messages = doc.as<JsonArray>();
    Serial.println("Jumlah pesan yang diterima: " + String(messages.size()));
    
    // Reset messageCount untuk memulai dari awal
    messageCount = 0;
    
    for (JsonObject message : messages) {
      String fromSerial = message["from"].as<String>();
      String content = message["content"].as<String>();
      String timestamp = message["timestamp"].as<String>();
      bool isRead = message["is_read"].as<bool>();
      
      // Hanya tambahkan pesan yang belum dibaca atau belum dihapus
      if (!isRead && messageCount < MAX_MESSAGES) {
        Serial.println("Menyimpan pesan baru ke database lokal");
        this->messages[messageCount].fromSerialNumber = fromSerial;
        this->messages[messageCount].content = content;
        this->messages[messageCount].timestamp = timestamp;
        this->messages[messageCount].isRead = isRead;
        messageCount++;
      }
    }
    
    saveMessagesToFile();
    Serial.println("Pesan berhasil disimpan ke file");
  } else {
    Serial.println("Gagal mendapatkan pesan dari server");
  }
  
  http.end();
}
