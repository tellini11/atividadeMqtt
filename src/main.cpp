#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Botao.h>
#include <HTTPClient.h>

void aoReceberMsg(char *, byte *, unsigned int);
void tratarJson(String);
void apiClima();
void reconectarMqtt();

LiquidCrystal_I2C lcd(0x27, 20, 4);

const char *SSID = "SENAI EDUCACIONAL";
const char *SENHA = "info@134";

const char *BROKER = "broker.emqx.io";
const int PORTA = 1883;

const char *TOPICO_PUB = "senai134/esp32/luy/status";
const char *TOPICO_SUB = "senai134/esp32/guilherme/status";

const int PINO_LED_RGB = 48;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Botao botao(14); 
bool estadoAtualLed = false;

String idDispositivo;

unsigned long tempoApi = 0;
const unsigned long intervaloApi = 60000;

unsigned long tempoReconexao = 0;
const unsigned long intervaloReconexao = 5000;

void setup()
{
    Serial.begin(115200);

    botao.iniciar();

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Led Gui: DESLIGADO");
    lcd.setCursor(0, 1);
    lcd.print("Led Luy: DESLIGADO");

    Serial.print("Conectando ao WiFi...");
    WiFi.begin(SSID, SENHA);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.print("\nWiFi conectado com sucesso!\n\r");

    idDispositivo = "esp32-luy-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.printf("ID do dispositivo: %s\n\r", idDispositivo.c_str());

    mqtt.setServer(BROKER, PORTA);
    mqtt.setCallback(aoReceberMsg);

    reconectarMqtt();
    apiClima();
}

void loop()
{
    if (!mqtt.connected())
    {
        if (millis() - tempoReconexao >= intervaloReconexao)
        {
            tempoReconexao = millis();
            reconectarMqtt();
        }
    }
    mqtt.loop();
    botao.atualizar();

    if (millis() - tempoApi >= intervaloApi)
    {
        tempoApi = millis();
        apiClima();
    }

    if (botao.pressionou())
    {
        estadoAtualLed = !estadoAtualLed;

        JsonDocument doc;
        doc["estadoLed"] = estadoAtualLed;

        String mensagem;
        serializeJson(doc, mensagem);

        if (mqtt.connected())
        {
            mqtt.publish(TOPICO_PUB, mensagem.c_str());
            Serial.printf("Publicado em %s: %s\n\r", TOPICO_PUB, mensagem.c_str());
        }
        else
        {
            Serial.println("MQTT desconectado, mensagem não enviada");
        }

        lcd.setCursor(0, 0);
        lcd.print("Led Gui: ");
        lcd.print(estadoAtualLed ? "LIGADO   " : "DESLIGADO");
    }
}

void reconectarMqtt()
{
    Serial.printf("Conectando ao Broker %s...\n\r", BROKER);

    if (mqtt.connect(idDispositivo.c_str()))
    {
        Serial.printf("Conectado ao Broker %s com sucesso!\n\r", BROKER);
        mqtt.subscribe(TOPICO_SUB);
        Serial.printf("Inscrito no tópico: %s\n\r", TOPICO_SUB);
    }
    else
    {
        Serial.printf("Falha ao conectar (codigo %d). Nova tentativa em 5s\n\r", mqtt.state());
    }
}

void aoReceberMsg(char *topico, byte *payload, unsigned int tamanho)
{
    String mensagem;
    mensagem.reserve(tamanho);

    for (unsigned int i = 0; i < tamanho; i++)
    {
        mensagem += (char)payload[i];
    }

    Serial.printf("Recebido em %s: %s\n\r", topico, mensagem.c_str());
    tratarJson(mensagem);
}

void tratarJson(String mensagem)
{
    JsonDocument doc;
    DeserializationError codigoErro = deserializeJson(doc, mensagem);

    switch (codigoErro.code())
    {
    case DeserializationError::Ok:
        break;

    case DeserializationError::InvalidInput:
        Serial.println("JSON com sintaxe inválida");
        return;

    case DeserializationError::EmptyInput:
        Serial.println("JSON vazio ou expirou o tempo");
        return;

    case DeserializationError::IncompleteInput:
        Serial.println("JSON incompleto");
        return;

    case DeserializationError::NoMemory:
        Serial.println("Sem memória");
        return;

    case DeserializationError::TooDeep:
        Serial.println("Aninhamento acima do limite definido pela função");
        return;

    default:
        Serial.println(codigoErro.c_str());
        return;
    }

    bool estadoLed = doc["estadoLed"] | false;

    lcd.setCursor(0, 1);
    lcd.print("Led Luy: ");

    if (estadoLed)
    {
        rgbLedWrite(PINO_LED_RGB, 0, 255, 0);
        lcd.print("LIGADO   ");
    }
    else
    {
        rgbLedWrite(PINO_LED_RGB, 0, 0, 0);
        lcd.print("DESLIGADO");
    }
}

void apiClima()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    HTTPClient http;
    http.setTimeout(3000);
    http.begin("https://api.open-meteo.com/v1/forecast?latitude=-23.55&longitude=-46.63&current=temperature_2m");

    int codigoHTTP = http.GET();
    if (codigoHTTP == 200)
    {
        JsonDocument docAPI;
        DeserializationError erro = deserializeJson(docAPI, http.getString());

        if (!erro)
        {
            float temperatura = docAPI["current"]["temperature_2m"].as<float>();

            lcd.setCursor(0, 3);
            lcd.print("Temp SP: ");
            lcd.print(temperatura, 1);
            lcd.print(" C   ");
        }
        else
        {
            Serial.println("Erro ao ler JSON da API de clima");
        }
    }
    else
    {
        Serial.printf("Falha na API de clima, codigo HTTP: %d\n\r", codigoHTTP);
    }
    http.end();
}