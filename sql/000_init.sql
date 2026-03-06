-- =============================================================================
-- Pilot UUV Database Initialization Script
-- PostgreSQL 17.x with PostGIS Extension
-- =============================================================================
-- This script initializes the database for a UUV (Unmanned Underwater Vehicle)
-- data collection platform. It supports multi-device operation and stores
-- time-series sensor data including GPS, thrusters, depth, INS/IMU,
-- accelerometer, DVL, ocean currents, and wind field data.
--
-- Usage:
--   psql -U pilot -d pilot_db -f 000_init.sql
-- =============================================================================

-- Enable required extensions
CREATE EXTENSION IF NOT EXISTS postgis;            -- Spatial/GIS support
CREATE EXTENSION IF NOT EXISTS postgis_topology;   -- Topology support
CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE; -- Time-series optimization (optional)
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";        -- UUID generation

-- =============================================================================
-- 1. Device Registry
-- =============================================================================
-- Each UUV or sensor platform registers here.
CREATE TABLE IF NOT EXISTS devices (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    device_id       TEXT NOT NULL UNIQUE,
    device_name     TEXT NOT NULL,
    device_type     TEXT NOT NULL DEFAULT 'UUV',
    description     TEXT,
    metadata        JSONB DEFAULT '{}',
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_devices_device_id ON devices(device_id);
CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(device_type);

-- =============================================================================
-- 2. GPS / Position Data
-- =============================================================================
-- Stores GPS fixes with PostGIS geometry for spatial queries.
CREATE TABLE IF NOT EXISTS gps_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    latitude        DOUBLE PRECISION NOT NULL,
    longitude       DOUBLE PRECISION NOT NULL,
    altitude        DOUBLE PRECISION,
    speed_knots     DOUBLE PRECISION,
    course_deg      DOUBLE PRECISION,
    hdop            DOUBLE PRECISION,
    satellites      INTEGER,
    fix_quality     INTEGER,
    geom            GEOMETRY(Point, 4326),
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_gps_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_gps_device_ts ON gps_data(device_id, ts DESC);
CREATE INDEX IF NOT EXISTS idx_gps_geom ON gps_data USING GIST(geom);
CREATE INDEX IF NOT EXISTS idx_gps_ts ON gps_data(ts DESC);

-- Auto-populate PostGIS geometry from lat/lon on insert
CREATE OR REPLACE FUNCTION gps_update_geom()
RETURNS TRIGGER AS $$
BEGIN
    NEW.geom := ST_SetSRID(ST_MakePoint(NEW.longitude, NEW.latitude, COALESCE(NEW.altitude, 0)), 4326);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_gps_update_geom ON gps_data;
CREATE TRIGGER trg_gps_update_geom
    BEFORE INSERT OR UPDATE ON gps_data
    FOR EACH ROW EXECUTE FUNCTION gps_update_geom();

-- =============================================================================
-- 3. Thruster Data (multiple thrusters per device)
-- =============================================================================
CREATE TABLE IF NOT EXISTS thruster_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    thruster_id     TEXT NOT NULL,
    rpm             DOUBLE PRECISION,
    power_watts     DOUBLE PRECISION,
    current_amps    DOUBLE PRECISION,
    voltage_volts   DOUBLE PRECISION,
    temperature_c   DOUBLE PRECISION,
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_thruster_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_thruster_device_ts ON thruster_data(device_id, ts DESC);
CREATE INDEX IF NOT EXISTS idx_thruster_id ON thruster_data(thruster_id);

-- =============================================================================
-- 4. Depth Sensor Data
-- =============================================================================
CREATE TABLE IF NOT EXISTS depth_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    depth_m         DOUBLE PRECISION NOT NULL,
    pressure_bar    DOUBLE PRECISION,
    temperature_c   DOUBLE PRECISION,
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_depth_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_depth_device_ts ON depth_data(device_id, ts DESC);

-- =============================================================================
-- 5. INS / IMU Data (Inertial Navigation System)
-- =============================================================================
-- Stores full 6-DOF inertial measurement + attitude angles.
CREATE TABLE IF NOT EXISTS ins_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    -- Attitude (Euler angles, degrees)
    roll_deg        DOUBLE PRECISION,
    pitch_deg       DOUBLE PRECISION,
    yaw_deg         DOUBLE PRECISION,
    -- Acceleration (m/s^2)
    accel_x         DOUBLE PRECISION,
    accel_y         DOUBLE PRECISION,
    accel_z         DOUBLE PRECISION,
    -- Angular velocity (deg/s)
    gyro_x          DOUBLE PRECISION,
    gyro_y          DOUBLE PRECISION,
    gyro_z          DOUBLE PRECISION,
    -- Magnetometer (uT)
    mag_x           DOUBLE PRECISION,
    mag_y           DOUBLE PRECISION,
    mag_z           DOUBLE PRECISION,
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_ins_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_ins_device_ts ON ins_data(device_id, ts DESC);

-- =============================================================================
-- 6. DVL (Doppler Velocity Log) Data
-- =============================================================================
-- Stores bottom-track and water-track velocity, plus DVL depth measurement.
CREATE TABLE IF NOT EXISTS dvl_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    -- Bottom-track velocity (m/s)
    vx_bt           DOUBLE PRECISION,
    vy_bt           DOUBLE PRECISION,
    vz_bt           DOUBLE PRECISION,
    -- Water-track velocity (m/s)
    vx_wt           DOUBLE PRECISION,
    vy_wt           DOUBLE PRECISION,
    vz_wt           DOUBLE PRECISION,
    -- DVL measured altitude above bottom (m)
    altitude_m      DOUBLE PRECISION,
    -- DVL measured depth (m)
    depth_m         DOUBLE PRECISION,
    -- Speed over ground (m/s)
    speed_og        DOUBLE PRECISION,
    -- Beam data validity
    beam_valid      INTEGER DEFAULT 0,
    temperature_c   DOUBLE PRECISION,
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_dvl_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_dvl_device_ts ON dvl_data(device_id, ts DESC);

-- =============================================================================
-- 7. Ocean Current Data (from DVL water profiling)
-- =============================================================================
-- Records current velocity at different depth layers.
-- Linked to GPS position for spatial correlation.
CREATE TABLE IF NOT EXISTS ocean_current_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    depth_layer_m   DOUBLE PRECISION NOT NULL,
    current_speed   DOUBLE PRECISION NOT NULL,
    current_dir_deg DOUBLE PRECISION NOT NULL,
    current_vx      DOUBLE PRECISION,
    current_vy      DOUBLE PRECISION,
    current_vz      DOUBLE PRECISION,
    temperature_c   DOUBLE PRECISION,
    salinity_psu    DOUBLE PRECISION,
    -- GPS reference for spatial correlation
    latitude        DOUBLE PRECISION,
    longitude       DOUBLE PRECISION,
    geom            GEOMETRY(Point, 4326),
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_current_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_current_device_ts ON ocean_current_data(device_id, ts DESC);
CREATE INDEX IF NOT EXISTS idx_current_depth ON ocean_current_data(depth_layer_m);
CREATE INDEX IF NOT EXISTS idx_current_geom ON ocean_current_data USING GIST(geom);

-- Auto-populate PostGIS geometry from lat/lon on insert
CREATE OR REPLACE FUNCTION current_update_geom()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.latitude IS NOT NULL AND NEW.longitude IS NOT NULL THEN
        NEW.geom := ST_SetSRID(ST_MakePoint(NEW.longitude, NEW.latitude), 4326);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_current_update_geom ON ocean_current_data;
CREATE TRIGGER trg_current_update_geom
    BEFORE INSERT OR UPDATE ON ocean_current_data
    FOR EACH ROW EXECUTE FUNCTION current_update_geom();

-- =============================================================================
-- 8. Wind Field Data
-- =============================================================================
-- Surface wind measurements linked to GPS position.
CREATE TABLE IF NOT EXISTS wind_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    wind_speed_ms   DOUBLE PRECISION NOT NULL,
    wind_dir_deg    DOUBLE PRECISION NOT NULL,
    gust_speed_ms   DOUBLE PRECISION,
    temperature_c   DOUBLE PRECISION,
    humidity_pct    DOUBLE PRECISION,
    pressure_hpa    DOUBLE PRECISION,
    -- GPS reference for spatial correlation
    latitude        DOUBLE PRECISION,
    longitude       DOUBLE PRECISION,
    geom            GEOMETRY(Point, 4326),
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_wind_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_wind_device_ts ON wind_data(device_id, ts DESC);
CREATE INDEX IF NOT EXISTS idx_wind_geom ON wind_data USING GIST(geom);

-- Auto-populate PostGIS geometry from lat/lon on insert
CREATE OR REPLACE FUNCTION wind_update_geom()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.latitude IS NOT NULL AND NEW.longitude IS NOT NULL THEN
        NEW.geom := ST_SetSRID(ST_MakePoint(NEW.longitude, NEW.latitude), 4326);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_wind_update_geom ON wind_data;
CREATE TRIGGER trg_wind_update_geom
    BEFORE INSERT OR UPDATE ON wind_data
    FOR EACH ROW EXECUTE FUNCTION wind_update_geom();

-- =============================================================================
-- 9. Battery / Power System Data
-- =============================================================================
CREATE TABLE IF NOT EXISTS battery_data (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    soc_pct         DOUBLE PRECISION,
    voltage_volts   DOUBLE PRECISION,
    current_amps    DOUBLE PRECISION,
    power_watts     DOUBLE PRECISION,
    temperature_c   DOUBLE PRECISION,
    charging        BOOLEAN DEFAULT FALSE,
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_battery_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_battery_device_ts ON battery_data(device_id, ts DESC);

-- =============================================================================
-- 10. Controller Events / System Log
-- =============================================================================
CREATE TABLE IF NOT EXISTS controller_events (
    id              BIGSERIAL PRIMARY KEY,
    device_id       TEXT NOT NULL,
    ts              TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    event_type      TEXT NOT NULL,
    sensor_id       TEXT,
    sensor_name     TEXT,
    value           DOUBLE PRECISION,
    unit            TEXT,
    metadata        JSONB DEFAULT '{}',
    CONSTRAINT fk_event_device FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_events_device_ts ON controller_events(device_id, ts DESC);
CREATE INDEX IF NOT EXISTS idx_events_type ON controller_events(event_type);

-- =============================================================================
-- 11. Legacy sensor_events table (backward compatibility)
-- =============================================================================
CREATE TABLE IF NOT EXISTS sensor_events (
    id          SERIAL PRIMARY KEY,
    sensor_id   TEXT,
    sensor_name TEXT,
    value       DOUBLE PRECISION,
    unit        TEXT,
    timestamp   TIMESTAMPTZ,
    event_type  TEXT,
    metadata    JSONB,
    created_at  TIMESTAMPTZ DEFAULT NOW()
);

-- =============================================================================
-- 12. Useful Views
-- =============================================================================

-- Latest position per device
CREATE OR REPLACE VIEW v_device_latest_position AS
SELECT DISTINCT ON (device_id)
    device_id,
    ts,
    latitude,
    longitude,
    altitude,
    speed_knots,
    course_deg,
    geom
FROM gps_data
ORDER BY device_id, ts DESC;

-- Latest depth per device
CREATE OR REPLACE VIEW v_device_latest_depth AS
SELECT DISTINCT ON (device_id)
    device_id,
    ts,
    depth_m,
    pressure_bar,
    temperature_c
FROM depth_data
ORDER BY device_id, ts DESC;

-- Current profile at a specific location (join current data with GPS)
CREATE OR REPLACE VIEW v_current_profile AS
SELECT
    oc.device_id,
    oc.ts,
    oc.depth_layer_m,
    oc.current_speed,
    oc.current_dir_deg,
    oc.temperature_c,
    oc.salinity_psu,
    oc.latitude,
    oc.longitude
FROM ocean_current_data oc
ORDER BY oc.device_id, oc.ts DESC, oc.depth_layer_m ASC;

-- =============================================================================
-- 13. Insert default device record (will be updated at runtime)
-- =============================================================================
INSERT INTO devices (device_id, device_name, device_type, description)
VALUES ('uuv-001', 'UUV Alpha', 'UUV', 'Default UUV device')
ON CONFLICT (device_id) DO NOTHING;
