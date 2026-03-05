#!/usr/bin/env bash
# =============================================================================
# Jetson Deployment / Initialization Script
# =============================================================================
# Sets up a fresh Nvidia Jetson (Xavier / Orin) as a UUV main controller.
#
# Usage:
#   sudo bash scripts/jetson_setup.sh
#
# This script:
#   1. Installs required system packages (PostgreSQL 17, PostGIS, build tools)
#   2. Initializes the pilot database with the UUV schema
#   3. Builds the pilot application natively
#   4. Installs a systemd service for auto-start
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
INSTALL_DIR="/opt/pilot"
CONFIG_FILE="$INSTALL_DIR/config/pilot.json"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

# ---------------------------------------------------------------------------
# 1. Check prerequisites
# ---------------------------------------------------------------------------
if [ "$(id -u)" -ne 0 ]; then
    log_error "This script must be run as root (sudo)."
    exit 1
fi

ARCH=$(uname -m)
if [ "$ARCH" != "aarch64" ]; then
    log_warn "Detected architecture: $ARCH (expected aarch64 for Jetson)"
    log_warn "Continuing anyway - the build may fail on non-ARM platforms."
fi

log_info "Starting Pilot UUV platform setup on Jetson ($ARCH)"

# ---------------------------------------------------------------------------
# 2. Install system packages
# ---------------------------------------------------------------------------
log_info "Installing system packages..."

# Add PostgreSQL 17 APT repository
apt-get update -y
apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gnupg \
    lsb-release

# Add PostgreSQL APT repository for version 17
CODENAME=$(lsb_release -cs 2>/dev/null || echo "jammy")
if [ ! -f /etc/apt/sources.list.d/pgdg.list ]; then
    curl -fsSL https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
        gpg --dearmor -o /usr/share/keyrings/postgresql-keyring.gpg
    echo "deb [signed-by=/usr/share/keyrings/postgresql-keyring.gpg] \
        https://apt.postgresql.org/pub/repos/apt ${CODENAME}-pgdg main" \
        > /etc/apt/sources.list.d/pgdg.list
    apt-get update -y
fi

apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    postgresql-17 \
    postgresql-17-postgis-3 \
    postgresql-client-17 \
    libpq-dev

log_info "System packages installed."

# ---------------------------------------------------------------------------
# 3. Configure PostgreSQL
# ---------------------------------------------------------------------------
log_info "Configuring PostgreSQL..."

# Ensure PostgreSQL is running
systemctl enable postgresql
systemctl start postgresql

# Create the pilot database and user
su - postgres -c "psql -tc \"SELECT 1 FROM pg_roles WHERE rolname='pilot'\" | grep -q 1 || \
    psql -c \"CREATE ROLE pilot WITH LOGIN PASSWORD 'pilot';\""

su - postgres -c "psql -tc \"SELECT 1 FROM pg_database WHERE datname='pilot_db'\" | grep -q 1 || \
    psql -c \"CREATE DATABASE pilot_db OWNER pilot;\""

# Grant permissions
su - postgres -c "psql -d pilot_db -c \"GRANT ALL PRIVILEGES ON DATABASE pilot_db TO pilot;\""

log_info "PostgreSQL configured."

# ---------------------------------------------------------------------------
# 4. Initialize database schema
# ---------------------------------------------------------------------------
log_info "Initializing UUV database schema..."

if [ -f "$PROJECT_DIR/sql/000_init.sql" ]; then
    su - postgres -c "psql -d pilot_db -f '$PROJECT_DIR/sql/000_init.sql'" || {
        log_warn "Some schema statements may have failed (tables may already exist)."
    }
    log_info "Database schema initialized."
else
    log_warn "SQL init file not found at $PROJECT_DIR/sql/000_init.sql"
fi

# ---------------------------------------------------------------------------
# 5. Build the pilot application
# ---------------------------------------------------------------------------
log_info "Building pilot application..."

cd "$PROJECT_DIR"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)"

log_info "Build complete."

# ---------------------------------------------------------------------------
# 6. Install application
# ---------------------------------------------------------------------------
log_info "Installing to $INSTALL_DIR..."

mkdir -p "$INSTALL_DIR"/{bin,config,frontend,sql,scripts}
cp build/pilot_server       "$INSTALL_DIR/bin/"
cp -r frontend/*            "$INSTALL_DIR/frontend/"
cp -r config/*              "$INSTALL_DIR/config/"
cp -r sql/*                 "$INSTALL_DIR/sql/"
cp -r scripts/*             "$INSTALL_DIR/scripts/"

# Create default config if it doesn't exist
if [ ! -f "$CONFIG_FILE" ]; then
    cp config/pilot.json "$CONFIG_FILE"
fi

log_info "Application installed to $INSTALL_DIR"

# ---------------------------------------------------------------------------
# 7. Install systemd service
# ---------------------------------------------------------------------------
log_info "Installing systemd service..."

cat > /etc/systemd/system/pilot.service << 'UNIT'
[Unit]
Description=Pilot UUV Data Collection Platform
After=network.target postgresql.service
Wants=postgresql.service

[Service]
Type=simple
User=root
WorkingDirectory=/opt/pilot
ExecStart=/opt/pilot/bin/pilot_server --config /opt/pilot/config/pilot.json
Restart=on-failure
RestartSec=5
Environment=PGHOST=localhost
Environment=PGDATABASE=pilot_db
Environment=PGUSER=pilot
Environment=PGPASSWORD=pilot

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable pilot.service

log_info "Systemd service installed and enabled."

# ---------------------------------------------------------------------------
# 8. Summary
# ---------------------------------------------------------------------------
echo ""
log_info "============================================"
log_info "  Pilot UUV Platform Setup Complete"
log_info "============================================"
echo ""
log_info "  Application:  $INSTALL_DIR/bin/pilot_server"
log_info "  Config:       $INSTALL_DIR/config/pilot.json"
log_info "  Database:     pilot_db (PostgreSQL 17 + PostGIS)"
log_info "  Web UI:       http://localhost:8080"
echo ""
log_info "  Start service:  sudo systemctl start pilot"
log_info "  View logs:      sudo journalctl -u pilot -f"
log_info "  Edit config:    sudo nano $CONFIG_FILE"
echo ""
