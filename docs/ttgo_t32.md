# TTGO T32 / WLED: transferencia técnica

Documento de handoff del fork que corre WLED en una LilyGO TTGO LoRa32
(T3 V1.6.1, SX1276). Describe el estado real del árbol, no una propuesta.
El contrato de los JSON que se pueden transmitir está en
`usermods/lora_rx/manual-json-lora.md`. Este archivo explica el firmware
que los recibe.

## Identidad del objetivo

No existe un board PlatformIO llamado TTGO T32. El hardware de este
proyecto es la LilyGO T3 LoRa32 V1.6.1:

| Dato | Valor |
|---|---|
| Entorno PlatformIO | `ttgo_t32` |
| Board | `ttgo-lora32-v21` |
| Chip medido en el flasheo | ESP32-PICO-D4, revisión v1.1, MAC `90:15:06:fc:5f:c4` |
| Flash / RAM | 4 MB / 320 KB, sin PSRAM |
| Partición | `${esp32.default_partitions}` (app de aproximadamente 1,5 MB) |
| Plataforma | tasmota `espressif32` 2026.05.50, Arduino-ESP32 3.3.8 |
| Radio | SX1276 a 915 MHz. No es el SX1278 de 433 MHz |
| Reset LoRa | GPIO 23. El RST 14 es de otra revisión y no se usa |

`platformio_override.ini` fija `default_envs = ttgo_t32`. Ese archivo está
en el `.gitignore` de WLED upstream y en este fork se versionó a propósito.
El entorno extiende `env:esp32dev`.

## Qué está enlazado

`[env:ttgo_t32]` en `platformio.ini`:

```
custom_usermods = ${common.default_usermods} lora_rx
```

`common.default_usermods` es `audioreactive`. El pin de audio está en `-1`
(`AUDIOPIN=-1`), así que el usermod entra en la imagen pero no tiene micrófono.
`usb_oled_test` existe en `usermods/usb_oled_test/` y no se enlaza. Fue el
banco USB para probar MessagePack antes de cerrar LoRa. No volver a meterlo
en `custom_usermods` de producción: lee `Serial` y la misma OLED.

Wi-Fi de WLED permanece. No hay `WLED_DISABLE_WIFI`. Conviven la estación,
el AP y la interfaz web con la tarea de recepción LoRa.

## Pines

Flags de `ttgo_t32` y valores por defecto de `usermods/lora_rx/lora_rx.cpp`.

| Función | GPIO | Dueño |
|---|---|---|
| Datos de la tira | 4 | `DATA_PINS=4`. El default de WLED en ESP32 clásico es el 16 y aquí pisa la OLED en otras revisiones |
| Botón | 0 | `BTNPIN=0` |
| Relé, IR, audio | -1 | desactivados |
| OLED SDA / SCL | 21 / 22 | `I2CSDAPIN` / `I2CSCLPIN`, bus HW_I2C de WLED. SSD1306 128x64 en `0x3C` |
| LoRa SCK / MISO / MOSI | 5 / 19 / 27 | SPI del SX1276. WLED los reserva como `HW_SPI` |
| LoRa CS | 18 | salida, `PinOwner::UM_LoRa` |
| LoRa RST | 23 | salida, `PinOwner::UM_LoRa` |
| LoRa DIO0 | 26 | entrada, IRQ de paquete recibido |
| LoRa DIO1 / DIO2 | 33 / 32 | entradas reservadas. RadioLib no las usa en RX básico. Están cableadas al SX1276 y no se reasignan |

No usar para LEDs ni para otro periférico: 5, 19, 27, 18, 23, 26, 33, 32, 21, 22.
Tampoco están libres en la placa, aunque este firmware no los bloquea en
`pinManager`: SD CS 13, SD MOSI 15, SD MISO 2, SD SCK 14, ADC de batería 35,
LED onboard 25.

`USERMOD_ID_LORA` es 59 y `PinOwner::UM_LoRa` apunta a ese id (`wled00/const.h`,
`wled00/pin_manager.h`). `USERMOD_ID_USB_OLED` es 60 y `JSON_LOCK_USB` es 25;
solo los usa el usermod de banco. LoRa usa `JSON_LOCK_LORA` 24.

## Recepción LoRa

Código: `usermods/lora_rx/lora_rx.cpp`. `library.json` lleva
`"libArchive": false` para que `REGISTER_USERMOD` quede enlazado.

Arranque en `setup()`:

1. Carga la llave AES desde `/aes128.key`.
2. Inicializa la OLED e imprime el primer estado.
3. Reutiliza el SPI si WLED ya lo marcó `HW_SPI`. No llama a `allocatePin`
   con tag `HW_SPI`: `pinManager` lo rechaza.
4. Reclama CS, RST, DIO0, DIO1 y DIO2.
5. `SPI.begin(5, 19, 27, 18)`.
6. `SX1276(Module(CS, DIO0, RST, RADIOLIB_NC))` y `begin(915.0)`.

`begin(frecuencia)` deja el resto en los defaults de RadioLib SX1276:

| Parámetro | Valor |
|---|---|
| Ancho de banda | 125 kHz |
| Factor de expansión | 9 |
| Coding rate | 4/7 (`cr = 7` en RadioLib) |
| Sync word | `0x12` (`RADIOLIB_SX127X_SYNC_WORD`). No es el `0x34` de LoRaWAN |
| Preámbulo | 8 |
| CRC | activo |
| Potencia | 10 dBm |

El emisor tiene que usar exactamente esa PHY. Un fallo de `begin` no impide
que la OLED siga mostrando Wi-Fi e IP.

La ISR de DIO0 solo entrega un semáforo binario, y solo si el rol es receptor
por LoRa. La tarea `lora_rx` corre en el core 0, prioridad 1, stack 10240
bytes. Espera el semáforo 100 ms, no `portMAX_DELAY`, y hace `delay(1)` para
no disparar el watchdog del idle. Después de cada trama vuelve a
`startReceive()`. Durante un OTA, `onUpdateBegin` suspende y reanuda esa
tarea.

## Rol: transmisor o receptor

Se elige en **Config → Usermods → lora_rx**. Persiste en `cfg.json` como
`lora_role` (`rx` o `tx`) y `rx_input` (`lora` o `wifi`). Si faltan, el
arranque queda en receptor por LoRa, el comportamiento anterior.

| Rol | Qué hace con un `POST /json/state` | Tira local |
|---|---|---|
| `tx` | Decodifica Base64 del campo `aes` y pone en el aire esos bytes (`0xA1…`). No descifra, no hace MessagePack, no vuelve a cifrar | No cambia |
| `rx` + `lora` | Ignora el estado HTTP. Aplica la trama LoRa: AES y luego MessagePack | Sí |
| `rx` + `wifi` | Descifrado AES y JSON, como la API de siempre. No aplica tramas LoRa | Sí |

`POST /json/cfg` y el formulario de Usermods siguen funcionando en los tres
modos. El transmisor encola la trama y la radio la envía en su tarea; el
manejador HTTP no espera el tiempo de aire. La PHY de transmisión es la misma
de `begin(915.0)`.

El orquestador solo manda MessagePack cifrado a las IP listadas en
`WLED_LORA_GATEWAY_IPS`. El resto de dispositivos sigue recibiendo JSON
cifrado por Wi-Fi.

Tope de payload del SX1276: 255 bytes. Un paquete de longitud 0 o mayor se
descarta.

## Cifrado y MessagePack

Sobre de cada trama, LoRa y HTTP usan el mismo:

```text
0xA1 | nonce 12 bytes | ciphertext | tag GCM 16 bytes
```

AES-128-GCM, sin AAD. Overhead 29 bytes. El MessagePack útil máximo es
226 bytes. El plaintext LoRa es un objeto MessagePack con las mismas claves
que `POST /json/state`. Tras descifrar, `unpackAndApply` hace
`MsgPack::Unpacker::feed` + `deserialize` sobre `gDoc` y llama a
`deserializeState(..., CALL_MODE_DIRECT_CHANGE)`.

El include de MsgPack va después de anular las macros `R`, `G`, `B` y `W`
de `colors.h`. Si no, rompen las plantillas de ArxTypeTraits. Antes del
include está `DEBUGLOG_DEFAULT_LOG_LEVEL_ERROR` para callar el `#warning`
TRACE de DebugLog.

Acepta el estado en la raíz o envuelto en `{"state":{...}}` solo cuando la
raíz no trae `on`, `bri`, `seg` ni `ps`. No entiende `loop`, `steps` ni
`duration_ms`. Una secuencia la temporiza el emisor, un estado por paquete.

Los últimos 16 nonces se recuerdan. Un nonce repetido se rechaza. El candado
AES y el de la decodificación Base64 son mutex distintos. `decryptBase64`
toma el segundo y luego `decryptFrame` toma el primero. No invertir ese orden.

Sin archivo de llave, LoRa no aplica paquetes. HTTP sigue en claro hasta que
la llave existe.

## Llave y canales Wi-Fi

La llave son 32 hexadecimales (16 bytes) en `/aes128.key`. No se persiste en
`cfg.json`. `addToConfig` escribe `aes_key` y `aes_unlock` vacíos para que
el formulario de Usermods pueda editarlos. `readFromConfig` ignora el vacío.
Si ya hay llave, un valor nuevo solo se acepta cuando `aes_unlock` coincide
en comparación de tiempo constante. La primera alta no pide unlock.

UI: **Config → Usermods → lora_rx**. Esos campos los pinta
`settings_um.htm` a partir del JSON del usermod. `appendConfigData` los
marca como `password` y `maxLength=32`.

Cuando `aesReady` es verdadero, estos sitios llaman al usermod. En otros
entornos las mismas funciones son símbolos débiles en `wled00/aes_link.cpp`
y no bloquean nada.

| Sitio | Función | Comportamiento con llave |
|---|---|---|
| `wled00/wled_server.cpp`, POST `/json` (incluye `/json/si` y `/json/cfg`) | `wledAesAccept` | El cuerpo debe ser `{"aes":"<base64 del sobre>"}`. Se sustituye el `JsonDocument` por el JSON descifrado |
| `wled00/ws.cpp`, texto WebSocket | `wledAesAccept` | Igual. Un fallo responde `{"error":1}` |
| `wled00/set.cpp`, `handleSet` con `request != nullptr` | `wledAesAllowHttp` | Cabecera `X-WLED-AES`. El plaintext tiene que ser `request->url()` exacto, por ejemplo `/win&T=2` |

`{"v":true}` y un objeto de un solo campo `lv` siguen en claro. Son lectura
y live view, no cambian los LEDs.

`wled00/data/index.js` (`requestJson`) cifra con Web Crypto AES-GCM si
`lastinfo.aes128` es verdadero. La llave de sesión está en
`sessionStorage.wled_aes_key`. Un 401 o `{"error":1}` la borra y vuelve a
pedirla. Tras editar `index.js` hay que regenerar las cabeceras con
`npm run build`. `pio run` ya lo hace.

No pasan por esta llave: MQTT, UDP, botones, infrarrojos, serie, presets
cargados en local y el POST HTML de `/settings`. Esos caminos llaman a
`deserializeState` o `handleSet` sin `AsyncWebServerRequest`, o usan otro
handler. Alexa queda detrás de `onNotFound`, después de `handleSet`.

## OLED

Sigue en `lora_rx`, no en el usermod USB. SSD1306 por el `Wire` de los
pines 21 y 22. `oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)` no
reinicia el bus. Antes del include de Adafruit se anulan `WHITE` y `BLACK`
porque `FX.h` ya los define.

`loop()` redibuja al cambiar el estado. En receptor, como máximo una vez por
segundo. En transmisor, cada 200 ms, para alcanzar el aviso de envío. También
al conectar Wi-Fi. La fuente por defecto es ASCII: no hay tildes.

| Texto | Significado |
|---|---|
| `WiFi OK` / `WiFi AP` / `WiFi OFF` | Estación, solo AP (`4.3.2.1`, SSID `WLED-AP`), o sin red |
| IP en la segunda línea | `WLEDNetwork.localIP()` o la IP del AP |
| `AES` `ON` / `OFF` | Existe `/aes128.key` válida |
| `RECIBE` grande | Rol receptor |
| `TRANSMITE` grande | Rol transmisor. No aplica la tira local |
| `OFF` grande | La radio no arrancó |
| `via LoRa` / `via WiFi` | Canal por el que entran o salen los datos |
| Recuadro `ENVIANDO` / `datos via LoRa` | Hay una trama en el aire, y sigue 2 s después |
| `n:` | Paquetes aplicados (receptor) o enviados (transmisor) |

## Build y flasheo

Desde el directorio `WLED` de este árbol. `pio` está en
`~/.platformio/penv/bin`, no en el `PATH` por defecto.

```bash
export PATH="$HOME/.platformio/penv/bin:$PATH"
pio run -e ttgo_t32
pio run -e ttgo_t32 -t upload --upload-port /dev/ttyACM0
```

El puerto visto en esta máquina es `/dev/ttyACM0`. Abrirlo con un monitor
reinicia la placa por DTR/RTS. Hay que cerrarlo antes de flashear. Si
esptool no engancha: mantener BOOT, pulsar RST, soltar BOOT y repetir.

Salida: `build_output/release/WLED_17.0.0-devV5_TTGO_T32.bin`.
La última imagen flasheada incluye OLED, LoRa y AES. Flash ocupada en torno
al 87 %. No añadir otra copia de GFX ni un segundo SSD1306.

`lib_compat_mode` del proyecto es `strict`. RadioLib resuelve en 7.x
(en el último build, 7.8.1) y MsgPack en 0.4.2.

## Repositorios

Este directorio `WLED/` tiene su propio git, distinto del repo padre
`CONTROL LEDS`.

| Remote | Uso |
|---|---|
| `origin` | `https://github.com/wled/WLED.git`. No empujar ahí |
| `wled_lora` | `https://github.com/jijunahe/WLED_LORA.git`. Fork de trabajo |

El primer commit de configuración de plataforma se subió a `wled_lora`.
El receptor LoRa, el AES, la OLED de producción y este documento son cambios
posteriores. No asumir que `wled_lora` ya los contiene: comprobar
`git status` antes de publicar.

## Invariantes para quien continúe

- No editar el núcleo de WLED salvo el gancho de autorización
  (`aes_link.h`, las tres llamadas en servidor, WebSocket y `handleSet`)
  y los ids de `const.h` / `pin_manager.h`.
- No desactivar Wi-Fi para “hacer sitio” a LoRa. La tarea ya va a prioridad 1.
- No compartir `Serial` ni la OLED con un segundo usermod.
- No usar los GPIO del SX1276 ni los de la OLED para la tira.
- El transmisor no cifra ni empaqueta. Reenvía la trama `0xA1` que ya armó el
  orquestador. El receptor LoRa espera MessagePack dentro de esa trama, no
  JSON con espacios, y no la cabecera USB `0xA5 0x5A`. Esa cabecera pertenece
  solo a `usb_oled_test`.
- Un paquete que no descifra no debe llegar a `deserializeState`.
- Comentarios de código en inglés. El marcador de bloques generados es
  `// AI: below section was generated by an AI` … `// AI: end`.
- Indentación de dos espacios en el usermod.
- No hacer `git push` a `origin`.
