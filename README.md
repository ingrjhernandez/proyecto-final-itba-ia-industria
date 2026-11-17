# Detección de estados de operación de un forzador de aire con ML y consulta conversacional con un agente de IA

Proyecto Final Integrador (Ruta A: prototipo funcional) · ITBA Educación Ejecutiva · IA Aplicada a la Industria.

- **Comisión 15:** Rodrigo Juan Hernández, Nicola Petetta, Rafael Loré Truninger, Patricio Proasi
- **Docentes:** Agustín Moro, Facundo Grillo Buide
- **Entrega:** 19 de noviembre de 2026

## Descripción

Un ventilador manual a baterías emula un forzador de aire. Una placa **Arduino Nano 33 BLE Sense Lite**, fijada al ventilador, clasifica con ML embebido su estado de operación en cuatro clases:

| Clase (etiqueta en Edge Impulse) | Significado |
|---|---|
| `apagado` | Equipo detenido |
| `velocidadminima` | Velocidad mínima |
| `velocidadmaxima` | Velocidad máxima |
| `velocidadmaximaobstruida` | Velocidad máxima con obstrucción |

El resultado se publica en **TagoIO** y puede consultarse, junto con su historial, en lenguaje natural por **Telegram** mediante un agente de IA en **n8n** que accede a TagoIO en solo lectura a través de su servidor MCP.

## Arquitectura

```
Ventilador + Arduino Nano 33 BLE Sense Lite
   │  IMU + magnetómetro (9 ejes, 100 Hz) → inferencia en la placa (Edge Impulse, int8)
   ▼
Puerto serie
   ▼
Script Python ──MQTT (1 vez/min)──► TagoIO
                                      ▲
                                      │ MCP (read_device_data, solo lectura)
Telegram ◄──► n8n (agente IA · Gemini Flash · memoria de 10 mensajes)
```

1. **Adquisición e inferencia:** la placa muestrea acelerómetro, giroscopio y magnetómetro (X, Y, Z) a 100 Hz y clasifica ventanas de 2 s.
2. **Puente a la nube:** la placa no tiene WiFi; publica la inferencia por serie y un script Python la envía a TagoIO por MQTT una vez por minuto (para reducir costos).
3. **Agente conversacional (n8n):** Telegram Trigger → filtro de chat autorizado → `sendChatAction` → agente de IA → respuesta por Telegram. La herramienta `leer_datos_tago` hace un HTTP Request JSON-RPC (`tools/call`) a `read_device_data` del servidor MCP de TagoIO (`mcp.ai.tago.io`) con Bearer token.
4. **Dispositivos en TagoIO:** `Machine01` (extrusora simulada, línea 3) y `MonitorForzador01` (forzador emulado).
5. **Formato de respuesta del agente:** máximo 5 líneas, texto plano, alertas primero, hora en HH:MM (UTC-3).

## Datos y modelo

- **Muestras:** ~10 s por clase, 20 de entrenamiento y 10 de prueba por clase (80 y 40 en total).
- **Ventanas de 2 s sin solapamiento:** 320 de entrenamiento y 160 de prueba. Entrenamiento total: 12 min 45 s.
- **Modelo:** Edge Impulse con bloque *Spectral features* (wavelet) y red densa de 5 capas (entrada y salida incluidas). Cuantizado a int8, exportado como librería Arduino y compilado con PlatformIO.

### Resultados sobre el conjunto de prueba (modelo float32)

| Métrica | Valor |
|---|---|
| Exactitud | 99,38 % (159 de 160 ventanas) |
| ROC AUC | 1,00 |
| Precisión / recall / F1 ponderados | 0,99 |
| Obstruido | precisión 1,00 · recall 1,00 |
| Apagado | precisión 0,98 · recall 1,00 |
| Velocidad mínima | precisión 1,00 · recall 0,98 |

El único error es una ventana de velocidad mínima clasificada como apagado.

### Recursos en la placa (int8, EON)

| Concepto | Tiempo | RAM pico | Flash |
|---|---|---|---|
| Procesamiento de señal | 41 ms | 10 KB | — |
| Inferencia de red | 1 ms | 1,6 KB | 24,9 KB |
| **Total por ventana de 2 s** | **~42 ms** | | |

## Herramientas

Arduino Nano 33 BLE Sense Lite · Edge Impulse · PlatformIO · Python · TagoIO (y su servidor MCP) · n8n · Telegram Bot API · Gemini.

## Estructura del repositorio

> Completar según la organización final del código.

```
firmware/     # Código de la placa (PlatformIO) y librería exportada de Edge Impulse
bridge/       # Script Python serie → MQTT → TagoIO
n8n/          # Export JSON del workflow del agente y prompt de sistema
data/         # Detalle del dataset y protocolo de captura
docs/         # Ejemplo de payload a TagoIO y documentación adicional
```

## Puesta en marcha

1. **Firmware:** compilar y cargar el proyecto en la placa con PlatformIO.
2. **Puente:** configurar las variables de entorno del script Python (puerto serie, credenciales MQTT y token de dispositivo de TagoIO) y ejecutarlo con la placa conectada.
3. **Agente:** importar el workflow de n8n y configurar las credenciales de Telegram, Gemini y el token de TagoIO, además del chat de Telegram autorizado.

> Las credenciales y tokens **no** deben subirse al repositorio; usar variables de entorno o el gestor de credenciales de n8n.

## Alcance

- **Incluye:** un forzador, 4 estados, prototipo en banco y consulta de solo lectura desde Telegram con n8n, LLM y conector MCP.
- **No incluye:** actuación sobre el equipo, predicción de vida útil remanente, flota de múltiples equipos ni integración con sistemas de mantenimiento.

## Limitaciones

- Datos de ambiente controlado, secuenciales y de una sola máquina; la mayoría de las muestras provienen de una misma sesión continua.
- La exactitud de la placa (int8) no se midió directamente; las métricas corresponden al modelo float32.
- El agente es una experiencia de laboratorio, sin guardrails ni protección contra *prompt injection*.


