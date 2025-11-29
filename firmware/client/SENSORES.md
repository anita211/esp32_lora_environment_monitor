# 📡 Guia de Conexão e Calibração dos Sensores

## 🔌 Conexões Físicas

**Resumo Rápido:**

| Sensor | Pino Sensor | ESP32-S3 Pin | GPIO | Tensão |
|--------|-------------|--------------|------|--------|
| HC-SR04 | VCC | 5V | - | 5V |
| HC-SR04 | TRIG | D0 | GPIO1 | 3.3V |
| HC-SR04 | ECHO | D1 | GPIO2 | 5V→3.3V ⚠️ |
| HC-SR04 | GND | GND | - | - |
| MH-RD | VCC | 3.3V | - | 3.3V ✓ |
| MH-RD | AO | A2 | GPIO3 | 3.3V |
| MH-RD | GND | GND | - | - |

### 1️⃣ **HC-SR04 (Sensor Ultrassônico de Distância)**

```
HC-SR04          XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│    5V      │
│    TRIG    │───│ D0 (GPIO1) │
│    ECHO    │───│ D1 (GPIO2) │ ⚠️ Usar divisor de tensão!
│    GND     │───│    GND     │
└────────────┘   └────────────┘
```

**⚠️ IMPORTANTE - Divisor de Tensão para ECHO:**
O HC-SR04 retorna 5V no pino ECHO, mas o ESP32-S3 aceita apenas 3.3V!

**Opção 1: Divisor de Tensão (Recomendado)**
```
ECHO (5V) ──[ R1: 1kΩ ]──┬── D1 (GPIO2)
                          │
                     [ R2: 2kΩ ]
                          │
                         GND

Tensão no GPIO2 = 5V × (2kΩ / 3kΩ) = 3.33V ✓
```

**Opção 2: Conversor de Nível Lógico (Mais Seguro)**
- Use um módulo conversor bidirecional 5V ↔ 3.3V

---

### MH-RD (Sensor de Umidade do Solo)

```
MH-RD Module     XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│   3.3V     │ ✓ Funciona com 3.3V
│    GND     │───│    GND     │
│ AO (Analog)│───│ A2 (GPIO3) │ ← ADC Pin
│ DO (Digital)│   │ (não usar) │
└────────────┘   └────────────┘
```

**Notas:**
- ✓ **VCC em 3.3V** - Seu sensor funciona com 3.3V!
- Use apenas a saída **AO (Analog Out)**
- A saída DO (Digital) não é necessária
- O sensor funciona de forma **invertida**: menor valor ADC = mais úmido
- Pino A2 (GPIO3) não conflita com HC-SR04

---

## 🔧 Calibração do Sensor MH-RD

O sensor MH-RD precisa ser **calibrado** para seu solo específico!

### Passo 1: Medir Valor "Seco"
```cpp
// No código, temporariamente:
#define MOISTURE_DRY_VALUE 4095  // Comece com esse valor
```

1. Deixe o sensor **completamente seco** ao ar livre por 30 minutos
2. Compile e rode o código
3. Anote o valor ADC mostrado no Serial Monitor
4. Atualize `MOISTURE_DRY_VALUE` com esse valor

### Passo 2: Medir Valor "Molhado"
```cpp
// No código:
#define MOISTURE_WET_VALUE 1500  // Ajuste esse valor
```

1. Coloque o sensor em **solo bem úmido** ou água
2. Anote o valor ADC mostrado
3. Atualize `MOISTURE_WET_VALUE` com esse valor

### Exemplo Real:
```
Sensor no ar (seco):  ADC = 4095  → 0% umidade
Sensor na água:       ADC = 800   → 100% umidade
Sensor em solo úmido: ADC = 1500  → ~60% umidade
```

**Atualize no `config.h`:**
```cpp
#define MOISTURE_DRY_VALUE 4095   // Seu valor seco
#define MOISTURE_WET_VALUE 800    // Seu valor molhado
```

---

## ⚙️ Configuração do Código

### Para Usar Sensores Reais:
No arquivo `include/config.h`, certifique-se que:
```cpp
#define USE_REAL_SENSORS true  // ← true para sensores reais
```

### Para Voltar à Simulação (Testes):
```cpp
#define USE_REAL_SENSORS false  // ← false para simulação
```

---

## 🧪 Testando os Sensores

### 1. Compile e Faça Upload:
```bash
cd firmware/client
platformio run --target upload
platformio device monitor
```

### 2. Verifique a Saída no Serial Monitor:

**Inicialização:**
```
Initializing sensors...
✓ HC-SR04 initialized (TRIG: GPIO1, ECHO: GPIO2)
✓ MH-RD Moisture sensor initialized (ADC: GPIO1)
Test readings: Moisture=45.2%, Distance=125.3cm
Sensors ready
```

**Durante Operação:**
```
--- Measurement Cycle ---
[SENSOR] MH-RD ADC: 2850, Humidity: 52.3%
[SENSOR] HC-SR04: 85.2 cm
Humidity: 52.30 %
Distance: 85.20 cm
Presence: DETECTED  ← Objeto < 100cm detectado!
```

---

## 🐛 Troubleshooting

### HC-SR04 Sempre Retorna 400cm (Out of Range)
✅ **Soluções:**
- Verifique as conexões TRIG e ECHO
- Use divisor de tensão no pino ECHO
- Certifique-se que o sensor tem alimentação de 5V
- Teste com objeto a ~30cm do sensor

### MH-RD Sempre Mostra 0% ou 100%
✅ **Soluções:**
- Recalibre os valores DRY e WET
- Verifique se o pino ADC está correto (A0 = GPIO1)
- Teste com o sensor em diferentes níveis de umidade

### "Out of range" no HC-SR04
✅ **Causa:** Nenhum objeto detectado em até 4 metros
- Coloque um objeto sólido a ~50cm do sensor

### Leituras Instáveis
✅ **Soluções:**
- Aumente `MOISTURE_SAMPLES` para 20 (mais lento, mais estável)
- Use fios curtos e blindados
- Adicione capacitor de 100µF entre VCC e GND do sensor

---

## 📊 Interpretação dos Dados

### Sensor de Umidade (MH-RD)
| ADC Value | Umidade % | Condição |
|-----------|-----------|----------|
| 4095 | 0% | Seco (ar) |
| 3000-3500 | 20-40% | Solo seco |
| 2000-2500 | 50-70% | Solo úmido |
| 800-1500 | 80-100% | Solo encharcado |

### Sensor Ultrassônico (HC-SR04)
| Distância | Presença | Uso |
|-----------|----------|-----|
| < 50cm | ✓ Detectado | Objeto muito próximo |
| 50-100cm | ✓ Detectado | Dentro do threshold |
| 100-200cm | ✗ Não detectado | Fora do threshold |
| > 200cm | ✗ Não detectado | Longe ou sem objeto |

---

## 🔋 Consumo de Energia

**Modo Ativo (com sensores):**
- ESP32-S3: ~240mA
- HC-SR04: ~15mA (durante medição, 2ms)
- MH-RD: ~5mA
- **Total: ~260mA durante ~3 segundos**

**Modo Deep Sleep:**
- ESP32-S3: ~10µA
- Sensores desligados automaticamente
- **Duração da bateria: semanas a meses**

---

## 📝 Checklist Antes de Ligar

- [ ] HC-SR04 tem **divisor de tensão** no ECHO
- [ ] MH-RD conectado no pino A0 (GPIO1)
- [ ] HC-SR04 alimentado com **5V**
- [ ] MH-RD alimentado com **3.3V**
- [ ] `USE_REAL_SENSORS` = `true` no config.h
- [ ] Valores DRY e WET calibrados
- [ ] Serial Monitor aberto para debug

---

## 🎯 Próximos Passos

1. **Teste Individual:** Teste cada sensor separadamente primeiro
2. **Calibração:** Faça a calibração do MH-RD no seu solo
3. **Integração:** Teste ambos os sensores juntos
4. **Deep Sleep:** Ative o deep sleep quando confirmar que tudo funciona
5. **Deploy:** Coloque em produção!

Boa sorte! 🚀
