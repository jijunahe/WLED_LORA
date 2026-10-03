# Manual de JSON para controlar la TTGO por LoRa

Este documento es el contrato entre un aplicativo web y el firmware de la TTGO
(`env:ttgo_t32`, usermod `lora_rx`). El aplicativo genera un estado de WLED.
El emisor lo convierte a MessagePack y lo manda en un solo paquete LoRa. La
TTGO lo descomprime y lo aplica con `deserializeState()`.

La tira usa el GPIO 4. Ese pin no va en el JSON: ya está fijado en el firmware.
La animación, una vez aceptada, sigue en la TTGO. LoRa no transporta fotogramas.

La TTGO puede ser transmisor o receptor (Config → Usermods). El transmisor
no comprime ni cifra: recibe por Wi-Fi la trama `{"aes":"..."}` ya armada y
pone esos bytes en el aire. El receptor por LoRa descifra y aplica el
MessagePack. El receptor por Wi-Fi aplica JSON, cifrado o en claro, y no
escucha LoRa. El canal del receptor es uno u otro.

## Límites que el aplicativo debe cumplir

El receptor es un SX1276. El payload de radio es el MessagePack completo, sin
cabecera extra.

| Límite | Valor |
|---|---|
| Payload máximo de radio | 255 bytes |
| MessagePack útil tras AES-128-GCM | 226 bytes |
| Frecuencia | 915.0 MHz |
| Ancho de banda | 125 kHz |
| Factor de expansión | 9 |
| Código | 4/7 |
| Palabra de sincronización | `0x12` |
| Preámbulo | 8 símbolos |
| CRC | activo |
| Segmentos en esta placa | 32 |
| Colores por segmento | 3 |

El emisor tiene que usar exactamente esa radio. Un paquete de otro ancho de
banda, factor de expansión o palabra de sincronización no lo decodifica la TTGO.

Tiempos en el aire con esa radio, según el tamaño MessagePack:

| Bytes | Tiempo aproximado |
|---|---|
| 5 | 0,14 s |
| 35 | 0,31 s |
| 51 | 0,43 s |
| 164 | 1,14 s |
| 241 | 1,63 s |
| 255 | 1,72 s |

El aplicativo debe esperar al menos ese tiempo antes de enviar el siguiente
paquete. Un paso de 1 segundo solo admite las posibilidades pequeñas (hasta
unos 50 bytes de MessagePack). Un estado grande se envía una vez y la animación
queda corriendo en la placa.

Esos tiempos son del MessagePack. En el aire va además el sobre AES: 1 byte de
versión, 12 de nonce y 16 de etiqueta. El paquete de radio mide
MessagePack + 29 y no puede pasar de 255.

Los espacios y los saltos de línea no se transmiten. Las cuentas de caracteres
de este manual son del JSON minificado. La comprobación real es el tamaño
MessagePack, no el número de caracteres.

## Regla de generación

1. Construir un objeto de estado de WLED.
2. Codificarlo en MessagePack.
3. Rechazarlo si el MessagePack supera 226 bytes.
4. Cifrarlo con la llave AES-128 del nodo y enviarlo como único payload LoRa.
5. Dejar en el aplicativo la duración y el bucle. Esos datos no viajan.

Cada paquete debe poder aplicarse solo. Si se pierde el anterior, el siguiente
vuelve a encender, fijar el brillo y definir el efecto y los colores. Por eso
las posibilidades recomendadas repiten `on`, `bri` y el segmento completo.

El objeto se envía en la raíz. La TTGO también acepta `{"state":{...}}` cuando
la raíz no trae `on`, `bri`, `seg` ni `ps`. El aplicativo debe usar la raíz
directa.

Campos que el generador puede emitir:

| Campo | Rango | Uso |
|---|---|---|
| `on` | `true` o `false` | Enciende o apaga |
| `bri` | 0–255 | Brillo |
| `ps` | índice de preset | Carga un preset ya guardado en la placa |
| `seg` | lista | Segmentos a modificar |
| `id` | 0–31 | Segmento |
| `start`, `stop` | índice de LED | Tramo. `stop` es el primer LED que queda fuera |
| `fx` | índice de efecto | Ver la tabla de efectos |
| `sx` | 0–255 | Velocidad |
| `ix` | 0–255 | Intensidad |
| `pal` | índice de paleta | Paleta integrada en el firmware |
| `col` | hasta 3 colores `[r,g,b]` | Cada canal va de 0 a 255 |

`start` y `stop` deben coincidir con la cantidad de LEDs configurada en WLED.
El ejemplo de cuatro zonas supone 120 LEDs. Con otra longitud, se divide entre
el número de zonas.

## Posibilidades

### 1. Apagar

Sirve como paro de emergencia. El aplicativo puede mandarlo en cualquier momento.

```json
{"on":false}
```

12 caracteres, 5 bytes, unos 0,14 s.

### 2. Color sólido

Un solo color en toda la tira. Es la posibilidad base para un botón de color.

```json
{"on":true,"bri":160,"seg":[{"id":0,"fx":0,"col":[[255,0,0]]}]}
```

63 caracteres, 35 bytes, unos 0,31 s. `fx` 0 es Solid.

Para otro color se cambia solo la tripleta y se reenvía el objeto entero.

### 3. Un efecto con un color

La animación usa el color principal. `sx` controla la velocidad.

```json
{"on":true,"bri":160,"seg":[{"id":0,"fx":2,"sx":128,"col":[[255,0,0]]}]}
```

72 caracteres, 40 bytes, unos 0,37 s. `fx` 2 es Breathe.

Cabe en un paso de 1 segundo.

### 4. Un efecto con tres colores

Máximo de colores propios dentro de un mismo segmento: principal, fondo y
tercero. Un cuarto color en el mismo `col` lo ignora WLED.

```json
{"on":true,"bri":160,"seg":[{"id":0,"fx":13,"sx":128,"col":[[255,0,0],[0,68,255],[0,255,136]]}]}
```

96 caracteres, 51 bytes, unos 0,43 s. `fx` 13 es Theater. Los tres colores
animan a la vez sobre los mismos LEDs.

### 5. Efecto de paleta

La paleta integrada aporta hasta 16 colores. El JSON no los enumera: envía el
índice `pal`. El firmware ya tiene esos colores.

```json
{"on":true,"bri":160,"seg":[{"id":0,"fx":8,"sx":128,"pal":11}]}
```

63 caracteres, 35 bytes, unos 0,31 s. `fx` 8 es Colorloop. `pal` 11 es Rainbow.

Índices de paleta útiles en este firmware:

| `pal` | Nombre |
|---|---|
| 0 | Default |
| 6 | Party |
| 11 | Rainbow |
| 12 | Rainbow Bands |

Esta es la forma de obtener muchos colores sin acercarse al límite de 255 bytes.

### 6. Varias zonas, un color por zona

Cada color visible en un tramo distinto es un segmento, con su efecto. Hace
falta `start` y `stop` para crear el tramo.

Cuatro zonas sobre 120 LEDs:

```json
{"on":true,"bri":160,"seg":[{"id":0,"start":0,"stop":30,"fx":2,"sx":128,"col":[[255,0,0]]},{"id":1,"start":30,"stop":60,"fx":1,"sx":180,"col":[[0,68,255]]},{"id":2,"start":60,"stop":90,"fx":13,"sx":140,"col":[[0,255,136]]},{"id":3,"start":90,"stop":120,"fx":12,"sx":100,"col":[[255,255,0]]}]}
```

292 caracteres, 165 bytes, unos 1,14 s. Entra en un paquete. Se envía una vez;
después cada zona anima sola.

En un solo paquete cifrado caben **5 segmentos de un color** (202 bytes de
MessagePack). El sexto ya no entra. La placa admite 32 segmentos, así que del
6 al 31 van en paquetes siguientes, cada uno con sus `id`, `start` y `stop`.

### 7. Varias zonas, tres colores por zona

Cada segmento puede llevar sus tres colores. En un paquete cifrado caben **4
segmentos con 3 colores** (12 colores, 208 bytes de MessagePack). El quinto
segmento con tres colores ya no entra.

El aplicativo, al armar este estado, codifica a MessagePack y corta en varios
paquetes si el resultado pasa de 255. Cada paquete incluye `on`, `bri` y los
segmentos de ese lote.

### 8. Secuencia en el tiempo

Una secuencia es una lista de posibilidades 2, 3 o 4. El aplicativo guarda
`duration_ms` y `loop`, espera el tiempo de cada paso y envía el estado
siguiente. Esos dos campos no forman parte del JSON transmitido.

Ejemplo de guion, solo en el aplicativo:

| Orden | Estado a transmitir | Duración en el aplicativo |
|---|---|---|
| 1 | Posibilidad 2 con rojo | 1000 ms |
| 2 | Posibilidad 2 con azul `[0,68,255]` | 1000 ms |
| 3 | Posibilidad 3, Breathe, verde | 2000 ms |

Cada fila es un paquete de unos 35–40 bytes. Un paso de 1000 ms alcanza, porque
el aire dura menos de medio segundo.

### 9. Preset guardado en la placa

Si el preset ya está en la TTGO (por la interfaz de WLED), el aplicativo solo
envía el índice.

```json
{"ps":1}
```

8 caracteres, 5 bytes, unos 0,14 s. El preset puede ser una escena grande: el
tamaño LoRa no cambia, porque los datos ya están en la placa. Si ese índice no
existe, la TTGO no tiene esa escena.

## Efectos para el generador

Índice `fx` de este firmware. El nombre que va después de `@` en el código
fuente son parámetros del efecto; aquí solo importa el nombre.

| `fx` | Nombre |
|---|---|
| 0 | Solid |
| 1 | Blink |
| 2 | Breathe |
| 3 | Wipe |
| 4 | Wipe Random |
| 5 | Random Colors |
| 6 | Sweep |
| 7 | Dynamic |
| 8 | Colorloop |
| 9 | Rainbow |
| 10 | Scan |
| 11 | Scan Dual |
| 12 | Fade |
| 13 | Theater |
| 14 | Theater Rainbow |
| 15 | Running |

El firmware tiene más efectos. Un `fx` nuevo se agrega a esta tabla solo después
de comprobar su índice en `wled00/FX.h` de esta misma versión.

## Llave AES-128

La misma llave cubre LoRa y el control por Wi-Fi. Se guarda en la placa, no en
`cfg.json`. En la interfaz: **Config → Usermods → lora_rx**.

| Campo | Uso |
|---|---|
| `aes_key` | 32 caracteres hexadecimales (16 bytes). En blanco conserva la llave actual |
| `aes_unlock` | Llave actual. Solo hace falta para reemplazarla |

Hasta que se guarda una llave, la API web sigue abierta y LoRa ignora los
paquetes. En cuanto existe, los dos canales exigen el sobre.

Trama binaria, igual en LoRa y dentro del Base64 de HTTP:

```text
0xA1 | nonce de 12 bytes | ciphertext | etiqueta GCM de 16 bytes
```

AES-128-GCM, sin datos asociados. El plaintext de LoRa es el MessagePack. El
plaintext HTTP es el JSON de estado. El nonce no puede repetirse entre los
últimos 16 mensajes; si se repite, la placa lo rechaza.

Por HTTP y por el WebSocket de la interfaz, el cuerpo pasa a ser:

```json
{"aes":"<base64 de la trama>"}
```

`{"v":true}` y `{"lv":...}` siguen en claro: solo piden estado o el live view.
La API `/win` lleva la misma trama en la cabecera `X-WLED-AES`. El plaintext de
esa cabecera es la URL exacta, por ejemplo `/win&T=2`.

La interfaz principal pide la llave una vez por sesión del navegador y cifra
cada cambio de color, brillo o efecto. Si la llave no coincide, hay que
volver a introducirla.

## Lo que el aplicativo debe rechazar

Estas salidas no deben llegar al emisor:

- Un JSON de secuencia completo, con `loop`, `steps` y `duration_ms`. El de
  referencia de este proyecto ocupa 795 caracteres y 468 bytes en MessagePack.
  El SX1276 lo corta. Además `deserializeState()` no reproduce ese guion.
- Cualquier objeto cuyo MessagePack supere 226 bytes.
- Un cuarto color dentro del mismo `col`.
- Más de 5 segmentos de un color, o más de 4 segmentos de tres colores, en el
  mismo paquete.
- Un paquete LoRa o un POST de estado sin el sobre AES, cuando la llave ya está
  guardada.
- Texto, espacios de relleno o claves que no estén en la tabla de campos.
- Fotogramas de la animación. El efecto se describe una vez.

## Comprobación antes de enviar

El aplicativo puede usar esta decisión:

1. Si el usuario eligió un preset existente, generar la posibilidad 9.
2. Si eligió una paleta, generar la posibilidad 5.
3. Si eligió hasta 3 colores sobre toda la tira, generar la posibilidad 3 o 4.
4. Si repartió colores en zonas, generar la posibilidad 6 o 7 y partir en
   paquetes de como máximo 226 bytes de MessagePack.
5. Si definió una línea de tiempo, guardar los tiempos en el aplicativo y emitir
   una posibilidad 2 o 3 por paso.
6. Codificar a MessagePack. Si el resultado supera 226 bytes, no transmitir y
   avisar en la interfaz.
7. Cifrar con AES-128-GCM y la llave configurada en el nodo.

La prueba por USB de `usb_oled_test` no usa el mismo sobre que LoRa. En USB la
trama es `0xA5 0x5A`, un byte de longitud y el MessagePack. En LoRa el payload
es solo el MessagePack. El aplicativo, al hablar con la radio, no debe anteponer
esa cabecera.
