#ifndef _MATA_ROBOT_GC9A01_H
#define _MATA_ROBOT_GC9A01_H

// menambah waktu / jam
#include <WiFi.h>
#include <time.h>

#include <TFT_eSPI.h>
#include <math.h>
extern TFT_eSPI display;
TFT_eSprite sprite = TFT_eSprite(&display);

#define BGCOLOR 0x0000
// #define MAINCOLOR 0x17bc

#define DEFAULT 0
#define TIRED 1
#define ANGRY 2
#define HAPPY 3

#define ON 1
#define OFF 0

#define N 1
#define NE 2
#define E 3
#define SE 4
#define S 5
#define SW 6
#define W 7
#define NW 8

class roboEyes {
private:
    unsigned char lastMood = DEFAULT;
    unsigned char lastPosition = DEFAULT;
    int lastEyeLx = 0;
    int lastEyeLy = 0;
    int lastEyeRx = 0;
    int lastEyeRy = 0;
    int lastEyeLwidth = 0;
    int lastEyeLheight = 0;
    int lastEyeRwidth = 0;
    int lastEyeRheight = 0;
    
    bool breathing = false;
    float breathingPhase = 0.0;
    float breathingSpeed = 0.05;
    int breathingAmplitude = 3;

    String displayText = "";
    uint16_t textColor = TFT_WHITE;
    // Enable/disable time display
    
    bool isWithinCircularScreen(int x, int y, int width, int height) {
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;
        int radius = min(screenWidth, screenHeight) / 2;
        
        int corners[4][2] = {
            {x, y},
            {x + width, y},
            {x, y + height},
            {x + width, y + height}
        };
        
        for (int i = 0; i < 4; i++) {
            int dx = corners[i][0] - centerX;
            int dy = corners[i][1] - centerY;
            int distance = sqrt(dx*dx + dy*dy);
            
            if (distance > radius - 5) {
                return false;
            }
        }
        
        return true;
    }

    void findValidCircularPosition(int &x, int &y) {
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;
        int radius = min(screenWidth, screenHeight) / 2;
        
        for (int attempts = 0; attempts < 10; attempts++) {
            float angle = random(360) * PI / 180.0;
            float distance = random(radius - max(eyeLwidthCurrent, eyeLheightCurrent) - 10);
            
            x = centerX + cos(angle) * distance;
            y = centerY + sin(angle) * distance;
            
            if (isWithinCircularScreen(x, y, eyeLwidthCurrent, eyeLheightCurrent) &&
                isWithinCircularScreen(x + eyeLwidthCurrent + spaceBetweenCurrent, y, 
                                    eyeRwidthCurrent, eyeRheightCurrent)) {
                return;
            }
        }
        
        x = centerX - (eyeLwidthCurrent + spaceBetweenCurrent + eyeRwidthCurrent) / 2;
        y = centerY - eyeLheightCurrent / 2;
    }

    // Time display variables
    bool showTime = false;
    bool timeConfigured = false;
    char timeString[20]; // Format: "HH:MM:SS"
    char dayString[12];  // Day name
    const char* gmtString = "GMT+7"; // GMT indicator
    unsigned long lastTimeUpdate = 0;
    const long timeUpdateInterval = 1000; // Update time every second
    const char* ntpServer = "pool.ntp.org";
    const long gmtOffset_sec = 7 * 3600;  // GMT+7 in seconds
    const int daylightOffset_sec = 0;     // No DST in Indonesia
    
    // Days of the week in Indonesian
    const char* daysOfWeek[7] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
    
    // Time configuration function
    void configureTime() {
        if (WiFi.status() == WL_CONNECTED && !timeConfigured) {
            // Configure NTP and timezone properly for Jakarta (GMT+7)
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            
            // Force an initial time sync
            struct tm timeinfo;
            if (!getLocalTime(&timeinfo)) {
                Serial.println("Failed to obtain time");
            } else {
                Serial.println("Time synchronized");
                timeConfigured = true;
            }
            
            updateTimeString();
        }
    }
    
    // Update the time string
    void updateTimeString() {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            strcpy(timeString, "--:--:--");
            strcpy(dayString, "---");
            return;
        }
        
        // Format time as HH:MM:SS
        sprintf(timeString, "%02d:%02d:%02d", 
                timeinfo.tm_hour, 
                timeinfo.tm_min, 
                timeinfo.tm_sec);
        
        // Get day name (0-6, Sunday = 0)
        strcpy(dayString, daysOfWeek[timeinfo.tm_wday]);
    }
    
    // Draw time on screen
    void drawTime() {
        if (!showTime) return;
        
        // Update time string every second
        unsigned long currentMillis = millis();
        if (currentMillis - lastTimeUpdate >= timeUpdateInterval) {
            updateTimeString();
            lastTimeUpdate = currentMillis;
        }
        
        int centerX = screenWidth / 2;
        int timeFontSize = 2;
        int dayFontSize = 2;
        
        // Calculate positions
        int timeYPos = min(eyeLy, eyeRy) - 30;  // Time position
        int dayYPos = timeYPos - 20;           // Day name position
        
        // Make sure it's within screen bounds
        if (dayYPos < 5) {
            dayYPos = 5;
            timeYPos = dayYPos + 15;
        }
        
        // Save current text settings
        uint16_t oldTextColor = sprite.textcolor;
        uint8_t oldTextSize = sprite.textsize;
        uint8_t oldTextDatum = sprite.textdatum;
        
        // Draw combined day and time
        sprite.setTextColor(TFT_WHITE, TFT_BLACK);
        sprite.setTextSize(dayFontSize);
        sprite.setTextDatum(TC_DATUM); // Top Center alignment
        sprite.drawString(dayString, centerX, dayYPos);
        
        // Draw the time with GMT+7
        sprite.setTextSize(timeFontSize);
        char fullTimeString[25];
        sprintf(fullTimeString, "%s %s", timeString, gmtString);
        sprite.drawString(fullTimeString, centerX, timeYPos);
        
        // Restore previous text settings
        sprite.setTextColor(oldTextColor);
        sprite.setTextSize(oldTextSize);
        sprite.setTextDatum(oldTextDatum);
    }   

public:
    uint16_t MAINCOLOR = 0x17bc;
    int duplikat1 = 0;
    int duplikat2 = 0;
    int screenWidth = 240;
    int screenHeight = 240;
    int frameInterval = 20;
    
    bool tired = 0;
    bool angry = 0;
    bool happy = 0;
    bool curious = 0;
    bool cyclops = 0;
    bool eyeL_open = 0;
    bool eyeR_open = 0;
    
    int eyeLwidthDefault = 36;
    int eyeLheightDefault = 36;
    int eyeLwidthCurrent = eyeLwidthDefault;
    int eyeLheightCurrent = 1;
    int eyeLwidthNext = eyeLwidthDefault;
    int eyeLheightNext = eyeLheightDefault;
    int eyeLheightOffset = 0;
    
    int eyeRwidthDefault = 36;
    int eyeRheightDefault = 36;
    int eyeRwidthCurrent = eyeRwidthDefault;
    int eyeRheightCurrent = 1;
    int eyeRwidthNext = eyeRwidthDefault;
    int eyeRheightNext = eyeRheightDefault;
    int eyeRheightOffset = 0;
    
    byte eyeLborderRadiusDefault = 8;
    byte eyeLborderRadiusCurrent = eyeLborderRadiusDefault;
    byte eyeLborderRadiusNext = eyeLborderRadiusDefault;
    
    byte eyeRborderRadiusDefault = 8;
    byte eyeRborderRadiusCurrent = eyeRborderRadiusDefault;
    byte eyeRborderRadiusNext = eyeRborderRadiusDefault;
    
    int eyeLxDefault;
    int eyeLyDefault;
    int eyeLx;
    int eyeLy;
    int eyeLxNext;
    int eyeLyNext;
    
    int eyeRxDefault;
    int eyeRyDefault;
    int eyeRx;
    int eyeRy;
    int eyeRxNext;
    int eyeRyNext;
    
    byte eyelidsHeightMax;
    byte eyelidsTiredHeight = 0;
    byte eyelidsTiredHeightNext = 0;
    byte eyelidsAngryHeight = 0;
    byte eyelidsAngryHeightNext = 0;
    byte eyelidsHappyBottomOffsetMax;
    byte eyelidsHappyBottomOffset = 0;
    byte eyelidsHappyBottomOffsetNext = 0;
    int spaceBetweenDefault = 10;
    int spaceBetweenCurrent = spaceBetweenDefault;
    int spaceBetweenNext = 10;
    
    bool hFlicker = 0;
    bool hFlickerAlternate = 0;
    byte hFlickerAmplitude = 2;
    bool vFlicker = 0;
    bool vFlickerAlternate = 0;
    byte vFlickerAmplitude = 10;
    bool autoblinker = 0;
    int blinkInterval = 1;
    int blinkIntervalVariation = 4;
    unsigned long blinktimer = 0;
    bool idle = 0;
    int idleInterval = 1;
    int idleIntervalVariation = 3;
    unsigned long idleAnimationTimer = 0;
    bool confused = 0;
    unsigned long confusedAnimationTimer = 0;
    int confusedAnimationDuration = 500;
    bool confusedToggle = 1;
    bool laugh = 0;
    unsigned long laughAnimationTimer = 0;
    int laughAnimationDuration = 500;
    bool laughToggle = 1;

    void setMainColor(uint16_t color) {
        MAINCOLOR = color;
      }
    // Enable/disable time display
    void setTimeDisplay(bool enable) {
        showTime = enable;
        if (enable) {
            timeConfigured = false;  // Force reconfiguration
            configureTime();
        }
    }
    
    // Force time resync (call this if time is incorrect)
    void resyncTime() {
        if (WiFi.status() == WL_CONNECTED) {
            timeConfigured = false;
            configureTime();
        }
    }
    
    // Check if time display is enabled
    bool isTimeDisplayEnabled() {
        return showTime;
    }

    void setText(String text, uint16_t color = TFT_WHITE) {
        displayText = text;
        textColor = color;
    }

    void drawWrappedText() {
        if (displayText.length() == 0) return;
        
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;
        int startY = eyeRy + eyeRheightCurrent + 10;
        int maxWidth = screenWidth - 40;
        int fontSize = 1.2;
        
        uint16_t oldTextColor = sprite.textcolor;
        uint8_t oldTextSize = sprite.textsize;
        uint8_t oldTextDatum = sprite.textdatum;
        
        sprite.setTextColor(textColor, TFT_BLACK);
        sprite.setTextSize(fontSize);
        sprite.setTextDatum(TL_DATUM);
        
        int charsPerLine = maxWidth / (6 * fontSize);
        
        String currentText = displayText;
        int yPos = startY;
        
        while (currentText.length() > 0) {
            String line;
            if (currentText.length() <= charsPerLine) {
                line = currentText;
                currentText = "";
            } else {
                int breakPoint = charsPerLine;
                while (breakPoint > 0 && currentText.charAt(breakPoint) != ' ') {
                    breakPoint--;
                }
                if (breakPoint == 0) breakPoint = charsPerLine;
                
                line = currentText.substring(0, breakPoint);
                currentText = currentText.substring(breakPoint + 1);
            }
            
            int xPos = centerX - (line.length() * 3 * fontSize);
            
            sprite.drawString(line, xPos, yPos, fontSize);
            
            yPos += 10 * fontSize;
            
            int radius = min(screenWidth, screenHeight) / 2;
            if (yPos > centerY + radius - 10) break;
        }
        
        sprite.setTextColor(oldTextColor);
        sprite.setTextSize(oldTextSize);
        sprite.setTextDatum(oldTextDatum);
    }

    String getText(){
        return displayText;
    }
    
    void clearText() {
        displayText = "";
    }

    void begin(int width, int height, byte frameRate) {
        screenWidth = width;
        screenHeight = height;
        eyeLheightCurrent = 1;
        eyeRheightCurrent = 1;
        
        eyeLxDefault = ((screenWidth)-(eyeLwidthDefault+spaceBetweenDefault+eyeRwidthDefault))/2;
        eyeLyDefault = ((screenHeight-eyeLheightDefault)/2);
        eyeLx = eyeLxDefault;
        eyeLy = eyeLyDefault;
        eyeLxNext = eyeLx;
        eyeLyNext = eyeLy;
        
        eyeRxDefault = eyeLx+eyeLwidthCurrent+spaceBetweenDefault;
        eyeRyDefault = eyeLy;
        eyeRx = eyeRxDefault;
        eyeRy = eyeRyDefault;
        eyeRxNext = eyeRx;
        eyeRyNext = eyeRy;
        
        eyelidsHeightMax = eyeLheightDefault/2;
        eyelidsHappyBottomOffsetMax = (eyeLheightDefault/2)+3;
        
        setFramerate(frameRate);
        sprite.createSprite(screenWidth, screenHeight);
        sprite.fillSprite(TFT_BLACK);
    }

    void update() {
        if(duplikat1 != eyeLx || duplikat2 != eyeLy){
            duplikat1 = eyeLx;
            duplikat2 = eyeLy;
            sprite.fillSprite(TFT_BLACK);
        }
        drawEyes();
        sprite.pushSprite(0, 0);
        vTaskDelay(frameInterval / portTICK_PERIOD_MS);
    }

    void setBreathing(bool active, int amplitude = 3, float speed = 0.05) {
        breathing = active;
        breathingAmplitude = amplitude;
        breathingSpeed = speed;
    }
    
    bool isBreathing() {
        return breathing;
    }

    void setFramerate(byte fps) {
        frameInterval = 1000/fps;
    }

    void setWidth(byte leftEye, byte rightEye) {
        eyeLwidthNext = leftEye;
        eyeRwidthNext = rightEye;
        eyeLwidthDefault = leftEye;
        eyeRwidthDefault = rightEye;
    }

    void setHeight(byte leftEye, byte rightEye) {
        eyeLheightNext = leftEye;
        eyeRheightNext = rightEye;
        eyeLheightDefault = leftEye;
        eyeRheightDefault = rightEye;
    }

    void setBorderradius(byte leftEye, byte rightEye) {
        eyeLborderRadiusNext = leftEye;
        eyeRborderRadiusNext = rightEye;
        eyeLborderRadiusDefault = leftEye;
        eyeRborderRadiusDefault = rightEye;
    }

    void setSpacebetween(int space) {
        spaceBetweenNext = space;
        spaceBetweenDefault = space;
    }

    void setMood(unsigned char mood) {
        switch (mood) {
            case TIRED:
                tired=1; angry=0; happy=0;
                break;
            case ANGRY:
                tired=0; angry=1; happy=0;
                break;
            case HAPPY:
                tired=0; angry=0; happy=1;
                break;
            default:
                tired=0; angry=0; happy=0;
                break;
        }
    }

    void setPosition(unsigned char position) {
        int newX = eyeLxNext;
        int newY = eyeLyNext;
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;
        int radius = min(screenWidth, screenHeight) / 2;
        float angle = 0;
        float distance = radius * 0.6;
        
        switch (position) {
            case N:
                angle = 270 * PI / 180.0;
                break;
            case NE:
                angle = 315 * PI / 180.0;
                break;
            case E:
                angle = 0 * PI / 180.0;
                break;
            case SE:
                angle = 45 * PI / 180.0;
                break;
            case S:
                angle = 90 * PI / 180.0;
                break;
            case SW:
                angle = 135 * PI / 180.0;
                break;
            case W:
                angle = 180 * PI / 180.0;
                break;
            case NW:
                angle = 225 * PI / 180.0;
                break;
            default:
                newX = centerX - (eyeLwidthCurrent + spaceBetweenCurrent + eyeRwidthCurrent) / 2;
                newY = centerY - eyeLheightCurrent / 2;
                eyeLxNext = newX;
                eyeLyNext = newY;
                return;
        }
        
        newX = centerX + cos(angle) * distance - (eyeLwidthCurrent + spaceBetweenCurrent + eyeRwidthCurrent) / 2;
        newY = centerY + sin(angle) * distance - eyeLheightCurrent / 2;
        
        if (isWithinCircularScreen(newX, newY, eyeLwidthCurrent, eyeLheightCurrent) &&
            isWithinCircularScreen(newX + eyeLwidthCurrent + spaceBetweenCurrent, newY, 
                                eyeRwidthCurrent, eyeRheightCurrent)) {
            eyeLxNext = newX;
            eyeLyNext = newY;
        } else {
            distance = radius * 0.4;
            newX = centerX + cos(angle) * distance - (eyeLwidthCurrent + spaceBetweenCurrent + eyeRwidthCurrent) / 2;
            newY = centerY + sin(angle) * distance - eyeLheightCurrent / 2;
            
            if (isWithinCircularScreen(newX, newY, eyeLwidthCurrent, eyeLheightCurrent) &&
                isWithinCircularScreen(newX + eyeLwidthCurrent + spaceBetweenCurrent, newY, 
                                    eyeRwidthCurrent, eyeRheightCurrent)) {
                eyeLxNext = newX;
                eyeLyNext = newY;
            } else {
                eyeLxNext = centerX - (eyeLwidthCurrent + spaceBetweenCurrent + eyeRwidthCurrent) / 2;
                eyeLyNext = centerY - eyeLheightCurrent / 2;
            }
        }
    }

    int getScreenConstraint_X() {
        return screenWidth-eyeLwidthCurrent-spaceBetweenCurrent-eyeRwidthCurrent;
    }
    
    int getScreenConstraint_Y() {
        return screenHeight-eyeLheightDefault;
    }

    void setAutoblinker(bool active, int interval, int variation) {
        autoblinker = active;
        blinkInterval = interval;
        blinkIntervalVariation = variation;
    }
    
    void setAutoblinker(bool active) {
        autoblinker = active;
    }

    void setIdleMode(bool active, int interval, int variation) {
        idle = active;
        idleInterval = interval;
        idleIntervalVariation = variation;
    }
    
    void setIdleMode(bool active) {
        idle = active;
    }

    void setCuriosity(bool curiousBit) {
        curious = curiousBit;
    }

    void setCyclops(bool cyclopsBit) {
        cyclops = cyclopsBit;
    }

    void setHFlicker(bool flickerBit, byte Amplitude) {
        hFlicker = flickerBit;
        hFlickerAmplitude = Amplitude;
    }
    
    void setHFlicker(bool flickerBit) {
        hFlicker = flickerBit;
    }

    void setVFlicker(bool flickerBit, byte Amplitude) {
        vFlicker = flickerBit;
        vFlickerAmplitude = Amplitude;
    }
    
    void setVFlicker(bool flickerBit) {
        vFlicker = flickerBit;
    }

    void close() {
        eyeLheightNext = 1;
        eyeRheightNext = 1;
        eyeL_open = 0;
        eyeR_open = 0;
    }

    void open() {
        eyeL_open = 1;
        eyeR_open = 1;
    }

    void blink() {
        close();
        open();
    }

    void close(bool left, bool right) {
        if(left) {
            eyeLheightNext = 1;
            eyeL_open = 0;
        }
        if(right) {
            eyeRheightNext = 1;
            eyeR_open = 0;
        }
    }

    void open(bool left, bool right) {
        if(left) eyeL_open = 1;
        if(right) eyeR_open = 1;
    }

    void blink(bool left, bool right) {
        close(left, right);
        open(left, right);
    }

    void anim_confused() {
        confused = 1;
    }

    void anim_laugh() {
        laugh = 1;
    }

    void drawEyes() {
        if(curious) {
            if(eyeLxNext<=10) {
                eyeLheightOffset=8;
            } else if (eyeLxNext>=(getScreenConstraint_X()-10) && cyclops) {
                eyeLheightOffset=8;
            } else {
                eyeLheightOffset=0;
            }
            if(eyeRxNext>=screenWidth-eyeRwidthCurrent-10) {
                eyeRheightOffset=8;
            } else {
                eyeRheightOffset=0;
            }
        } else {
            eyeLheightOffset=0;
            eyeRheightOffset=0;
        }

        eyeLheightCurrent = (eyeLheightCurrent + eyeLheightNext + eyeLheightOffset)/2;
        eyeLy+= ((eyeLheightDefault-eyeLheightCurrent)/2);
        eyeLy-= eyeLheightOffset/2;
        
        eyeRheightCurrent = (eyeRheightCurrent + eyeRheightNext + eyeRheightOffset)/2;
        eyeRy+= (eyeRheightDefault-eyeRheightCurrent)/2;
        eyeRy-= eyeRheightOffset/2;

        if(eyeL_open && eyeLheightCurrent <= 1 + eyeLheightOffset) {
            eyeLheightNext = eyeLheightDefault;
        }
        if(eyeR_open && eyeRheightCurrent <= 1 + eyeRheightOffset) {
            eyeRheightNext = eyeRheightDefault;
        }

        eyeLwidthCurrent = (eyeLwidthCurrent + eyeLwidthNext)/2;
        eyeRwidthCurrent = (eyeRwidthCurrent + eyeRwidthNext)/2;

        spaceBetweenCurrent = (spaceBetweenCurrent + spaceBetweenNext)/2;

        eyeLx = (eyeLx + eyeLxNext)/2;
        eyeLy = (eyeLy + eyeLyNext)/2;

        eyeRxNext = eyeLxNext+eyeLwidthCurrent+spaceBetweenCurrent;
        eyeRyNext = eyeLyNext;
        eyeRx = (eyeRx + eyeRxNext)/2;
        eyeRy = (eyeRy + eyeRyNext)/2;

        eyeLborderRadiusCurrent = (eyeLborderRadiusCurrent + eyeLborderRadiusNext)/2;
        eyeRborderRadiusCurrent = (eyeRborderRadiusCurrent + eyeRborderRadiusNext)/2;

        if(autoblinker && millis() >= blinktimer) {
            blink();
            blinktimer = millis()+(blinkInterval*1000)+(random(blinkIntervalVariation)*1000);
        }

        if(laugh) {
            if(laughToggle) {
                setVFlicker(1, 5);
                laughAnimationTimer = millis();
                laughToggle = 0;
            } else if(millis() >= laughAnimationTimer+laughAnimationDuration) {
                setVFlicker(0, 0);
                laughToggle = 1;
                laugh=0; 
            }
        }

        if(confused) {
            if(confusedToggle) {
                setHFlicker(1, 20);
                confusedAnimationTimer = millis();
                confusedToggle = 0;
            } else if(millis() >= confusedAnimationTimer+confusedAnimationDuration) {
                setHFlicker(0, 0);
                confusedToggle = 1;
                confused=0; 
            }
        }

        if(idle && millis() >= idleAnimationTimer) {
            int newX, newY;
            findValidCircularPosition(newX, newY);
            eyeLxNext = newX;
            eyeLyNext = newY;
            idleAnimationTimer = millis()+(idleInterval*1000)+(random(idleIntervalVariation)*1000);
        }

        if(breathing) {
            float breathOffset = sin(breathingPhase) * breathingAmplitude;
            
            int tempLy = eyeLy + breathOffset;
            int tempRy = eyeRy + breathOffset;
            
            if(isWithinCircularScreen(eyeLx, tempLy, eyeLwidthCurrent, eyeLheightCurrent) &&
               (!cyclops && isWithinCircularScreen(eyeRx, tempRy, eyeRwidthCurrent, eyeRheightCurrent))) {
                eyeLy = tempLy;
                eyeRy = tempRy;
            }
            
            breathingPhase += breathingSpeed;
            if(breathingPhase > 2 * PI) {
                breathingPhase -= 2 * PI;
            }
        }
        
        if(hFlicker) {
            int tempLx = eyeLx;
            int tempRx = eyeRx;
            
            if(hFlickerAlternate) {
                tempLx += hFlickerAmplitude;
                tempRx += hFlickerAmplitude;
            } else {
                tempLx -= hFlickerAmplitude;
                tempRx -= hFlickerAmplitude;
            }
            
            if(isWithinCircularScreen(tempLx, eyeLy, eyeLwidthCurrent, eyeLheightCurrent) &&
               isWithinCircularScreen(tempRx, eyeRy, eyeRwidthCurrent, eyeRheightCurrent)) {
                eyeLx = tempLx;
                eyeRx = tempRx;
            }
            
            hFlickerAlternate = !hFlickerAlternate;
        }

        if(vFlicker) {
            int tempLy = eyeLy;
            int tempRy = eyeRy;
            
            if(vFlickerAlternate) {
                tempLy += vFlickerAmplitude;
                tempRy += vFlickerAmplitude;
            } else {
                tempLy -= vFlickerAmplitude;
                tempRy -= vFlickerAmplitude;
            }
            
            if(isWithinCircularScreen(eyeLx, tempLy, eyeLwidthCurrent, eyeLheightCurrent) &&
               isWithinCircularScreen(eyeRx, tempRy, eyeRwidthCurrent, eyeRheightCurrent)) {
                eyeLy = tempLy;
                eyeRy = tempRy;
            }
            
            vFlickerAlternate = !vFlickerAlternate;
        }

        if(cyclops) {
            eyeRwidthCurrent = 0;
            eyeRheightCurrent = 0;
            spaceBetweenCurrent = 0;
        }

        sprite.fillRoundRect(eyeLx, eyeLy, eyeLwidthCurrent, eyeLheightCurrent, eyeLborderRadiusCurrent, MAINCOLOR);
        if (!cyclops) {
            sprite.fillRoundRect(eyeRx, eyeRy, eyeRwidthCurrent, eyeRheightCurrent, eyeRborderRadiusCurrent, MAINCOLOR);
        }

        if (tired) {
            eyelidsTiredHeightNext = eyeLheightCurrent/2; 
            eyelidsAngryHeightNext = 0;
            eyelidsTiredHeight = (eyelidsTiredHeight + eyelidsTiredHeightNext)/2;
            if (!cyclops) {
                sprite.fillTriangle(eyeLx, eyeLy-1, eyeLx+eyeLwidthCurrent, eyeLy-1, eyeLx, eyeLy+eyelidsTiredHeight-1, BGCOLOR);
                sprite.fillTriangle(eyeRx, eyeRy-1, eyeRx+eyeRwidthCurrent, eyeRy-1, eyeRx+eyeRwidthCurrent, eyeRy+eyelidsTiredHeight-1, BGCOLOR);
            } else {
                sprite.fillTriangle(eyeLx, eyeLy-1, eyeLx+(eyeLwidthCurrent/2), eyeLy-1, eyeLx, eyeLy+eyelidsTiredHeight-1, BGCOLOR);
                sprite.fillTriangle(eyeLx+(eyeLwidthCurrent/2), eyeLy-1, eyeLx+eyeLwidthCurrent, eyeLy-1, eyeLx+eyeLwidthCurrent, eyeLy+eyelidsTiredHeight-1, BGCOLOR);
            }
        } else {
            eyelidsTiredHeightNext = 0;
        }
        
        if (angry) {
            eyelidsAngryHeightNext = eyeLheightCurrent/2; 
            eyelidsTiredHeightNext = 0;
            eyelidsAngryHeight = (eyelidsAngryHeight + eyelidsAngryHeightNext)/2;
            if (!cyclops) {
                sprite.fillTriangle(eyeLx, eyeLy-1, eyeLx+eyeLwidthCurrent, eyeLy-1, eyeLx+eyeLwidthCurrent, eyeLy+eyelidsAngryHeight-1, BGCOLOR);
                sprite.fillTriangle(eyeRx, eyeRy-1, eyeRx+eyeRwidthCurrent, eyeRy-1, eyeRx, eyeRy+eyelidsAngryHeight-1, BGCOLOR);
            } else {
                sprite.fillTriangle(eyeLx, eyeLy-1, eyeLx+(eyeLwidthCurrent/2), eyeLy-1, eyeLx+(eyeLwidthCurrent/2), eyeLy+eyelidsAngryHeight-1, BGCOLOR);
                sprite.fillTriangle(eyeLx+(eyeLwidthCurrent/2), eyeLy-1, eyeLx+eyeLwidthCurrent, eyeLy-1, eyeLx+(eyeLwidthCurrent/2), eyeLy+eyelidsAngryHeight-1, BGCOLOR);
            }
        } else {
            eyelidsAngryHeightNext = 0;
        }
        
        if (happy) {
            eyelidsHappyBottomOffsetNext = eyeLheightCurrent/2;
            eyelidsHappyBottomOffset = (eyelidsHappyBottomOffset + eyelidsHappyBottomOffsetNext)/2;
            sprite.fillRoundRect(eyeLx-1, (eyeLy+eyeLheightCurrent)-eyelidsHappyBottomOffset+1, eyeLwidthCurrent+2, eyeLheightDefault, eyeLborderRadiusCurrent, BGCOLOR);
            if (!cyclops) {
                sprite.fillRoundRect(eyeRx-1, (eyeRy+eyeRheightCurrent)-eyelidsHappyBottomOffset+1, eyeRwidthCurrent+2, eyeRheightDefault, eyeRborderRadiusCurrent, BGCOLOR);
            }
        } else {
            eyelidsHappyBottomOffsetNext = 0;
        }
        drawTime();
        if (displayText.length() > 0) {
            drawWrappedText();
        }
    }
};

#endif
