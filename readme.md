# Volante e Aro de Corrida Custom - ESP32 Wireless Gamepad

Este projeto transforma um volante antigo/antropomórfico (com mais de 10 anos parado) em um controlador de simulação de corrida de alto desempenho para PC/Celular utilizando um **ESP32**. O projeto conta com leitura estável do barramento de botões original do aro via SPI e um sensor magnético **AS5600** via I2C para um posicionamento angular sem ruídos ou desgaste mecânico.

## 🚀 Funcionalidades

* **Conexão Nativa Bluetooth (HID):** Reconhecido diretamente pelo Windows (`joy.cpl`) como um Gamepad padrão, sem necessidade de softwares de terceiros rodando em background.
* **Arquitetura Dual-Core (FreeRTOS):** * **Core 1:** Dedicado exclusivamente à amostragem ultra-rápida dos sensores (SPI e I2C), garantindo latência zero e que nenhum clique rápido de botão seja perdido.
  * **Core 0:** Dedicado exclusivamente ao gerenciamento e envio dos pacotes via Bluetooth (BLE).
* **Filtro Anti-Drift com Janela Móvel:** Algoritmo customizado para absorver a histerese (folga mecânica de retorno) do volante, garantindo centro absoluto (`0`) estável no jogo.
* **Desconexão por Software:** Atalho integrado para derrubar a conexão Bluetooth atual e liberar o volante para emparelhamento em novos dispositivos sem precisar resetar a memória flash do chip.
* **Mapeamento Clássico:** Inclui direcionais (D-Pad), menus (`START`/`SELECT`), botões principais (`QUADRADO`, `X`, `CIRCULO`, `TRIANGULO`) e gatilhos traseiros (`L1`, `R1`, `L2`, `R2`).

## 🛠️ Hardware Utilizado

* **Microcontrolador:** ESP32 (Dual-Core, ex: ESP32-WROOM-32)
* **Sensor de Ângulo:** AS5600 (Magnético de 12 bits via I2C)
* **Aro/Volante:** Placa de botões original do aro (Protocolo PS2/SPI Slave)

### Pinagem de Conexão

| Componente | Pino Componente | Pino ESP32 | Função |
| :--- | :--- | :--- | :--- |
| **Aro (SPI)** | CS | **GPIO 5** | Chip Select |
| **Aro (SPI)** | MISO | **GPIO 19** | Master In Slave Out |
| **Aro (SPI)** | MOSI | **GPIO 23** | Master Out Slave In |
| **Aro (SPI)** | SCK | **GPIO 18** | Serial Clock |
| **AS5600** | SDA | **GPIO 21** | I2C Data |
| **AS5600** | SCL | **GPIO 22** | I2C Clock |
| **Energia** | VCC / 3V3 | **3V3** | Alimentação 3.3V |
| **Energia** | GND | **GND** | Terra Comum |

*Nota: Para alimentação final via painel USB customizado, os 5V do cabo de energia devem ser injetados no pino **VIN** do ESP32 e o negativo no **GND**.*

## 💻 Configuração do Código

### Dependências
Antes de compilar o código na Arduino IDE, instale a seguinte biblioteca através do gerenciador de bibliotecas:
* **ESP32-BLE-Gamepad** (por *lemmingDev*)

### Ajuste de Calibragem Mecânica
Se a folga física do retorno do seu volante parar em posições diferentes, ajuste os limites da janela móvel no topo do código principal:
```cpp
const int CENTRO_MIN = 2340; // Limite vindo da esquerda
const int CENTRO_MAX = 2380; // Limite vindo da direita