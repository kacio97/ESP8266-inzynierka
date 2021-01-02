#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <cstdlib>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

//Do wyliczania wysokości
#define SEALEVELPRESSURE_HPA (1013.25)

//Czujnik BME
Adafruit_BME280 bme;

//Dane do połączenia WiFi
String soft_ssid = "esp8266";
String soft_passwd = "";

IPAddress soft_ip(192, 168, 1, 99);
IPAddress soft_msk(255, 255, 255, 0);

// ZMIENNE POMOCNICZE
String ssid_h = "";
String passwd_h = "";
String mqttServer_h = "";
String clientId_h = "ESP-8266-";
String isOn = "0,0,0,";
String isOn2 = "0,0,0,";

// Nazwy tematow do publikacji dla MQTT
const char *mqttTopicTemp = "temperatura";
const char *mqttTopicCis = "cisnienie";
const char *mqttTopicWilg = "wilgotnosc";

// Prototypy funkcji do obłsugi rządań HTTP
void handleRoot();
void handleConnection();
void handleNotFound();

//Prototypy funkcji dla setup()
void ServerHttpSetup();
void wiFiSetup();
void bme280Setup();
void mqttClientSetup();
void setSoftAP();

//Prototypy funkcji dla zapis/odczyt EEPROM
void saveDataToEEPROM();
void loadDataFromEEPROM();
void writeDataToEEPROM(int address, String str);
String readDataFromEEPROM(int address);

// inne prototypy funkcji
void reconnectMqtt();
void callback(char *topic, byte *message, unsigned int length);
void fillRGB(int, int, int);

// zmienne dla GPIO
int ledPin = 2;
int redPin = 14;
int greenPin = 12;
int bluePin = 13;

// Obiekty typu webserver który nasłuchuje na port 80
ESP8266WebServer server(80);

//Połączenie z serwerem MQTT (RBPI)/(WEB)
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup()
{
  // Ustawianie konfiguracji (raz na początku)

  //Uzywane do monitorowania (LOGI)
  Serial.begin(9600);
  EEPROM.begin(512);
  // pinMode(ledPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  delay(3000);

  setSoftAP();
  delay(2000);

  ServerHttpSetup();
  delay(2000);

  bme280Setup();
  delay(2000);

  loadDataFromEEPROM();

  wiFiSetup();
  delay(3000);

  // mqttClientSetup();
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      reconnectMqtt();
    }
  }

  // // Wykonuj w pętli
  client.loop();
  server.handleClient();

  if (isOn2 == "1020,1020,1020,")
  {
    fillRGB(1023, 1023, 1023);
  }

  Serial.println("Temperatura: " + (String)bme.readTemperature() + "°C");
  Serial.print("Ciśnienie: ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");
  Serial.println("Wilgotność: " + (String)bme.readHumidity() + "%");
  Serial.println("Wysokość: " + (String)bme.readAltitude(SEALEVELPRESSURE_HPA) + " mNpm");
  Serial.println("");
  Serial.println("IsON " + isOn);
  Serial.println("IsON 2 " + isOn2);
  String temperatura = (String)bme.readTemperature();
  temperatura += "°C";
  float prss = bme.readPressure() / 100.0F;
  String cisnienie = (String)prss;
  cisnienie += " hPa";
  String wilgotnosc = (String)bme.readHumidity();
  wilgotnosc += " %";

  if (client.connected())
  {
    client.publish(mqttTopicTemp, (char *)temperatura.c_str());
    client.publish(mqttTopicCis, (char *)cisnienie.c_str());
    client.publish(mqttTopicWilg, (char *)wilgotnosc.c_str());
    client.publish("ison", (char *)isOn2.c_str());
  }
  delay(5000);
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Dotarla wiadomosc na temat: ");
  Serial.print(topic);
  Serial.print(". Wiadomosc: ");
  String messageTemp;

  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  //Jeśli wiadomosc do nas dotrze na temat ledOutput sprawdzamy jaki kolor dostalismy i go ustawiamy wedlug informacji
  if (String(topic) == "ledOutput")
  {
    Serial.print("Zmiana wartosci na: ");
    int redColor = 0;
    int greenColor = 0;
    int blueColor = 0;
    int tmpIndex = 0;
    String tmpColor = "";
    isOn2.clear();

    for (unsigned int i = 0; i < messageTemp.length(); i++)
    {
      if (messageTemp[i] == ',')
      {

        // Serial.println("tmp color: " + tmpColor);

        if (tmpIndex == 0)
        {
          redColor = atoi(tmpColor.c_str());
          isOn2 += redColor;
          isOn2 += ",";
          tmpIndex++;
        }
        else if (tmpIndex == 1)
        {
          greenColor = atoi(tmpColor.c_str());
          isOn2 += greenColor;
          isOn2 += ",";
          tmpIndex++;
        }
        else if (tmpIndex == 2)
        {
          blueColor = atoi(tmpColor.c_str());
          isOn2 += blueColor;
          isOn2 += ",";
        }
        tmpColor.clear();
        continue;
      }
      tmpColor += messageTemp[i];
    }

    Serial.print(redColor);
    Serial.print(" ");
    Serial.print(greenColor);
    Serial.print(" ");
    Serial.print(blueColor);
    Serial.println();
    fillRGB(redColor, greenColor, blueColor);
  }

  if (String(topic) == "lightSwitch")
  {
    //DO zrobienia
  }
}

void reconnectMqtt()
{
  // Petla dopoki nie zostaniemy ponownie polaczeni
  while (!client.connected())
  {
    Serial.print("Próba nawiązania połączenia MQTT...");
    // Proba polaczenia
    if (client.connect(clientId_h.c_str()))
    {
      Serial.println("Połączono się z serwerem MQTT");
      // Subskrybuj temat
      // client.setCallback(callback);
      client.subscribe("ledOutput");
    }
    else
    {
      Serial.print("nieudane połączenie kod błądu: ");
      Serial.print(client.state());
      Serial.println(" ponowna próba za 10 sekund...");
      delay(10000);
    }
  }
}

void fillRGB(int redValue, int greenValue, int blueValue)
{
  isOn = "";
  if (redValue != 1020)
  {
    analogWrite(redPin, redValue);
  }
  else
  {
    analogWrite(redPin, 1023);
  }
  if (greenValue != 1020)
  {
    analogWrite(greenPin, greenValue);
  }
  else
  {
    analogWrite(greenPin, 1023);
  }
  if (blueValue != 1020)
  {
    analogWrite(bluePin, blueValue);
  }
  else
  {
    analogWrite(bluePin, 1023);
  }

  isOn += redValue;
  isOn += ",";
  isOn += greenValue;
  isOn += ",";
  isOn += blueValue;
  isOn += ",";
}

void ServerHttpSetup()
{

  if (MDNS.begin("esp8266"))
  { // Start the mDNS responder for esp8266.local
    Serial.println("Uruchomiono responder mDNS");
  }
  else
  {
    Serial.println("Błąd podczas konfigurowania odpowiedzi mDNS!");
  }

  //Zawolaj metode handleRoot kiedy klient zada danego URI
  server.on("/", handleRoot);
  //Zawolaj metode handleConnection kiedy wystapi zadanie POST dla danego URI
  server.on("/connect", handleConnection);
  // Zawolaj metode handleNotFound kiedy nie odnaleziono zadanego URI
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Uruchomiono serwer HTTP ");
}

void mqttClientSetup()
{
  mqttServer_h += WiFi.macAddress();
  client.setServer(mqttServer_h.c_str(), 1883);
  client.setCallback(callback);
  // client.setServer((char *)mqttServer_h.c_str(), 1883);
  if (!client.connected())
  {
    if (client.connect(clientId_h.c_str()))
    {
      Serial.println("Połączono się z serwerem MQTT");
      client.subscribe("ledOutput");
    }
    else
    {
      Serial.print("Nieudane połączenie z serwerem MQTT kod bledu: ");
      Serial.println(client.state());
    }
  }
}

void wiFiSetup()
{
  if (ssid_h != soft_ssid)
  {
    //POŁĄCZENIE WIFI
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_h, passwd_h);
    Serial.print("Próba nawiązania połączenia z ");
    Serial.println(ssid_h);
    int index = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.print(".");
      index++;
      if (index > 30)
      {
        break;
      }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print("Nawiązano połączenie z ");
      Serial.println(ssid_h);
      Serial.println("Adres IP: " + WiFi.localIP().toString());

      // wylacz punkt dostepowy esp8266
      WiFi.softAPdisconnect();
      delay(1000);
      mqttClientSetup();
    }
    else
    {
      Serial.print("Nieudane polaczenie z ");
      Serial.print(ssid_h);
    }
  }
}

void bme280Setup()
{
  //CZUJNIK
  if (bme.begin(0x76) == false)
  {
    Serial.println("Nie wykryto czujnika ");
    // digitalWrite(ledPin, LOW);
  }
  else
  {
    Serial.println("Połączono z czujnikiem BME280");
  }
}

void handleRoot()
{

  String htmlConfigSite = "<head><title>Konfiguracja modulu ESP</title></head>\
  <h4>Wypelnij wszystkie pola!</h4>\
    </br>\
    < form action='/connect' method='POST'>\
    <input type='text' name='ssid' placeholder='Nazwa Sieci*'>\
    </br>\
    <input type='password' name='password' placeholder='Haslo*'>\
    </br>\
    <input type='text' name='mqtt' placeholder='Adres serwera MQTT*'>\
    </br>\
    <input type = 'submit' value = 'Zapisz dane'>\
    </form>\
    <p><h4> Wstepna konfiguracja sieci i serwera MQTT </h4>\
    <h6></br></br>\
    <b> *Nazwa sieci</ b> : <i> nazwa sieci twojego routera wifi</i></br>\
    <b> *Haslo : </b><i> haslo do twojej sieci wifi</i></br>\
    <b> *Adres serwera MQTT : </b><i> Sprawdz na stronie WWW z ktorej korzystasz np.domoticzgoogleHome etc.\
    </br>lub jezeli uzywasz lokalnego borkera uzyj aresu IP przypisanego przez router</ i></ br>\
    </br><b><i> Uwaga wymagane sa pola oznaczone gwiazdka * </ i></b></h6></p> ";

  server.send(200, "text/html", htmlConfigSite);
}

void handleConnection()
{
  server.sendContent("<html><body>"
                     "<h3><p>Czy chcesz powrócić do <a href='/'>strony konfiguracyjnej ?</a>.</p></h3><br><br>"
                     "</body></html>");

  if (!server.hasArg("ssid") || !server.hasArg("password") || !server.hasArg("mqtt") || server.arg("ssid") == NULL || server.arg("password") == NULL || server.arg("mqtt") == NULL)
  {
    server.send(400, "text/plain", "400: Invalid Request"); // The request is invalid, so send HTTP status 400
    return;
  }
  else
  {
    ssid_h = server.arg("ssid");
    passwd_h = server.arg("password");
    mqttServer_h = server.arg("mqtt");
    // server.arg("ssid").toCharArray(ssid, ssid_h.length());
    // server.arg("password").toCharArray(passwd, passwd_h.length());
    // server.arg("mqtt").toCharArray(mqttServer, mqttServer_h.length());
    server.sendContent(ssid_h);
    server.sendContent("<br>");
    server.sendContent(passwd_h);
    server.sendContent("<br>");
    server.sendContent(mqttServer_h);
    Serial.println("odebrano informacje z formularza");
    Serial.print(ssid_h);
    // Serial.println(ssid.length());
    Serial.print(passwd_h);
    // Serial.println(passwd.length());
    Serial.print(mqttServer_h);
    // Serial.println(mqttServer.length());
    delay(2000);
    saveDataToEEPROM();
    // server.send(200, "text/html", "");
    server.sendContent("<h4>Zakonczono konfiguracje sieci</h4><br>Za chwile nastapi restart modulu ESP8266..");
    delay(5000);
    // server.client().stop();
    // ESP.restart();
  }
}

void handleNotFound()
{
  server.send(404, "text/plain", "404: Not found");
}

void saveDataToEEPROM()
{
  Serial.println("zapisuje dane do EEPROM");
  writeDataToEEPROM(0, ssid_h);
  Serial.println("");
  delay(2000);
  writeDataToEEPROM(ssid_h.length() + 1, passwd_h);
  Serial.println("");
  // EEPROM.put(0 + sizeof(ssid), passwd);
  delay(2000);
  writeDataToEEPROM(ssid_h.length() + passwd_h.length() + 2, mqttServer_h);
  Serial.println("");
  // EEPROM.put(0 + sizeof(ssid) + sizeof(passwd), mqttServer);
  delay(2000);
  Serial.println("Koniec zapisu do EEPROM");
  Serial.println("Restart esp8266 za 5 sekund");
  EEPROM.end();
  delay(5000);
  ESP.restart();
}

void writeDataToEEPROM(int address, String str)
{
  for (int i = address; i < str.length() + address; i++)
  {
    Serial.print(str[i - address]);
    EEPROM.put(i, str[i - address]);
    delay(20);
  }
  EEPROM.put(str.length() + address, '\0');
  EEPROM.commit();
}

void loadDataFromEEPROM()
{
  Serial.println("Odczyt danych z EEPROM");
  ssid_h = readDataFromEEPROM(0);
  // EEPROM.get(0, ssid);
  delay(2000);
  passwd_h = readDataFromEEPROM(0 + ssid_h.length() + 1);
  // EEPROM.get(0 + sizeof(ssid), passwd);
  delay(2000);
  mqttServer_h = readDataFromEEPROM(0 + ssid_h.length() + passwd_h.length() + 2);
  // EEPROM.get(0 + sizeof(ssid) + sizeof(passwd), mqttServer);
  delay(2000);
  // EEPROM.end();
  delay(2000);
  Serial.println("Przywrócono dane z pamięcie EEPROM");
  Serial.print("SSID: ");
  Serial.println(ssid_h);
  Serial.print("Haslo: ");
  Serial.println(passwd_h);
  Serial.print("Adres serwera MQTT: ");
  Serial.println(mqttServer_h);
}

String readDataFromEEPROM(int address)
{
  String str = "";
  char currentChar;
  int index = address;

  while (currentChar != '\0')
  {
    currentChar = char(EEPROM.read(index));
    index++;

    if (currentChar == '\0')
    {
      break;
    }
    else
    {
      str += currentChar;
    }
  }

  return str;
}

void setSoftAP()
{
  WiFi.softAPConfig(soft_ip, soft_ip, soft_msk);
  WiFi.softAP(soft_ssid, soft_passwd);
  delay(1000);
  Serial.print("Adres ip softAP: ");
  Serial.println(WiFi.softAPIP());
}
