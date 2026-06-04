# Mejoras recomendadas y ajuste a los materiales de clase

Este archivo resume que mejoras conviene hacer o defender en el proyecto y como se ha ajustado al estilo de los ejemplos que te han pasado en clase.

## 1. Que se ha visto en los materiales de clase

Los ejemplos de `UDPsockets` y `Solucion TCPSockets` trabajan con una estructura sencilla:

- `WSAStartup`.
- Creacion de socket con `socket`.
- Direcciones con `sockaddr_in`.
- Conversion de IP con `inet_pton`.
- `bind` en el servidor.
- `sendto` y `recvfrom` en UDP.
- `send` y `recv` en TCP.
- Una estructura `DataPacket`.
- Funciones auxiliares tipo `treatError` y `treatErrorExit`.
- Una libreria comun para no repetir todo en cliente y servidor.

Tu proyecto mantiene esa base. La diferencia principal es que el enunciado exige mas cosas que los ejemplos basicos:

- JSON.
- Servidor multihilo.
- Juego real con estado.
- Al menos 7 acciones RPC.

Por eso el proyecto es necesariamente mas grande, pero sigue apoyandose en la misma idea: cliente, servidor, libreria comun y sockets UDP.

## 2. Lo que ya esta bien ajustado a tus conocimientos

### 2.1 Cliente sencillo

El cliente no calcula la partida.

Solo hace:

1. Arrancar WinSock.
2. Crear socket UDP.
3. Conectar con el lobby del servidor.
4. Enviar `INIT`.
5. Leer comandos de consola.
6. Enviar comandos.
7. Mostrar respuestas.

Esto encaja bien con los ejemplos de clase, donde el cliente basicamente envia un `DataPacket` y espera respuesta.

### 2.2 Libreria comun

La libreria mantiene funciones parecidas a las vistas:

- `udpCommonSocketSetup`
- `udpServerSocketSetup`
- `sendtoMsg`
- `recvfromMsg`
- `sendtorecvfromMsg`

La parte nueva es que ahora `sendtoMsg` convierte el paquete a JSON y `recvfromMsg` reconstruye el paquete desde JSON.

Esto es bueno para defenderlo asi:

> En clase vimos enviar una estructura directamente. En la practica se pide JSON, asi que he mantenido la misma interfaz de libreria, pero cambiando el formato real que viaja por la red.

### 2.3 Servidor con autoridad

El servidor guarda toda la logica del juego.

El cliente no decide:

- Donde hay minas.
- Si una celda es valida.
- Si hay victoria o derrota.
- Cuantas monedas se ganan.
- Si se activa la racha.

Esto es correcto para un modelo cliente-servidor.

### 2.4 RPC textual

Los comandos son faciles de explicar:

```text
INIT|8|8|10
STATE
REVEAL|2|3
FLAG|1|4
SHOP|STATE
EXIT
```

Esto ayuda a que parezca una practica de redes y no solo un juego.

## 3. Cambios hechos para que se entienda mejor

Se han anadido comentarios breves en el codigo:

- En la libreria, para explicar por que se usa JSON aunque se siga usando `DataPacket`.
- En el cliente, para aclarar que no calcula estado de juego.
- En el servidor, para aclarar que cada hilo atiende una sesion independiente.

No se han metido comentarios enormes para no ensuciar el codigo.

## 4. Mejoras importantes antes de entregar

Estas son las mejoras que mas suben nota y que si tienen sentido para tu nivel.

### 4.1 Anadir trazas reales a la memoria

El enunciado da 15 puntos a las trazas.

Conviene meter capturas o texto de:

- Arranque del servidor.
- Arranque del cliente.
- `CONNECT`.
- `OK|PORT|...`.
- `INIT`.
- `STATE`.
- `REVEAL`.
- `FLAG`.
- `CHEST|SPAWN|k`.
- `SHOP|STATE`.
- Error controlado, por ejemplo `REVEAL|99|99`.
- `EXIT`.
- Prueba con dos clientes para demostrar multihilo.

Ya hay una memoria actualizada con seccion de trazas en:

```text
memoria_actualizada/ENTORNOS_memoria_actualizada.pdf
```

### 4.2 Preparar una demo corta

Para defenderlo, conviene tener una secuencia preparada:

```text
STATE
CHEST|SPAWN|3
STATE
FLAG|0|0
SHOW|0|0
SHOP|STATE
REVEAL|1|1
PASS
EXIT
```

Y otra para errores:

```text
REVEAL|99|99
SHOP|BUY|LIFE
USE|SCAN|0|0
```

Asi puedes demostrar que no crashea.

### 4.3 Demostrar JSON

En consola se imprimen trazas como:

```text
Client sent JSON: {"client_id":1,"sequence":1,"msg":"STATE"}
Server received JSON: {"client_id":1,"sequence":1,"msg":"STATE"}
```

Eso es muy bueno para ensenar al profesor que el apartado JSON esta hecho.

### 4.4 Demostrar multihilo

Para demostrar multihilo:

1. Arranca el servidor.
2. Arranca un cliente.
3. Sin cerrar el primero, arranca otro cliente.

El servidor debe mostrar:

```text
Server[1]: session started
Server[2]: session started
```

Eso demuestra que el lobby sigue aceptando clientes mientras una sesion ya existe.

## 5. Mejoras que no recomiendo meter

Estas mejoras podrian sonar bien, pero no convienen para esta entrega.

### 5.1 Base de datos

El enunciado la valora como extra, pero puede complicar mucho:

- Instalacion.
- Dependencias.
- Errores nuevos.
- Mas cosas que defender.

No la recomiendo salvo que el resto este perfecto.

### 5.2 Interfaz grafica

Tambien es extra, pero no encaja con el requisito de cliente por terminal.

Puede subir nota, pero tambien puede hacer que el proyecto parezca menos centrado en redes.

No la recomiendo ahora.

### 5.3 IA o enemigo avanzado

No hace falta.

El enunciado pide cliente-servidor, RPC, UDP, JSON y multihilo. Meter IA puede distraer.

### 5.4 Protocolos demasiado complejos

No conviene meter:

- Autenticacion.
- Cifrado.
- Reintentos UDP avanzados.
- ACKs manuales.
- Serializadores externos grandes.

El proyecto debe poder explicarse con lo que habeis visto.

## 6. Riesgos actuales

### 6.1 Version de Visual Studio

Se fijo:

```text
VCToolsVersion=14.44.35207
```

En tu equipo esta instalada.

Si en otro ordenador no esta, habria que instalar esa herramienta desde Visual Studio Installer o cambiar los tres proyectos a la misma version disponible.

### 6.2 Puerto 4000 ocupado

Si el servidor no arranca o el cliente da:

```text
Client recvfrom error: 10054
```

lo mas probable es que:

- El servidor no este abierto.
- Otro proceso este usando el puerto 4000.
- Quede una instancia vieja del servidor abierta.

### 6.3 Ejecutable bloqueado

Si aparece:

```text
LINK : fatal error LNK1168
```

significa que el `.exe` esta abierto.

Solucion:

- Cerrar la ventana del servidor/cliente.
- Recompilar solucion.

## 7. Nota estimada tras los ajustes

Con el servidor multihilo, JSON, memoria actualizada y trazas, la practica esta mucho mejor que antes.

Estimacion razonable:

```text
8 - 8.5 / 10
```

Podria subir mas si:

- La memoria final incluye capturas reales.
- La demo sale fluida.
- El profesor ve claramente JSON y multihilo.
- No hay errores de compilacion en su Visual Studio.

## 8. Que deberias saber explicar

Para defender el proyecto, asegurate de poder explicar estas frases:

1. El cliente no calcula el juego; solo envia comandos RPC.
2. El servidor es la autoridad del estado.
3. UDP se usa con `sendto` y `recvfrom`.
4. El lobby escucha en `127.0.0.1:4000`.
5. Cada cliente recibe un puerto dedicado.
6. Cada cliente tiene un hilo y un `MinesweeperGame` propio.
7. JSON se usa como formato de intercambio.
8. Hay mas de 7 acciones RPC.
9. Los errores devuelven `ERROR|...` y no crashean.
10. La racha, cofres, tienda y puerta son logica del servidor.

## 9. Ajuste final recomendado

Mi recomendacion es no anadir mas funciones grandes.

Lo mejor ahora es:

- Mantener el codigo estable.
- Ensayar la demo.
- Llevar la memoria actualizada.
- Recompilar solucion antes de entregar.
- Tener claras las respuestas de defensa.

