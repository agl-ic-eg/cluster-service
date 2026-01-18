# Cluster Service and API - User Guide

**For Application Developers Using the Cluster Service API**

Version 1.0 | AGL Instrument Cluster Expert Group

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Getting Started](#getting-started)
4. [API Reference](#api-reference)
5. [Advanced Topics](#advanced-topics)
6. [Support and Resources](#support-and-resources)
---

## Overview

The **cluster-service** provides a reusable IPC mechanism for automotive instrument cluster applications running on Automotive Grade Linux (AGL). It enables applications to receive real-time vehicle data such as speed, RPM, warning indicators (telltales), trip computer information, and more.

This software focuses to base software, not a final software. User can use reference implementation for demo. User should change for own real usage.

### Usage

### The command line options

```
--help       Print help strings.

--sound      Enable alarm sound.

--demo       Enable demo signals.  If this feature is enabled at build-time.

--can=NAME   Enable socket can feature (NAME=can if name, ex. can0).  If this feature is enabled at build-time.
```

### Key Features

- **Real-time broadcast system**: Server pushes updates to all connected clients
- **Low latency**: 16ms update interval with data smoothing
- **Thread-safe**: Protected data access with pthread mutexes
- **Change notifications**: Register callbacks for specific signal changes
- **Multiple data sources**: Supports demo data generator and SocketCAN input. Plugin at build-time.
- **Comprehensive telltales**: 50+ warning/indicator signals
- **Alarm sound**: Play builtin sound if it necessary. Plugin at build-time.

### Dependency

- **Linux**
- **libsystemd**
- **libalsa**

---

## Architecture

### Communication Model

The cluster-service uses a **push-based broadcast architecture**:

```
┌────────────────────────────────────────────────────────┐
│                    CLUSTER SERVER                      │
│  ┌────────────┐      ┌──────────────┐                  │
│  │ Data       │─────▶│ Server Data  │                  │
│  │ Sources    │      │ Pool (Global)│                  │
│  │(CAN/Demo)  │      └──────┬───────┘                  │
│  └────────────┘             │                          │
│                             │                          │
│                   ┌─────────▼─────────┐                │
│                   │ Timer (16ms)      │                │
│                   │ + Smoothing       │                │
│                   └─────────┬─────────┘                │
│                             │                          │
│                             ▼                          │
│                   ┌──────────────────┐                 │
│                   │ Broadcast to ALL │                 │
│                   │ Clients          │                 │
│                   └─────────┬────────┘                 │
└─────────────────────────────┼──────────────────────────┘
                              │
                              │ SOCK_SEQPACKET
                              │ (Abstract Unix Socket)
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  CLIENT A     │     │  CLIENT B     │     │  CLIENT C     │
│ ┌───────────┐ │     │ ┌───────────┐ │     │ ┌───────────┐ │
│ │ Local     │ │     │ │ Local     │ │     │ │ Local     │ │
│ │ Data Pool │ │     │ │ Data Pool │ │     │ │ Data Pool │ │
│ └─────┬─────┘ │     │ └─────┬─────┘ │     │ └─────┬─────┘ │
│       │       │     │       │       │     │       │       │
│       ▼       │     │       ▼       │     │       ▼       │
│ ┌───────────┐ │     │ ┌───────────┐ │     │ ┌───────────┐ │
│ │ Callbacks │ │     │ │ Callbacks │ │     │ │ Callbacks │ │
│ └───────────┘ │     │ └───────────┘ │     │ └───────────┘ │
│       │       │     │       │       │     │       │       │
│       ▼       │     │       ▼       │     │       ▼       │
│  Application  │     │  Application  │     │  Application  │
└───────────────┘     └───────────────┘     └───────────────┘
```

### Key Architectural Facts

1. **One-Way Communication**: Clients ONLY receive data; they NEVER send to the server
2. **SOCK_SEQPACKET**: Uses sequenced packet sockets (not stream sockets)
3. **Abstract Socket**: Linux abstract socket namespace (no filesystem path)
4. **Two Data Pools**: 
   - One global data pool in the server process
   - Separate local data pool in each client process
5. **Broadcast Model**: Server timer fires every 16ms and broadcasts identical packets to ALL connected clients
6. **Event-Driven**: Clients use sd-event to receive broadcasts asynchronously

### Data Flow

1. **Data Sources** (demo generator or CAN) write to the server's global data pool
2. **Timer fires** every 16ms in the server
3. **Server reads** data from its data pool and applies smoothing (for speed/tacho)
4. **Server broadcasts** the same packet to ALL client sockets
5. **Client event loop** wakes up (EPOLLIN event)
6. **Client receives** packet via `data_pool_receive()`
7. **Client updates** its LOCAL data pool copy via `data_pool_set_v1()`
8. **Change detection** triggers registered callbacks
9. **Application calls** getter functions like `getTurnR()` which read from the local data pool

---

## Getting Started

### Installation

#### 1. Build and Install the Cluster Service

```bash
# Clone the repository
git clone https://github.com/agl-ic-eg/cluster-service.git
cd cluster-service

# Generate configure script and configure with demo data source
./autogen.sh --enable-fake-data-source

# Build
make

# Install (may require sudo)
sudo make install

# Update library cache
sudo ldconfig
```

#### 2. Start the Cluster Service

```bash
# Run the cluster service daemon
cluster-service &
```

### Minimal Client Application

example/cluster-client.c


### Configure options

```
--enable-fake-data-source : Enable fake data source (default is no)
--enable-socketcan-data-source : Enable socketcan data source (default is no)
```

---

## API Reference

### Initialization and Cleanup in libsystemd binding.

#### `data_pool_client_setup_sdevent()`

Connect to the cluster service and integrate with sd-event loop.

```c
#include <cluster-api-sdevent.h>

int data_pool_client_setup_sdevent(
    sd_event *event,
    data_pool_client_handle_sdevent *handle
);
```

**Parameters:**
- `event`: sd-event loop handle (must not be NULL)
- `handle`: Pointer to receive client handle (must not be NULL)

**Returns:**
- `0`: Success
- `-1`: Internal error (socket creation failed, connection failed, etc.)
- `-2`: Argument error (NULL parameters)

**Important Notes:**
- This function internally calls `clusterInit()` - do NOT call it manually
- Creates a SOCK_SEQPACKET socket
- Connects to the abstract Unix socket (no filesystem path)
- Registers socket with the event loop for EPOLLIN events
- **You MUST run the event loop** after calling this function to receive data

#### `data_pool_client_cleanup_sdevent()`

Disconnect from the cluster service and free resources.

```c
int data_pool_client_cleanup_sdevent(
    data_pool_client_handle_sdevent handle
);
```

### Reading Data

Refer to API document at AGL Confluence.

https://lf-automotivelinux.atlassian.net/wiki/spaces/IC/pages/17991785/IC-Service+API

---

## Advanced Topics

### Thread Safety

- **Data pool**: Thread-safe (uses pthread mutexes)
- **Getter functions**: Thread-safe (read-only access to locked data)
- **Callback registration**: NOT thread-safe (register before starting threads)
- **sd-event**: NOT thread-safe (run event loop in single thread)

### Performance Characteristics

- **Update rate**: 16ms
- **Latency**: Typically < 50ms from source to getter
- **Smoothing**: Speed and tachometer values are smoothed over 5-10 samples
- **Memory**: ~1KB per client for local data pool

---

## Support and Resources

### Community

- **AGL Instrument Cluster Expert Group**: https://wiki.automotivelinux.org/ic
- **GitHub**: https://github.com/agl-ic-eg/cluster-service
- **Mailing List**: automotive-discuss@lists.automotivelinux.org

---

## License

The cluster-service is licensed under the Apache License 2.0.

Copyright (c) 2021, Nippon Seiki Co., Ltd.

Copyright (C) 2024 Automotive Grade Linux.

---
**End of User Guide**
