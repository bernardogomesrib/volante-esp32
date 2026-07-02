#include <SPI.h>
#include <Wire.h>
#include <BleGamepad.h> 

BleGamepad bleGamepad("Volante ESP32 Wireless", "Custom", 100);

// --- PINOS SPI (Botões do Aro) ---
#define PIN_CS   5
#define PIN_MISO 19
#define PIN_MOSI 23
#define PIN_SCLK 18

// --- ENDEREÇO I2C DO SENSOR AS5600 ---
#define AS5600_ADDRESS 0x36

// --- CONFIGURAÇÃO DA JANELA DE CENTRO ANTI-DRIFT ---
const int CENTRO_MIN = 2340; 
const int CENTRO_MAX = 2380; 

SPISettings ps2HardwareSPI(250000, LSBFIRST, SPI_MODE3);

// --- VARIÁVEIS COMPARTILHADAS ENTRE AS THREADS ---
volatile int16_t dadoEixoX = 0;
volatile byte dadoB1 = 0xFF;
volatile byte dadoB2 = 0xFF;
volatile bool dadosValidos = false;

// Variáveis para monitoramento no Serial do Core 1
byte b1_antigo = 0xFF;
byte b2_antigo = 0xFF;
int16_t eixoX_antigo = -9999;
byte modo_antigo = 0x00;

// Variáveis para a desconexão no Modo 3
unsigned long tempoBotaoStart = 0;
bool botaoStartSegurado = false;

// Função para ler a posição angular do AS5600 via I2C
int lerPosicaoAS5600() {
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(0x0C); 
  if (Wire.endTransmission() != 0) return 2360; 

  Wire.requestFrom(AS5600_ADDRESS, 2);
  if (Wire.available() >= 2) {
    int alta = Wire.read();
    int baixa = Wire.read();
    return (alta << 8) | baixa; 
  }
  return 2360;
}

// =========================================================================
// THREAD PARALELA: Roda exclusivamente no CORE 0 dedicada ao Bluetooth
// =========================================================================
void TaskBluetooth(void *pvParameters) {
  Serial.print("Thread do Bluetooth iniciada no Core: ");
  Serial.println(xPortGetCoreID());

  for (;;) { // Loop infinito desta thread
    if (bleGamepad.isConnected() && dadosValidos) {
      // Pega uma cópia rápida dos dados da outra thread
      int16_t x = dadoEixoX;
      byte b1 = dadoB1;
      byte b2 = dadoB2;

      bleGamepad.setX(x);

      // Mapeamento dos botões
      if (!(b1 & (1 << 3))) bleGamepad.press(BUTTON_1);  else bleGamepad.release(BUTTON_1);  // START
      if (!(b1 & (1 << 0))) bleGamepad.press(BUTTON_2);  else bleGamepad.release(BUTTON_2);  // SELECT
      if (!(b2 & (1 << 7))) bleGamepad.press(BUTTON_3);  else bleGamepad.release(BUTTON_3);  // QUADRADO
      if (!(b2 & (1 << 6))) bleGamepad.press(BUTTON_4);  else bleGamepad.release(BUTTON_4);  // X
      if (!(b2 & (1 << 5))) bleGamepad.press(BUTTON_5);  else bleGamepad.release(BUTTON_5);  // CIRCULO
      if (!(b2 & (1 << 4))) bleGamepad.press(BUTTON_6);  else bleGamepad.release(BUTTON_6);  // TRIANGULO
      if (!(b2 & (1 << 3))) bleGamepad.press(BUTTON_7);  else bleGamepad.release(BUTTON_7);  // L1
      if (!(b2 & (1 << 2))) bleGamepad.press(BUTTON_8);  else bleGamepad.release(BUTTON_8);  // R1
      if (!(b2 & (1 << 1))) bleGamepad.press(BUTTON_9);  else bleGamepad.release(BUTTON_9);  // L2
      if (!(b2 & (1 << 0))) bleGamepad.press(BUTTON_10); else bleGamepad.release(BUTTON_10); // R2

      // Mapeamento do D-Pad
      if (!(b1 & (1 << 4)))      bleGamepad.setHat1(HAT_UP);
      else if (!(b1 & (1 << 5))) bleGamepad.setHat1(HAT_RIGHT);
      else if (!(b1 & (1 << 6))) bleGamepad.setHat1(HAT_DOWN);
      else if (!(b1 & (1 << 7))) bleGamepad.setHat1(HAT_LEFT);
      else                       bleGamepad.setHat1(DPAD_CENTERED);

      bleGamepad.sendReport();
    }
    // Delay do FreeRTOS (5ms) para não travar o processador do Core 0
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_MISO, INPUT_PULLUP); 
  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS);

  Wire.begin(); 
  bleGamepad.begin();

  // CRIAÇÃO DA THREAD NO CORE 0
  xTaskCreatePinnedToCore(
    TaskBluetooth,    // Função que gertencia a thread
    "TaskBT",         // Nome interno da tarefa
    4096,             // Tamanho da memória (Stack size)
    NULL,             // Parâmetros de entrada
    1,                // Prioridade da tarefa
    NULL,             // Identificador da tarefa
    0                 // <--- FORÇA RODAR NO CORE 0
  );

  Serial.print("Loop principal rodando no Core: ");
  Serial.println(xPortGetCoreID());
}

// =========================================================================
// LOOP PRINCIPAL: Roda exclusivamente no CORE 1 focado em ler os sensores
// =========================================================================
void loop() {
  // 1. Leitura rápida do AS5600
  int valorVolante = lerPosicaoAS5600(); 
  int16_t eixoX = 0;

  if (valorVolante >= CENTRO_MIN && valorVolante <= CENTRO_MAX) {
    eixoX = 0; 
  } else if (valorVolante < CENTRO_MIN) {
    eixoX = map(valorVolante, 0, CENTRO_MIN, -32767, 0);
  } else {
    eixoX = map(valorVolante, CENTRO_MAX, 4095, 0, 32767);
  }

  // 2. Leitura rápida do Aro via SPI (21 Bytes)
  byte dadosRecebidos[21] = {0};

  SPI.beginTransaction(ps2HardwareSPI);
  digitalWrite(PIN_CS, LOW);
  delayMicroseconds(10); // Reduzido o delay para máxima velocidade de amostragem

  dadosRecebidos[0] = SPI.transfer(0x01); delayMicroseconds(10);
  dadosRecebidos[1] = SPI.transfer(0x42); delayMicroseconds(10);
  dadosRecebidos[2] = SPI.transfer(0x00); delayMicroseconds(10);
  
  for (int i = 3; i < 21; i++) {
    dadosRecebidos[i] = SPI.transfer(0x00);
    delayMicroseconds(10);
  }
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();

  if (dadosRecebidos[2] == 0x5A) {
    byte modoAtual = dadosRecebidos[1];
    byte b1 = dadosRecebidos[3];
    byte b2 = dadosRecebidos[4];

    // Atualiza as variáveis globais que a thread do Bluetooth (Core 0) vai ler
    dadoEixoX = eixoX;
    dadoB1 = b1;
    dadoB2 = b2;
    dadosValidos = true;

    // --- LÓGICA DE DESCONEXÃO NO MODO 3 ---
    if (modoAtual == 0x23 && !(b1 & (1 << 3))) { 
      if (!botaoStartSegurado) {
        tempoBotaoStart = millis();
        botaoStartSegurado = true;
      } else if (millis() - tempoBotaoStart >= 1500) {
        if (bleGamepad.isConnected()) {
          Serial.println("!!! FORÇANDO DESCONEXÃO BLUETOOTH !!!");
          bleGamepad.end();
          delay(300);
          bleGamepad.begin();
        }
        botaoStartSegurado = false; 
      }
    } else {
      botaoStartSegurado = false; 
    }

    // --- MONITOR SERIAL CORRIGIDO: Printa absolutamente TODOS os botões agora ---
    if (b1 != b1_antigo || b2 != b2_antigo || modoAtual != modo_antigo || abs(eixoX - eixoX_antigo) > 150) {
      Serial.print("Modo: 0x"); Serial.print(modoAtual, HEX);
      Serial.print(" | Eixo X: "); Serial.print(eixoX);
      Serial.print(" | Botões: ");
      
      // Byte 1
      if (!(b1 & (1 << 3))) Serial.print("[START] ");
      if (!(b1 & (1 << 0))) Serial.print("[SELECT] ");
      if (!(b1 & (1 << 4))) Serial.print("[UP] ");
      if (!(b1 & (1 << 5))) Serial.print("[RIGHT] ");
      if (!(b1 & (1 << 6))) Serial.print("[DOWN] ");
      if (!(b1 & (1 << 7))) Serial.print("[LEFT] ");

      // Byte 2
      if (!(b2 & (1 << 7))) Serial.print("[QUADRADO] "); 
      if (!(b2 & (1 << 6))) Serial.print("[X] ");        
      if (!(b2 & (1 << 5))) Serial.print("[CIRCULO] ");
      if (!(b2 & (1 << 4))) Serial.print("[TRIANGULO] ");
      if (!(b2 & (1 << 3))) Serial.print("[L1] ");
      if (!(b2 & (1 << 2))) Serial.print("[R1] ");
      if (!(b2 & (1 << 1))) Serial.print("[L2] ");
      if (!(b2 & (1 << 0))) Serial.print("[R2] ");
      
      if (b1 == 0xFF && b2 == 0xFF) Serial.print("Nenhum");
      Serial.println();

      b1_antigo = b1; b2_antigo = b2; modo_antigo = modoAtual; eixoX_antigo = eixoX;
    }
  }
  // Apenas 1ms de delay de folga para o Core 1 não fritar. 
  // Taxa de amostragem dos botões agora está absurdamente instantânea!
  delay(1); 
}