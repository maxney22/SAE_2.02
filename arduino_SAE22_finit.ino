#include <VS1053.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ESP32_VS1053_Stream.h>
#include <PubSubClient.h>


// broches utilisées
#define VS1053_CS     32
#define VS1053_DCS    33
#define VS1053_DREQ   15
#define SCI_MODE 0x00  // registre SCI_MODE
#define SM_EARSPEAKER_LO 0x0010  // bit 4
#define SM_EARSPEAKER_HI 0x0080  // bit 7


WiFiClient mqttWifiClient;                                    
PubSubClient mqttClient(mqttWifiClient);

const char* mqtt_server = "test.mosquitto.org";  

ESP32_VS1053_Stream radio;
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);



const char* urls[] = {
  "http://radios.rtbf.be/wr-c21-metal-128.mp3",
  "https://scdn.nrjaudio.fm/adwz2/fr/30601/mp3_128.mp3?origine=fluxradios",
  "https://stream.funradio.fr/fun-1-48-128.mp3",
  "https://scdn.nrjaudio.fm/adwz2/fr/30001/mp3_128.mp3?origine=fluxradios",
  "http://ecoutez.chyz.ca:8000/mp3",
  "http://ice4.somafm.com/seventies-128-mp3",
  "http://lyon1ere.ice.infomaniak.ch/lyon1ere-high.mp3"
};
const int NOMBRECHAINES = sizeof(urls) / sizeof(urls[0]);
int chaine = 0;
// nom et mot de passe de votre réseau:
//const char *ssid = "maxime";
//const char *password = "telmaxime";

#define BUFFSIZE 64  //32, 64 ou 128
uint8_t mp3buff[BUFFSIZE];

int volume = 85;  // volume sonore 0 à 100
uint8_t rtone[] = {0x00, 0x00, 0x00, 0x00};
// Indices :   [ 0   ,    1    ,   2  ,    3    ]
// Signifie : [Treble, TrebleFreq, BassAmp, BassFreq]

//caractéristiques de la station actuellement sélectionnée
char host[40];
char path[40];
int httpPort;


WiFiClient client;

// connexion à une chaine



void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFiManager wm;

  if (!wm.autoConnect("TestESP32", "12345678")) {
    Serial.println("Échec");
    ESP.restart();
  }

  Serial.println("Connecté !");
  Serial.println(WiFi.localIP());

 
  Serial.println("\n\nRadio WiFi");
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //Serial.println("WiFi connecte");
  //Serial.println("Adresse IP: ");
  //Serial.println(WiFi.localIP());

  SPI.begin();

  player.begin();
  player.switchToMp3Mode();
  player.setVolume(volume);
  player.setTone(rtone);
  
  radio.startDecoder(VS1053_CS, VS1053_DCS, VS1053_DREQ);
  Serial.print("Connexion à l'URL : ");
  Serial.println(urls[chaine]);

  if (!radio.connecttohost(urls[chaine])) {
    Serial.println("Erreur de connexion au flux !");
  } else {
    Serial.println("Connexion réussie !");
  }

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);

  mqttClient.subscribe("radio/chainePlus");
  mqttClient.subscribe("radio/chaineMoins");
  mqttClient.subscribe("radio/volplus");
  mqttClient.subscribe("radio/volmoins");
  mqttClient.subscribe("radio/bassplus");
  mqttClient.subscribe("radio/bassmoins");
  mqttClient.subscribe("radio/aiguplus");
  mqttClient.subscribe("radio/aigumoins");
  mqttClient.subscribe("radio/spa0");
  mqttClient.subscribe("radio/spa1");
  mqttClient.subscribe("radio/spa2");
  mqttClient.subscribe("radio/spa3");
  mqttClient.subscribe("radio/url");
  mqttClient.subscribe("radio/revenir");
}

void setSpatialisationMode(int mode) {
  uint16_t value = player.read_register(0x00); // SCI_MODE

  // Efface bits 4 (LO) et 7 (HI)
  value &= ~( (1 << 4) | (1 << 7) );

  // Active bits selon le mode
  if (mode == 1) value |= (1 << 4);              // minimal
  else if (mode == 2) value |= (1 << 7);         // normal
  else if (mode == 3) value |= (1 << 4) | (1 << 7); // extrême

  player.writeRegister(0x00, value);

  Serial.print("Spatialisation mode ");
  Serial.println(mode);
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("MQTT message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "radio/chainePlus") {
    if (message == "1") {
      chaine = (chaine + 1) % NOMBRECHAINES;
      Serial.println("Chaîne suivante (chainePlus)");
      radio.stopSong();
      radio.connecttohost(urls[chaine]);
    }
  }
  else if (String(topic) == "radio/chaineMoins") {
    if (message == "1") {
      chaine = (chaine - 1 + NOMBRECHAINES) % NOMBRECHAINES;
      Serial.println("Chaîne précédente (chaineMoins)");
      radio.stopSong();
      radio.connecttohost(urls[chaine]);
    }
  }
  else if (String(topic) == "radio/volplus") {
    if (message == "1") {
      if (volume < 100) {
        Serial.println("Plus fort");
        volume++;
        player.setVolume(volume);
      }
    }
  }
  else if (String(topic) == "radio/volmoins") {
    if (message == "1") {
      if (volume > 0) {
        Serial.println("Moins fort");
        volume--;
        player.setVolume(volume);
      }
    }
  }
  else if (String(topic) == "radio/bassplus") {
    if (message == "1") {
      if (rtone[2] < 15) {
        Serial.println("basses +");
        rtone[2]++;
        player.setTone(rtone);
      }
    }
  }
  else if (String(topic) == "radio/bassmoins") {
    if (message == "1") {
      if (rtone[2] > 0) {
        Serial.println("basses -");
        rtone[2]--;
        player.setTone(rtone);
      }
    }
  }
  else if (String(topic) == "radio/aiguplus") {
    if (message == "1") {
      int8_t current = (int8_t)rtone[0];
      if (current < 7) {
        rtone[0] = current + 1;
        player.setTone(rtone);
        Serial.print("Aigus augmentés: ");
      } else {
        Serial.println("Aigus déjà au maximum (7)");
      }
    }
  }
  else if (String(topic) == "radio/aigumoins") {
    if (message == "1") {
      int8_t current = (int8_t)rtone[0]; // Conversion explicite en signé
      if (current > -8) {
        rtone[0] = current - 1;
        player.setTone(rtone);
        Serial.print("Aigus diminués: ");
      } else {
        Serial.println("Aigus déjà au minimum (-8)");
      }
    }
  }

  else if (String(topic) == "radio/spa0") {
    if (message == "1") {
      setSpatialisationMode(0);
    }
  }
   else if (String(topic) == "radio/spa1") {
    if (message == "1") {
      setSpatialisationMode(1);
    }
  }
   else if (String(topic) == "radio/spa2") {
    if (message == "1") {
      setSpatialisationMode(2);
    }
  }
   else if (String(topic) == "radio/spa3") {
    if (message == "1") {
      setSpatialisationMode(3);
    }
  }

  else if (String(topic) == "radio/url") {
  String url = message;
  if(url.length() > 0) {
    Serial.print("Tentative de lecture de l'URL: ");
    Serial.println(url);
    
    radio.stopSong();
    if(radio.connecttohost(url.c_str())) {
      Serial.println("Lecture démarrée !");
      mqttClient.publish("radio/status", "URL en lecture");
    } else {
      Serial.println("Échec de connexion au flux");
      mqttClient.publish("radio/status", "Erreur de connexion");
      }  
    }
  }
  else if (String(topic) == "radio/revenir") {
    if (message == "1") {
      radio.stopSong();
      radio.connecttohost(urls[chaine]);
      Serial.println("connexion au urls par defauts reussi");
    }
  }
      
    
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connexion MQTT...");
    if (mqttClient.connect("ESP32RadioClient")) {
      Serial.println("Connecté !");
      mqttClient.subscribe("radio/chainePlus");
      mqttClient.subscribe("radio/chaineMoins");
      mqttClient.subscribe("radio/volplus");
      mqttClient.subscribe("radio/volmoins");
      mqttClient.subscribe("radio/bassplus");
      mqttClient.subscribe("radio/bassmoins");
      mqttClient.subscribe("radio/aiguplus");
      mqttClient.subscribe("radio/aigumoins");
      mqttClient.subscribe("radio/spa0");
      mqttClient.subscribe("radio/spa1");
      mqttClient.subscribe("radio/spa2");
      mqttClient.subscribe("radio/spa3");
      mqttClient.subscribe("radio/url");
      mqttClient.subscribe("radio/revenir");
      Serial.println("Réabonnement aux topics effectué");
    } else {
      Serial.print("Échec, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Nouvelle tentative dans 5 secondes...");
      delay(5000);
    }
  }
}



void loop() {

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();


  radio.loop();
  if (Serial.available()) {
    char c = Serial.read();

    // n: prochaine chaine
    if (c == 'n' || c == 'v') {
      chaine = (chaine + 1) % NOMBRECHAINES;
      Serial.println("Changement de chaine...");
      radio.stopSong();

      Serial.print("Connexion à l'URL : ");
      Serial.println(urls[chaine]);

      if (!radio.connecttohost(urls[chaine])) {
        Serial.println("Erreur de connexion au flux !");
      } else {
        Serial.println("Connexion réussie !");
      }
    }


    // y : augmenter le volume
    if (c == 'y') {
      if (volume < 100) {
        Serial.println("Plus fort");
        volume++;
        player.setVolume(volume);
      }
    }

    // b : diminuer le volume
    if (c == 'b') {
      if (volume > 0) {
        Serial.println("Moins fort");
        volume--;
        player.setVolume(volume);
      }
    }


 // g[+] : augmenter les basses
    if (c == 'g') {
      if (rtone[2] < 15) {
        Serial.println("basses +");
        rtone[2]++;
        player.setTone(rtone);
      }
    }

// f[-]  : diminuer les basses
    if (c == 'f') {
      if (rtone[2] > 0) {
        Serial.println("basses -");
        rtone[2]--;
        player.setTone(rtone);
      }
    }

// a[+] : augmenter les aigus
    if (c == 'j') {
      int8_t current = (int8_t)rtone[0]; // Conversion explicite en signé
      if (current < 7) {
        rtone[0] = current + 1;
        player.setTone(rtone);
        Serial.print("Aigus augmentés: ");
      } else {
        Serial.println("Aigus déjà au maximum (7)");
      }
    }

// h[-]  : diminuer les aigus
    if (c == 'h') {
      int8_t current = (int8_t)rtone[0]; // Conversion explicite en signé
      if (current > -8) {
        rtone[0] = current - 1;
        player.setTone(rtone);
        Serial.print("Aigus diminués: ");
      } else {
        Serial.println("Aigus déjà au minimum (-8)");
      }
    }

//tonalité par défaut
  if (c == 'd') {
    Serial.println("tonalité par défaut");
    rtone[2] = 0x00;
    rtone[0] = 0x00;
    player.setTone(rtone);
  }

//spatialisation
  if ( c == '0') setSpatialisationMode(0);
  if ( c == '1') setSpatialisationMode(1);
  if ( c == '2') setSpatialisationMode(2);
  if ( c == '3') setSpatialisationMode(3);
  }

  if (client.available() > 0) {
    uint8_t bytesread = client.read(mp3buff, BUFFSIZE);
    if (bytesread) {
      player.playChunk(mp3buff, bytesread);
    }
  }
}