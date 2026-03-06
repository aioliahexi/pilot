# Pilot – UUV Data Collection and Display Platform

> 水下无人航行器(UUV)数据采集与展示平台 | Unmanned Underwater Vehicle Data Collection Platform

---

## Overview / 概述

**English:** Pilot is a real-time telemetry platform written in C++17 designed for Unmanned Underwater Vehicles (UUVs). It collects sensor data (GPS, multi-thruster RPM, depth, INS/IMU attitude, accelerometer, DVL velocity/depth, ocean currents, wind fields, temperature, battery SOC), evaluates configurable conditions, persists time-series data to PostgreSQL 17 with PostGIS, and exposes a REST + Server-Sent Events API consumed by a built-in web dashboard. The platform supports multi-device operation and targets NVIDIA Jetson Xavier / Orin ARM platforms.

**中文:** Pilot 是一个用 C++17 编写的实时遥测平台，专为水下无人航行器（UUV）设计。它采集传感器数据（GPS、多推进器转速、深度、惯导姿态、加速度计、DVL多普勒测速/测深、洋流、风场、温度、电池SOC），对可配置条件进行评估，将时序数据持久化到 PostgreSQL 17（含 PostGIS），并通过内置 Web 仪表盘的 REST + SSE API 对外暴露。平台支持多设备运行，目标运行环境为 NVIDIA Jetson Xavier / Orin 等 ARM 平台。

---

## Architecture / 架构

```
┌─────────────────────────────────────────────────────┐
│  Layer 4 – Server Layer (src/server/)               │
│  REST API (cpp-httplib) + SSE + Web Dashboard       │
├─────────────────────────────────────────────────────┤
│  Layer 3 – Control Layer (src/control/)             │
│  VehicleController, VehicleProfile (ECO/NORMAL/     │
│  SPORT/CHARGE), automatic safety responses          │
├─────────────────────────────────────────────────────┤
│  Layer 2 – Engine Layer (src/engine/)               │
│  Lock-free SPSC RingBuffer, DataStream (poll +      │
│  dispatch threads), SlidingWindow, ConditionEngine  │
├─────────────────────────────────────────────────────┤
│  Layer 1 – Sensor Layer (src/sensors/)              │
│  GPS, IMU, Depth, DVL, Thrusters (RPM),             │
│  Temperature, Battery SOC (simulated/real)           │
└─────────────────────────────────────────────────────┘
         │ persistence (multi-device, time-series)
         ▼
  PostgreSQL 17 + PostGIS (sql/000_init.sql)
```

---

## Configuration / 配置

All configuration is centralized in a single JSON file: **`config/pilot.json`**

```json
{
    "device": {
        "id": "uuv-001",
        "name": "UUV Alpha",
        "type": "UUV"
    },
    "server": { "port": 8080 },
    "database": {
        "host": "localhost",
        "port": 5432,
        "name": "pilot_db",
        "user": "pilot",
        "password": "pilot"
    },
    "sensors": {
        "poll_interval_ms": 200,
        "condition_window_seconds": 10,
        "thrusters": [
            { "id": "thruster_0", "base_rpm": 3000.0 },
            { "id": "thruster_1", "base_rpm": 3000.0 }
        ]
    },
    "control": {
        "default_mode": "NORMAL",
        "soc_low_threshold": 10.0
    }
}
```

Specify the config file with `--config`:
```bash
./pilot_server --config config/pilot.json
```

CLI flags (`--port`, `--db`) and environment variables (`PGHOST`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`) override config file values.

---

## Build Instructions / 构建说明

**Dependencies fetched automatically via CMake FetchContent:**
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) v0.18.1
- [GoogleTest](https://github.com/google/googletest) v1.14.0 (tests only)

**Optional:** PostgreSQL development libraries (`libpq-dev` on Debian/Ubuntu).

### Native Build (x86 or ARM)

```bash
# Install optional PostgreSQL dev libraries
sudo apt install libpq-dev

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run tests
cd build && ctest --output-on-failure
```

### Cross-Compilation (x86 → ARM64)

Use the provided CMake toolchain file:
```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=docker/toolchain-aarch64.cmake
cmake --build build --parallel
```

---

## Docker Build / Docker 构建

### Native ARM64 Build (on Jetson)

```bash
docker build -f docker/Dockerfile.arm64 -t pilot:arm64 .
```

### Cross-Compilation Build (x86 host → ARM64 target)

```bash
docker build -f docker/Dockerfile.cross -t pilot:cross-arm64 .
# Extract binary:
docker create --name tmp pilot:cross-arm64
docker cp tmp:/pilot_server ./pilot_server
docker rm tmp
```

### Full Stack with Docker Compose

```bash
# Start PostgreSQL 17 + PostGIS + Pilot application
docker compose up -d

# View logs
docker compose logs -f pilot

# Stop
docker compose down
```

The `docker-compose.yml` automatically:
- Starts PostgreSQL 17 with PostGIS 3.5
- Initializes the UUV database schema from `sql/000_init.sql`
- Starts the Pilot server connected to the database

---

## Jetson Deployment / Jetson 部署

For deploying on a fresh NVIDIA Jetson Xavier or Orin:

```bash
sudo bash scripts/jetson_setup.sh
```

This script:
1. Installs PostgreSQL 17, PostGIS, and build tools
2. Creates the `pilot_db` database and `pilot` user
3. Initializes the UUV schema (PostGIS + all sensor tables)
4. Builds the application natively on ARM64
5. Installs to `/opt/pilot/`
6. Creates a `systemd` service for auto-start

After setup:
```bash
sudo systemctl start pilot      # Start the service
sudo journalctl -u pilot -f     # View logs
sudo nano /opt/pilot/config/pilot.json  # Edit configuration
```

---

## Database Schema / 数据库结构

The PostgreSQL 17 schema (`sql/000_init.sql`) includes PostGIS for spatial queries and supports multi-device UUV data:

| Table | Description |
|-------|-------------|
| `devices` | Device registry (multi-UUV support) |
| `gps_data` | GPS positions with PostGIS geometry (auto-populated) |
| `thruster_data` | Multi-thruster RPM, power, current, voltage |
| `depth_data` | Depth sensor readings (pressure, temperature) |
| `ins_data` | INS/IMU: attitude (roll/pitch/yaw), accel, gyro, mag |
| `dvl_data` | DVL: bottom/water-track velocity, altitude, depth |
| `ocean_current_data` | Ocean currents at depth layers (speed, direction, GPS-linked) |
| `wind_data` | Wind field: speed, direction, gusts (GPS-linked) |
| `battery_data` | Battery SOC, voltage, current, power |
| `controller_events` | System events and condition triggers |
| `sensor_events` | Legacy event table (backward compatible) |

### PostGIS Features
- GPS data automatically generates PostGIS `Point` geometry via triggers
- Ocean current and wind data spatially linked to GPS positions
- Spatial indexes (GIST) for efficient geo-queries

### Useful Views
- `v_device_latest_position` – Latest GPS position per device
- `v_device_latest_depth` – Latest depth per device
- `v_current_profile` – Ocean current profiles by depth layer

---

## Running / 运行

```bash
# Default: port 8080, no database
./build/pilot_server

# With config file
./build/pilot_server --config config/pilot.json

# With CLI overrides
./build/pilot_server --config config/pilot.json --port 9090

# With environment variables (override config)
PGHOST=localhost PGDATABASE=pilot_db PGUSER=pilot PGPASSWORD=pilot \
    ./build/pilot_server --config config/pilot.json
```

Open **http://localhost:8080** in a browser to view the live dashboard.

---

## API Endpoints / API 接口

| Method | Path | Description |
|--------|------|-------------|
| `GET`  | `/api/status` | Current vehicle controller status (JSON) |
| `GET`  | `/api/profile` | Active vehicle profile details (JSON) |
| `POST` | `/api/profile` | Set vehicle mode: `{"mode":"ECO"\|"NORMAL"\|"SPORT"\|"CHARGE"}` |
| `GET`  | `/api/conditions` | Map of condition names → triggered state (JSON) |
| `GET`  | `/events` | **Server-Sent Events** stream of sensor readings |
| `GET`  | `/` | Redirects to the web dashboard |

---

## Project Structure / 项目结构

```
pilot/
├── config/
│   └── pilot.json              # Centralized configuration file
├── docker/
│   ├── Dockerfile.arm64        # Native ARM64 build (Jetson)
│   ├── Dockerfile.cross        # x86 → ARM64 cross-compilation
│   └── toolchain-aarch64.cmake # CMake cross-compilation toolchain
├── docker-compose.yml          # Full stack deployment
├── frontend/
│   └── index.html              # Web dashboard (Chart.js)
├── include/
│   ├── config/                 # Configuration loader
│   ├── control/                # Vehicle controller & profiles
│   ├── db/                     # PostgreSQL client
│   ├── engine/                 # Data pipeline (ring buffer, streams)
│   ├── sensors/                # Sensor interfaces
│   └── server/                 # REST API server
├── scripts/
│   └── jetson_setup.sh         # Jetson deployment script
├── sql/
│   └── 000_init.sql            # Database initialization (PostGIS)
├── src/                        # C++ implementations
├── tests/                      # GoogleTest unit tests
├── CMakeLists.txt              # Build system
└── README.md
```
