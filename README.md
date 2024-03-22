# IoT Projekt 2

Quellcode zum 2. Projekt für das IoT-Labor der Heinrich-Hertz-Schule
Karlsruhe.

## Quick-Start

```shell
cmake --preset "default"
cmake --build --preset "debug"   # Debug build
cmake --build --preset "release" # Release build
```

## Lokale Entwicklungsumgebung

Zur einfachen Installation einer lokalen Entwicklungsumgebung ist eine
`docker-compose` Datei bereitgestellt.

Um diese verwenden zu können muss
[Docker](https://www.docker.com/products/docker-desktop)
auf der Entwicklungsmaschine installiert sein.

Dann lässt sich die Umgebung mit dem Befehl `docker compose up -d` im
Projektverzeichnis starten.

Die folgenden Dienste werden zur Verfügung gestellt:

- Mosquitto MQTT Server unter `localhost:1883`
- InfluxDB Datenbank unter `localhost:8086`
