# Pilot – Vehicle Data Collection and Display Platform

> 车辆数据采集与展示平台 | Vehicle Data Collection and Display Platform

---

## Overview / 概述

**English:** Pilot is a real-time vehicle telemetry platform written in C++17. It collects sensor data (temperature, RPM, battery state-of-charge, IMU), evaluates configurable conditions, persists events to PostgreSQL, and exposes a REST + Server-Sent Events API consumed by a built-in web dashboard.

**中文:** Pilot 是一个用 C++17 编写的实时车辆遥测平台。它采集传感器数据（温度、转速、电池荷电状态、IMU），对可配置条件进行评估，将事件持久化到 PostgreSQL，并通过内置 Web 仪表盘消费的 REST + SSE API 对外暴露。

---

## Architecture / 架构

The system is organized into **4 layers**:

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
│  TemperatureSensor, RpmSensor, BatterySocSensor,    │
│  ImuSensor (simulated with Gaussian noise)          │
└─────────────────────────────────────────────────────┘
         │ optional persistence
         ▼
  PostgreSQL (src/db/pg_client.cpp, libpq)
```

系统由 **4 层**组成：
1. **传感器层**：温度、转速、电池 SOC、IMU 传感器（高斯噪声模拟）
2. **引擎层**：无锁 SPSC 环形缓冲区、双线程数据流、滑动窗口统计、条件引擎
3. **控制层**：车辆配置文件（ECO / NORMAL / SPORT / CHARGE）与自动安全响应
4. **服务器层**：REST API + SSE 实时推送 + Web 仪表盘

---

## Build Instructions / 构建说明

**Dependencies fetched automatically via CMake FetchContent:**
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) v0.18.1
- [GoogleTest](https://github.com/google/googletest) v1.14.0 (tests only)

**Optional:** PostgreSQL development libraries (`libpq-dev` on Debian/Ubuntu).

```bash
# Install optional PostgreSQL dev libraries (Debian/Ubuntu)
sudo apt install libpq-dev

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run tests
cd build && ctest --output-on-failure
```

---

## Running / 运行

```bash
# Default port 8080, no database
./build/pilot_server

# Custom port and PostgreSQL connection
./build/pilot_server --port 9090 --db "host=localhost dbname=pilot_db user=pilot password=pilot"
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

## WebSocket / Server-Sent Events

The `/events` endpoint uses [Server-Sent Events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events) (SSE) to push real-time sensor data to the browser with event type `sensor_data`:

```
event: sensor_data
data: {"sensor_id":"temp_0","value":85.3,"unit":"°C","timestamp":"2024-01-01T00:00:00Z"}
```

The dashboard automatically reconnects on connection loss and maintains a rolling 60-point history per sensor.
