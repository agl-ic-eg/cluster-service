# Cluster Service - Developer Guide

**For Developers Modifying the Cluster Service Itself**

Version 1.0 | AGL Instrument Cluster Expert Group

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Deep Dive](#architecture-deep-dive)
3. [Component Details](#component-details)
4. [Build System](#build-system)
5. [Data Flow](#data-flow)
6. [Extending the Service](#extending-the-service)
7. [Testing](#testing)
8. [Debugging](#debugging)
9. [Performance Tuning](#performance-tuning)
10. [Contributing](#contributing)

---

## Overview

The cluster-service is a broadcast-based IPC service for automotive instrument cluster data. This guide is for developers who want to modify, extend, or debug the service itself.

### Key Design Principles

1. **One-Way Broadcast**: Server pushes to all clients; clients never send
2. **Low Latency**: Target 16ms update cycle as default
3. **Thread-Safe**: Protected by pthread mutexes
4. **Event-Driven**: sd-event for async I/O
5. **Fail-Safe**: Graceful handling of client disconnections

### Repository Structure

```
cluster-service/
├── include/              # Public API headers
│   ├── cluster_api.h           # Main API header
│   ├── cluster_api_*.h         # Category-specific headers
│   └── ipc_protocol.h          # IPC packet definitions
├── lib/                  # Client library implementation
│   ├── cluster-api-core.c      # Core initialization
│   ├── cluster-api-sdevent.c   # sd-event integration
│   ├── data-pool.c             # Data pool (compiled into both)
│   ├── data-pool-client.c      # Client-side receive logic
│   └── data-pool-static-configurator.c
├── src/                  # Server implementation
│   ├── cluster-service.c       # Main server entry point
│   ├── data-pool-service.c     # Server broadcast logic
│   ├── cluster-service-util.c  # Signal handling utilities
│   └── source/                 # Data source reference implementations
│       ├── fake-data/          # Demo data generator
│       └── socket-can/         # CAN bus reader
├── test/                 # Unit tests (gtest/gmock)
├── example/              # Example client application
└── tools/                # Build and test scripts
```

---

## Architecture Deep Dive

### Communication Model

```
┌───────────────────────────────────────────────────────────────┐
│                    SERVER PROCESS                             │
│                                                               │
│  ┌──────────────────┐                                         │
│  │ Data Source      │  Writes to global data_pool             │
│  │ (CAN/Demo)       │                                         │
│  └────────┬─────────┘                                         │
│           │                                                   │
│           ▼                                                   │
│  ┌──────────────────┐                                         │
│  │ Global Data Pool │  (data-pool.c)                          │
│  │ (g_agl_cluster_  │  Thread-safe access via pthread_mutex   │
│  │  data_pool)      │                                         │
│  └────────┬─────────┘                                         │
│           │                                                   │
│           │  Timer fires every 16ms                           │
│           ▼                                                   │
│  ┌──────────────────┐                                         │
│  │data_pool_get_v1()│  Read data from pool                    │
│  └────────┬─────────┘                                         │
│           │                                                   │
│           ▼                                                   │
│  ┌──────────────────┐                                         │
│  │ Smoothing Logic  │  Average speed/tacho over N samples     │
│  └────────┬─────────┘                                         │
│           │                                                   │
│           ▼                                                   │
│  ┌──────────────────┐                                         │
│  │ Build Packet     │  Add header (magic, version, seqnum)    │
│  └────────┬─────────┘                                         │
│           │                                                   │
│           ▼                                                   │
│  ┌──────────────────┐                                         │
│  │ Write to ALL     │  Loop through client session list       │
│  │ Client Sockets   │  write() to each fd                     │
│  └──────────────────┘                                         │
│           │                                                   │
│           │  SOCK_SEQPACKET                                   │
│           │  Abstract Unix Socket                             │
│           │  (no filesystem path)                             │
└───────────┼───────────────────────────────────────────────────┘
            │
            │  Each client receives identical packet
            │
    ┌───────┴───────┬─────────────┬────────────┐
    │               │             │            │
    ▼               ▼             ▼            ▼
┌────────┐      ┌────────┐   ┌────────┐   ┌────────┐
│CLIENT 1│      │CLIENT 2│   │CLIENT 3│   │CLIENT N│
└────────┘      └────────┘   └────────┘   └────────┘
    │               │             │            │
    │  EPOLLIN      │             │            │
    ▼               ▼             ▼            ▼
┌────────────────────────────────────────────────────┐
│ data_pool_sessions_handler() triggered             │
│   → data_pool_receive(fd)                          │
│     → read(fd, &packet)                            │
│     → data_pool_set_v1(&packet.data)               │
│       → memcpy to local g_agl_cluster_data_pool    │
│       → data_pool_check_change_v1()                │
│       → call registered callbacks                  │
└────────────────────────────────────────────────────┘
    │
    ▼
┌────────────────────────────────────────────────────┐
│ Application calls getters                          │
│   → Read from LOCAL data pool                      │
│   → Protected by pthread_mutex                     │
└────────────────────────────────────────────────────┘
```

### Socket Details

**Socket Type**: `SOCK_SEQPACKET`
- Sequenced, reliable, two-way connection-based data transmission
- Preserves message boundaries (unlike SOCK_STREAM)
- Each `read()` returns exactly one packet
- No partial reads or buffering issues

**Socket Namespace**: Abstract Unix Domain Socket
- Format: `"\0/agl/agl-cluster-service"` (first byte is NUL)
- Lives in abstract namespace (not filesystem)
- No file permissions to manage
- Automatically cleaned up when server exits
- See: It can configured at g_config.socket_name in `data-pool-static-configurator.c`

**Socket Options**:
- `SOCK_NONBLOCK`: Non-blocking I/O
- `SOCK_CLOEXEC`: Close on exec (security)

### Data Pool Architecture

**Critical Fact**: The file `data-pool.c` is compiled into **BOTH** server and client:

```
Server Process Memory:               Client Process Memory:
┌─────────────────────┐             ┌─────────────────────┐
│ libcluster-server   │             │ libcluster-api.so   │
│                     │             │                     │
│ data-pool.c         │             │ data-pool.c         │
│   ↓                 │             │   ↓                 │
│ g_agl_cluster_      │             │ g_agl_cluster_      │
│  data_pool          │             │  data_pool          │
│   (GLOBAL DATA)     │             │   (LOCAL COPY)      │
└─────────────────────┘             └─────────────────────┘
```

**Why Two Copies?**
- **Server**: Writable by data sources, read by broadcast timer
- **Client**: Updated by incoming packets, read by getter functions
- **Benefit**: No shared memory complexity, process isolation
- **Tradeoff**: Memory overhead (~1KB per client)

---

## Component Details

### 1. Server Entry Point: `src/cluster-service.c`

**Responsibilities**:
- Parse command-line arguments
- Setup signal handlers (SIGTERM, SIGINT)
- Initialize sd-event loop
- Setup data pool service
- Setup data source (fake or CAN)
- Run event loop
- Cleanup on shutdown

**Key Functions**:

```c
int main(int argc, char *argv[])
{
    sd_event *event = NULL;
    data_pool_service_handle handle = NULL;

    // Create event loop
    sd_event_default(&event);

    // Setup signal handling
    signal_setup(event);

    // Setup data pool service (creates listening socket)
    data_pool_service_setup(event, &handle);

    // Setup data source
#ifdef DATA_SOURCE_FAKE
    demo_data_setup(event);
#endif
#ifdef DATA_SOURCE_SOCKET_CAN
    socketcan_receiver_setup(event);
#endif

    // Run until signal
    sd_event_loop(event);

    // Cleanup
    data_pool_service_cleanup(handle);
    sd_event_unref(event);
}
```

### 2. Server Broadcast Logic: `src/data-pool-service.c`

**Core Structure**:

```c
struct s_data_pool_service {
    sd_event *parent_eventloop;
    sd_event_source *socket_evsource;         // Listening socket
    struct s_data_pool_notification_timer *notification_timer;
    struct s_data_pool_session *session_list; // Linked list of clients
    struct s_smoothing_barrel_uint32 speed_barrel;
    struct s_smoothing_barrel_uint32 tacho_barrel;
};

struct s_data_pool_session {
    struct s_data_pool_session *next;
    sd_event_source *socket_evsource;         // Client connection
};
```

**Broadcast Flow**:

```c
// Timer callback (every 40ms)
static int data_pool_notification_timer_handler(...)
{
    return data_pool_message_passanger(dp);
}

// Broadcast to all clients
static int data_pool_message_passanger(data_pool_service_handle dp)
{
    AGLCLUSTER_SERVICE_PACKET service_packet;

    // Increment sequence number
    g_packet_header.seqnum++;

    // Read from global data pool
    data_pool_get_v1(&service_packet.data);

    // Apply smoothing
    service_packet.data.spAnalogVal = 
        smoothing_barrel_operation_uint32(&dp->speed_barrel, 
                                          service_packet.data.spAnalogVal);
    service_packet.data.taAnalogVal = 
        smoothing_barrel_operation_uint32(&dp->tacho_barrel, 
                                          service_packet.data.taAnalogVal);

    // Add header
    service_packet.header.magic = AGLCLUSTER_SERVICE_PACKETHEADER_MAGIC;
    service_packet.header.version = AGLCLUSTER_SERVICE_PACKET_VERSION_V1;
    service_packet.header.seqnum = g_packet_header.seqnum;

    // Write to all clients
    for (each client in dp->session_list) {
        fd = sd_event_source_get_io_fd(client->socket_evsource);
        write(fd, &service_packet, sizeof(service_packet));
    }
}
```

**Client Connection Handling**:

```c
// Accept new client
static int data_pool_incoming_handler(sd_event_source *event, int fd, 
                                       uint32_t revents, void *userdata)
{
    // Accept connection
    sessionfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);

    // Create session structure
    session = malloc(sizeof(struct s_data_pool_session));

    // Add to sd-event for disconnect detection
    sd_event_add_io(dp->parent_eventloop, &session->socket_evsource,
                    sessionfd, EPOLLIN | EPOLLHUP | EPOLLERR,
                    data_pool_sessions_handler, dp);

    // Add to session list
    session->next = dp->session_list;
    dp->session_list = session;
}

// Handle client disconnect
static int data_pool_sessions_handler(sd_event_source *event, int fd,
                                       uint32_t revents, void *userdata)
{
    if (revents & (EPOLLHUP | EPOLLERR)) {
        // Remove from session list
        // Unref event source (automatically closes fd)
        sd_event_source_disable_unref(listp->socket_evsource);
        free(listp);
    }
}
```

### 3. Data Pool: `lib/data-pool.c`

**Global State**:

```c
struct s_agl_cluster_data_pool {
    change_notify_function_t notify;         // Callback function
    pthread_mutex_t data_pool_lock;          // Thread safety
    struct S_AGLCLUSTER_SERVICE_DATA_V1 data; // Actual data
};

static struct s_agl_cluster_data_pool g_agl_cluster_data_pool = {
    .notify = NULL,
    .data_pool_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    .data = { /* initial values */ }
};
```

**Key Functions**:

```c
// Set data (used by server data sources and client receive)
int data_pool_set_v1(struct S_AGLCLUSTER_SERVICE_DATA_V1 *data)
{
    if (data_pool_lock()) {
        signals = data_pool_check_change_v1(data);  // Detect changes
        memcpy(&g_agl_cluster_data_pool.data, data, 
               sizeof(g_agl_cluster_data_pool.data));
        data_pool_unlock();

        // Trigger callbacks
        if (g_agl_cluster_data_pool.notify != NULL && signals != 0)
            g_agl_cluster_data_pool.notify(signals);
    }
}

// Get data (used by server broadcast and client getters)
int data_pool_get_v1(struct S_AGLCLUSTER_SERVICE_DATA_V1 *data)
{
    if (data_pool_lock()) {
        memcpy(data, &g_agl_cluster_data_pool.data, 
               sizeof(struct S_AGLCLUSTER_SERVICE_DATA_V1));
        data_pool_unlock();
    }
}

// Change detection
static uint64_t data_pool_check_change_v1(struct S_AGLCLUSTER_SERVICE_DATA_V1 *data)
{
    uint64_t signals = 0;

    // Compare each field
    if (g_agl_cluster_data_pool.data.turnR != data->turnR)
        signals |= IC_HMI_TT_TURN_R;
    if (g_agl_cluster_data_pool.data.turnL != data->turnL)
        signals |= IC_HMI_TT_TURN_L;
    // ... for all fields

    return signals;  // Bitmask of changed signals
}
```

**Thread Safety**:
- All access protected by `pthread_mutex_lock()`
- Mutex type: `PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP`
- Lock order: Always lock before read/write, unlock immediately after
- No nested locking

### 4. Client Receive: `lib/data-pool-client.c`

**Receive Handler**:

```c
int data_pool_receive(int fd)
{
    AGLCLUSTER_SERVICE_PACKET packet;
    ssize_t sret;

    // Read exactly one packet (SOCK_SEQPACKET guarantees this)
    sret = read(fd, &packet, sizeof(packet));
    if (sret == sizeof(packet)) {
        // Validate packet
        if (data_pool_packet_check_v1(&packet.header) == true) {
            // Update local data pool
            data_pool_set_v1(&packet.data);
        }
    }
}

static bool data_pool_packet_check_v1(struct S_AGLCLUSTER_SERVICE_PACKETHEADER_V1 *header)
{
    return (header->magic == AGLCLUSTER_SERVICE_PACKETHEADER_MAGIC) &&
           (header->version == AGLCLUSTER_SERVICE_PACKET_VERSION_V1);
}
```

### 5. Client Setup: `lib/cluster-api-sdevent.c`

**Connection Establishment**:

```c
int data_pool_client_setup_sdevent(sd_event *event, 
                                    data_pool_client_handle_sdevent *handle)
{
    // Create client socket
    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, AF_UNIX);

    // Connect to abstract socket
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    sasize = get_data_pool_service_socket_name(name.sun_path, 
                                                sizeof(name.sun_path));
    connect(fd, (const struct sockaddr *)&name, sasize + sizeof(sa_family_t));

    // Register with event loop
    sd_event_add_io(event, &socket_source, fd, EPOLLIN, 
                    data_pool_sessions_handler, dp);

    // Auto-close fd when event source is freed
    sd_event_source_set_io_fd_own(socket_source, 1);

    // Initialize data pool
    clusterInit();

    *handle = dp;
}
```

**Event Handler**:

```c
static int data_pool_sessions_handler(sd_event_source *event, int fd,
                                       uint32_t revents, void *userdata)
{
    if (revents & (EPOLLHUP | EPOLLERR)) {
        // Server disconnected
        dp->socket_evsource = sd_event_source_disable_unref(dp->socket_evsource);
    } else if (revents & EPOLLIN) {
        // Data available
        data_pool_receive(fd);
    }
}
```

### 6. Data Sources

#### Fake Data Source: `src/source/fake-data/demo-data-generator.c`

**Purpose**: Generate synthetic vehicle data for testing/demo.

**Implementation**:

```c
// Timer-based data generation
static int demo_data_generator_handler(sd_event_source *es, uint64_t usec, 
                                        void *userdata)
{
    static uint32_t speed = 0;
    static uint32_t rpm = 0;

    // Update values
    speed = (speed + 100) % 20000;  // 0-200 km/h
    rpm = (rpm + 1000) % 800000;    // 0-8000 RPM

    // Write to server's data pool
    setSpAnalogVal(speed);
    setTaAnalogVal(rpm);

    // Reschedule timer
    g_demo_timer.timerval += g_demo_data_update_interval;
    sd_event_source_set_time(es, g_demo_timer.timerval);
}
```

#### SocketCAN Source: `src/source/socket-can/socketcan-receiver.c`

**Purpose**: Read real CAN bus data.

**Implementation**:

```c
// CAN frame handler
static int socketcan_receiver_handler(sd_event_source *es, int fd,
                                       uint32_t revents, void *userdata)
{
    struct can_frame frame;
    ssize_t nbytes;

    // Read CAN frame
    nbytes = read(fd, &frame, sizeof(frame));

    // Parse based on CAN ID
    switch (frame.can_id) {
        case 0x123:  // Speed CAN ID
            speed = parse_speed(&frame);
            setSpAnalogVal(speed);
            break;
        case 0x456:  // RPM CAN ID
            rpm = parse_rpm(&frame);
            setTaAnalogVal(rpm);
            break;
        // ... more CAN IDs
    }
}
```

### 7. IPC Protocol: `include/ipc_protocol.h`

**Packet Structure**:

```c
#define AGLCLUSTER_SERVICE_PACKETHEADER_MAGIC (0xbdacffeeu)
#define AGLCLUSTER_SERVICE_PACKET_VERSION_V1 (1)

struct __attribute__((packed)) S_AGLCLUSTER_SERVICE_PACKETHEADER_V1 {
    uint32_t magic;      // 0xbdacffee (packet validation)
    uint32_t version;    // 1 (protocol version)
    uint64_t seqnum;     // Sequence number (detect lost packets)
};

struct __attribute__((packed)) S_AGLCLUSTER_SERVICE_DATA_V1 {
    // Telltales (50+ fields)
    int32_t turnR;
    int32_t turnL;
    // ...

    // Speed & Tacho
    uint32_t spAnalogVal;
    uint32_t taAnalogVal;

    // Trip computer (20+ fields)
    uint64_t trcomTripAVal;
    // ...
};

typedef struct __attribute__((packed)) {
    struct S_AGLCLUSTER_SERVICE_PACKETHEADER_V1 header;
    struct S_AGLCLUSTER_SERVICE_DATA_V1 data;
} AGLCLUSTER_SERVICE_PACKET;
```

**Packet Size**: ~400 bytes (actual size depends on field count)

**Why `__attribute__((packed))`?**
- Ensures no padding between fields
- Consistent layout across architectures
- Binary compatibility between processes

---

## Build System

### Autotools Structure

```
configure.ac          # Autoconf configuration
Makefile.am           # Top-level makefile
lib/Makefile.am       # Client library
src/Makefile.am       # Server binary
test/Makefile.am      # Unit tests
```

### Build Options

#### Data Sources

```bash
# Demo data (for testing)
./configure --enable-fake-data-source

# Real CAN bus
./configure --enable-socketcan-data-source

# Both (mutually exclusive at runtime, selected by command-line args)
./configure --enable-fake-data-source --enable-socketcan-data-source
```

**Implementation**:
```autoconf
# configure.ac
AC_ARG_ENABLE([fake-data-source],
  [AS_HELP_STRING([--enable-fake-data-source], 
                  [Enable fake data source (default is no)])],
  [:],
  [enable_fake_data_source=no])
AM_CONDITIONAL([DATA_SOURCE_FAKE], [test "$enable_fake_data_source" = "yes"])
```

```makefile
# src/Makefile.am
if DATA_SOURCE_FAKE
cluster_service_SOURCES += source/fake-data/demo-data-generator.c
cluster_service_CFLAGS += -DDATA_SOURCE_FAKE
endif
```

#### Debug Options

```bash
# Address sanitizer (memory errors)
./configure --enable-address-sanitizer

# Code coverage
./configure --enable-gcov

# Unit tests
./configure --enable-test
```

#### Conditional Compilation

```c
// In source code
#ifdef DATA_SOURCE_FAKE
    demo_data_setup(event);
#endif

#ifdef DATA_SOURCE_SOCKET_CAN
    socketcan_receiver_setup(event);
#endif
```

### Dependencies

**Required**:
- `libsystemd` - Event loop, socket activation
- `libalsa` - Alarm sounds
- `pthread` - Mutexes

**Optional** (for testing):
- `gtest` - Google Test framework
- `gmock` - Google Mock framework

**Check in configure.ac**:

```autoconf
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd])
PKG_CHECK_MODULES([LIBALSA], [alsa])
PKG_CHECK_MODULES([GTEST_MAIN], [gtest_main], , enable_test=no)
PKG_CHECK_MODULES([GMOCK_MAIN], [gmock_main], , enable_test=no)
```

### Build Workflow

```bash
# 1. Generate configure script
./autogen.sh

# 2. Configure build
./configure --enable-fake-data-source --enable-test

# 3. Build
make -j$(nproc)

# 4. Run tests
make check

# 5. Install
sudo make install
sudo ldconfig

# 6. Uninstall
sudo make uninstall
```

### Output Artifacts

```
# Libraries
/usr/local/lib/libcluster-api.so         # Client library
/usr/local/lib/libcluster-api.a          # Static library (if enabled)

# Headers
/usr/local/include/cluster_api.h
/usr/local/include/cluster_api_*.h
/usr/local/include/cluster-api-sdevent.h

# Binaries
/usr/local/bin/cluster-service           # Server executable

# pkg-config
/usr/local/lib/pkgconfig/cluster_api.pc
/usr/local/lib/pkgconfig/cluster-api-systemd.pc
```

---

## Data Flow

### Detailed Sequence Diagram

```
Time →

Server Data Source     Server Data Pool      Server Timer         Server Broadcast        Client Socket       Client Data Pool      Application
      │                       │                    │                       │                     │                    │                  │
      │ setSpAnalogVal(100)   │                    │                       │                     │                    │                  │
      │──────────────────────>│                    │                       │                     │                    │                  │
      │                       │ data_pool_set_v1() │                       │                     │                    │                  │
      │                       │ (lock, memcpy,     │                       │                     │                    │                  │
      │                       │  unlock)           │                       │                     │                    │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │ Timer fires (40ms)    │                     │                    │                  │
      │                       │                    │<──────────────────────│                     │                    │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │<───────────────────│ data_pool_get_v1()    │                     │                    │                  │
      │                       │                    │ (lock, memcpy,        │                     │                    │                  │
      │                       │                    │  unlock)              │                     │                    │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │ Apply smoothing       │                     │                    │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │ Build packet          │                     │                    │                  │
      │                       │                    │ (magic, version,      │                     │                    │                  │
      │                       │                    │  seqnum)              │                     │                    │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │ write(packet)       │                    │                  │
      │                       │                    │                       │────────────────────>│                    │                  │
      │                       │                    │                       │                     │ EPOLLIN event      │                  │
      │                       │                    │                       │                     │───────────────────>│                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │ data_pool_receive()│                  │
      │                       │                    │                       │                     │ read(fd, &packet)  │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │ Validate packet    │                  │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │                    │ data_pool_set_v1()│
      │                       │                    │                       │                     │                    │<─────────────────│
      │                       │                    │                       │                     │                    │ (lock, memcpy,   │
      │                       │                    │                       │                     │                    │  unlock)         │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │                    │ Check changes    │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │                    │ Call callbacks   │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │                    │                  │ getSpAnalogVal()
      │                       │                    │                       │                     │                    │                  │<────────────────
      │                       │                    │                       │                     │                    │ data_pool_get_v1()│
      │                       │                    │                       │                     │                    │<─────────────────│
      │                       │                    │                       │                     │                    │ (lock, memcpy,   │
      │                       │                    │                       │                     │                    │  unlock)         │
      │                       │                    │                       │                     │                    │                  │
      │                       │                    │                       │                     │                    │ Return value     │
      │                       │                    │                       │                     │                    │─────────────────>│
```

### Critical Timing

1. **Data Source Update**: Varies (CAN bus frequency, demo timer)
2. **Server Broadcast**: Every 40ms (hardcoded in `data_pool_service_setup()`)
3. **Client Wakeup**: Immediate (EPOLLIN event)
4. **Client Processing**: < 1ms (read, validate, memcpy)
5. **End-to-End Latency**: Typically 40-50ms (worst case: next broadcast cycle)

### Smoothing Algorithm

**Purpose**: Reduce jitter in speed and tachometer displays.

**Implementation**:

```c
struct s_smoothing_barrel_uint32 {
    uint32_t *barrel;      // Circular buffer
    size_t barrel_num;     // Buffer size (e.g., 10 samples)
    size_t barrel_wp;      // Write pointer
};

static uint32_t smoothing_barrel_operation_uint32(
    struct s_smoothing_barrel_uint32 *pbarrel, uint32_t push_value)
{
    // Advance write pointer (circular)
    pbarrel->barrel_wp = (pbarrel->barrel_wp + 1) % pbarrel->barrel_num;

    // Store new value
    pbarrel->barrel[pbarrel->barrel_wp] = push_value;

    // Calculate average
    uint64_t sum = 0;
    for (size_t i = 0; i < pbarrel->barrel_num; i++) {
        sum += pbarrel->barrel[i];
    }

    return (uint32_t)(sum / pbarrel->barrel_num);
}
```

**Configuration**:
- Default: 10 samples (400ms window at 40ms update rate)
- Adjustable in `data_pool_service_setup()`:

```c
dp->speed_barrel.barrel_num = 10;
dp->speed_barrel.barrel = calloc(10, sizeof(uint32_t));

dp->tacho_barrel.barrel_num = 10;
dp->tacho_barrel.barrel = calloc(10, sizeof(uint32_t));
```

---

## Extending the Service

### Adding a New Data Field

#### Step 1: Update IPC Protocol

Edit `include/ipc_protocol.h`:

```c
struct __attribute__((packed)) S_AGLCLUSTER_SERVICE_DATA_V1 {
    // Existing fields...
    uint32_t taAnalogVal;

    // NEW FIELD
    int32_t myNewIndicator;  // Add at end to maintain compatibility

    // Trip computer fields...
    uint32_t trcomTripAVal;
};
```

#### Step 2: Add Public API Header

Create or edit `include/cluster_api_myfeature.h`:

```c
#ifndef CLUSTER_API_MYFEATURE_H
#define CLUSTER_API_MYFEATURE_H

#include <stdint.h>

// Getter function
IC_HMI_ON_OFF getMyNewIndicator(void);

// If you need a signal flag for notifications:
#define IC_HMI_TT_MY_NEW_INDICATOR 0x0000001000000000ULL

#endif
```

Include in `include/cluster_api.h`:

```c
#include <cluster_api_registernotify.h>
#include <cluster_api_myfeature.h>  // NEW
```

#### Step 3: Implement Getter in data-pool.c

Edit `lib/data-pool.c`:

```c
// Getter implementation (auto-generated pattern)
IC_HMI_ON_OFF getMyNewIndicator(void)
{
    IC_HMI_ON_OFF ret = IC_HMI_OFF;

    if (data_pool_lock() == true) {
        ret = (IC_HMI_ON_OFF)g_agl_cluster_data_pool.data.myNewIndicator;
        (void)data_pool_unlock();
    }

    return ret;
}
```

#### Step 4: Update Change Detection

Edit `lib/data-pool.c`, find `data_pool_check_change_v1()`:

```c
uint64_t data_pool_check_change_v1(struct S_AGLCLUSTER_SERVICE_DATA_V1 *data)
{
    uint64_t signals = 0;

    // Existing checks...
    if (g_agl_cluster_data_pool.data.turnR != data->turnR)
        signals |= IC_HMI_TT_TURN_R;

    // NEW CHECK
    if (g_agl_cluster_data_pool.data.myNewIndicator != data->myNewIndicator)
        signals |= IC_HMI_TT_MY_NEW_INDICATOR;

    return signals;
}
```

#### Step 5: Add Data Source Setter (if needed)

If your data source needs to write this field:

```c
// In data-pool.c
void setMyNewIndicator(int32_t value)
{
    if (data_pool_lock() == true) {
        g_agl_cluster_data_pool.data.myNewIndicator = value;
        (void)data_pool_unlock();
    }
}
```

#### Step 6: Update Data Source

Edit `src/source/fake-data/demo-data-generator.c` or CAN source:

```c
static int demo_data_generator_handler(...)
{
    static int toggle = 0;

    // Existing data generation...

    // NEW DATA
    toggle = (toggle + 1) % 2;
    setMyNewIndicator(toggle ? IC_HMI_ON : IC_HMI_OFF);

    return 0;
}
```

#### Step 7: Rebuild and Test

```bash
make clean
make
sudo make install
sudo ldconfig

# Test with example client
./example/cluster-client
```

### Adding a New Data Source

#### Option 1: New Timer-Based Source

Similar to fake-data source:

1. Create `src/source/my-source/my-source.c`
2. Implement timer handler:

```c
static int my_source_handler(sd_event_source *es, uint64_t usec, void *userdata)
{
    // Read data from your source (file, network, hardware, etc.)
    uint32_t speed = read_speed_from_source();

    // Write to data pool
    setSpAnalogVal(speed);

    // Reschedule timer
    // ...
}

int my_source_setup(sd_event *event)
{
    // Setup timer
    sd_event_add_time(event, &timer, CLOCK_MONOTONIC, ...);
}
```

3. Add to `Makefile.am` with conditional
4. Call from `src/cluster-service.c`

#### Option 2: New Event-Based Source

Similar to SocketCAN source:

1. Create socket/file descriptor source
2. Register with sd-event for EPOLLIN
3. Read data in handler
4. Write to data pool

```c
int my_source_setup(sd_event *event)
{
    int fd = open_my_device();

    sd_event_add_io(event, &source, fd, EPOLLIN, my_source_handler, NULL);
}

static int my_source_handler(sd_event_source *es, int fd, 
                              uint32_t revents, void *userdata)
{
    // Read from fd
    read(fd, &buffer, size);

    // Parse and write to data pool
    setSpAnalogVal(parsed_speed);
}
```

### Changing Broadcast Frequency

Edit `src/data-pool-service.c`:

```c
// Find this constant (currently ~40ms)
const uint64_t g_data_pool_notification_timer_interval = 40 * 1000; // usec

// Change to desired value (e.g., 20ms for 50Hz)
const uint64_t g_data_pool_notification_timer_interval = 20 * 1000;
```

**Tradeoffs**:
- **Faster** (< 40ms): More CPU usage, more network traffic, lower latency
- **Slower** (> 40ms): Less CPU usage, less traffic, higher latency
- **Recommendation**: 20-50ms range for instrument cluster use cases

---

## Testing

### Unit Tests

Located in `test/` directory, using Google Test framework.

**Test Categories**:

1. **Data Pool Tests** (`data-pool-service-test-*.cpp`):
   - Socket creation
   - Client connection
   - Packet validation
   - Timer operation
   - Data pool initialization

2. **API Tests** (`cluster-api-test-*.cpp`):
   - Getter functions
   - Change notifications
   - Thread safety

3. **Utility Tests** (`cluster-service-util-test.cpp`):
   - Signal handling
   - Configuration parsing

**Running Tests**:

```bash
# Build with tests enabled
./configure --enable-test
make

# Run all tests
make check

# Run specific test
./test/cluster-api-test-telltale

# Run with verbose output
./test/cluster-api-test-telltale --gtest_print_time=1 --gtest_color=yes
```

**Example Test**:

```cpp
// test/data-pool-client-test.cpp
TEST(DataPoolClient, BasicReceive)
{
    AGLCLUSTER_SERVICE_PACKET packet;

    // Setup packet
    packet.header.magic = AGLCLUSTER_SERVICE_PACKETHEADER_MAGIC;
    packet.header.version = AGLCLUSTER_SERVICE_PACKET_VERSION_V1;
    packet.data.turnR = IC_HMI_ON;

    // Mock socket
    int mock_fd = create_mock_socket();
    write(mock_fd, &packet, sizeof(packet));

    // Test receive
    int ret = data_pool_receive(mock_fd);
    EXPECT_EQ(ret, 0);

    // Verify data was stored
    EXPECT_EQ(getTurnR(), IC_HMI_ON);
}
```

### Integration Testing

**Manual Test Scenarios**:

1. **Basic Connectivity**:
   ```bash
   # Terminal 1: Start server
   cluster-service --fake-data
   
   # Terminal 2: Run client
   ./example/cluster-client
   
   # Verify: Client prints changing speed values
   ```

2. **Multiple Clients**:
   ```bash
   # Start server
   cluster-service --fake-data &
   
   # Start 5 clients
   for i in {1..5}; do
       ./example/cluster-client &
   done
   
   # Verify: All clients receive data
   # Check: ps aux | grep cluster-client
   ```

3. **Client Disconnect**:
   ```bash
   # Start server
   cluster-service --fake-data &
   
   # Start client
   ./example/cluster-client &
   CLIENT_PID=$!
   
   # Kill client
   kill $CLIENT_PID
   
   # Verify: Server continues running
   # Check: ps aux | grep cluster-service
   ```

4. **Server Restart**:
   ```bash
   # Start server
   cluster-service --fake-data &
   SERVER_PID=$!
   
   # Start client
   ./example/cluster-client &
   
   # Kill server
   kill $SERVER_PID
   
   # Client should detect disconnect
   # Restart server
   cluster-service --fake-data &
   
   # Client needs manual reconnect (by design)
   ```

### Performance Testing

**Latency Test**:

```c
// Add timestamps to packet header
struct S_AGLCLUSTER_SERVICE_PACKETHEADER_V1 {
    uint32_t magic;
    uint32_t version;
    uint64_t seqnum;
    uint64_t timestamp_usec;  // NEW
};

// Server: Add timestamp
ret = sd_event_now(event, CLOCK_MONOTONIC, &timestamp);
service_packet.header.timestamp_usec = timestamp;

// Client: Measure latency
uint64_t now;
sd_event_now(event, CLOCK_MONOTONIC, &now);
uint64_t latency = now - packet.header.timestamp_usec;
printf("Latency: %lu usec\n", latency);
```

**Throughput Test**:

```bash
# Monitor network traffic
watch -n1 'ss -x | grep cluster | wc -l'

# Monitor packet rate
# Add counter in server: packets_sent++
# Print every second: packets_sent / elapsed_time
```

---

## Debugging

### Enable Debug Logging

Add debug prints (controlled by compile flag):

```c
// In source files
#ifdef DEBUG_PRINT
#define DPRINTF(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

// Use in code
DPRINTF("Received packet: seqnum=%lu", packet.header.seqnum);
```

Compile with:
```bash
./configure CFLAGS="-DDEBUG_PRINT -g -O0"
make
```

### GDB Debugging

**Server**:
```bash
gdb ./cluster-service
(gdb) break data_pool_message_passanger
(gdb) run --fake-data
(gdb) bt        # Backtrace
(gdb) print dp  # Inspect variables
```

**Client**:
```bash
gdb ./example/cluster-client
(gdb) break data_pool_receive
(gdb) run
```

**Attach to Running Process**:
```bash
# Find PID
ps aux | grep cluster-service

# Attach
sudo gdb -p <PID>
(gdb) continue
```

### Valgrind

**Memory Leaks**:
```bash
valgrind --leak-check=full --show-leak-kinds=all \
         ./cluster-service --fake-data
```

**Thread Errors**:
```bash
valgrind --tool=helgrind ./cluster-service --fake-data
```

### Address Sanitizer

Build with ASan:
```bash
./configure --enable-address-sanitizer
make

# Run - crashes on memory errors
./cluster-service --fake-data
```

### strace

**System Calls**:
```bash
# Trace all syscalls
strace -f ./cluster-service --fake-data

# Trace only socket operations
strace -e socket,bind,listen,accept,read,write \
       ./cluster-service --fake-data

# Trace with timestamps
strace -tt -f ./cluster-service --fake-data
```

### Inspecting Socket

**List Abstract Sockets**:
```bash
# Show all abstract sockets
ss -x | grep '@'

# Show cluster service socket
ss -x | grep cluster
```

**Monitor Connections**:
```bash
watch -n1 'ss -x | grep cluster'
```

---

## Performance Tuning

### Broadcast Frequency

**Current**: 40ms (25 Hz)

**Adjustment**:
```c
// src/data-pool-service.c
const uint64_t g_data_pool_notification_timer_interval = 40 * 1000; // usec

// Change to:
// 20ms = 50 Hz (smoother, more CPU)
// 50ms = 20 Hz (less CPU, slight latency)
// 16.67ms = 60 Hz (match display refresh)
```

### Smoothing Window

**Current**: 10 samples

**Adjustment**:
```c
// src/data-pool-service.c, in data_pool_service_setup()

// Current
dp->speed_barrel.barrel_num = 10;

// Increase for smoother (but more lag)
dp->speed_barrel.barrel_num = 20;

// Decrease for more responsive (but more jitter)
dp->speed_barrel.barrel_num = 5;
```

### Socket Buffer Size

For high-frequency updates:

```c
// In data_pool_service_setup() after socket creation
int sendbuf = 65536;  // 64KB
setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));

// In data_pool_client_setup_sdevent() after socket creation
int recvbuf = 65536;
setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));
```

### CPU Affinity

Pin server to specific CPU core:

```c
// In main(), before event loop
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);  // Pin to core 0
sched_setaffinity(0, sizeof(cpuset), &cpuset);
```

Or use systemd service file:
```ini
[Service]
CPUAffinity=0
```

---

## Contributing

### Code Style

**C Code**:
- Indent: Tabs (8 spaces)
- Braces: K&R style
- Line length: 100 characters
- Comments: `/* */` for multi-line, `//` for single-line

**Check Style**:
```bash
./tools/check-codingstyle.sh
```

**Auto-Format**:
```bash
./tools/do-reformat-codingstyle.sh
```

### Complexity Check

```bash
./tools/check-complexity.sh
```

Limits:
- Cyclomatic complexity: < 15 per function
- Function length: < 200 lines

### Commit Messages

Format:
```
component: short description

Longer explanation if needed.

Fixes: #123
Signed-off-by: Your Name <email@example.com>
```

Example:
```
data-pool: add support for new telltale indicator

Adds getMyNewIndicator() function and IC_HMI_TT_MY_NEW_INDICATOR
signal flag for change notifications.

Signed-off-by: John Doe <john@example.com>
```

### Pull Request Process

1. Fork repository
2. Create feature branch: `git checkout -b feature/my-feature`
3. Make changes
4. Run tests: `make check`
5. Check style: `./tools/check-codingstyle.sh`
6. Commit with sign-off: `git commit -s`
7. Push and create PR

### Review Checklist

- [ ] Code follows style guide
- [ ] All tests pass
- [ ] No new compiler warnings
- [ ] Complexity within limits
- [ ] Documentation updated (if API changed)
- [ ] Commit message properly formatted
- [ ] Signed-off-by present

---

## Appendix A: File Reference

### Server Files

| File | Purpose |
|------|---------|
| `src/cluster-service.c` | Main entry point, event loop setup |
| `src/data-pool-service.c` | Broadcast logic, client management |
| `src/cluster-service-util.c` | Signal handlers, utilities |
| `src/source/fake-data/demo-data-generator.c` | Demo data source |
| `src/source/socket-can/socketcan-receiver.c` | CAN bus source |

### Client Library Files

| File | Purpose |
|------|---------|
| `lib/cluster-api-core.c` | Basic init/term functions |
| `lib/cluster-api-sdevent.c` | sd-event integration |
| `lib/data-pool.c` | Data storage (compiled into both) |
| `lib/data-pool-client.c` | Client receive logic |

### Header Files

| File | Purpose |
|------|---------|
| `include/cluster_api.h` | Main public API |
| `include/cluster_api_telltale.h` | Telltale indicators |
| `include/cluster_api_speed.h` | Speed functions |
| `include/cluster_api_tacho.h` | Tachometer functions |
| `include/cluster_api_tripcomputer.h` | Trip computer functions |
| `include/cluster_api_shiftposition.h` | Gear position functions |
| `include/cluster_api_registernotify.h` | Callback registration |
| `include/ipc_protocol.h` | Packet structures |

---

## Appendix B: Timing Diagrams

### Server Broadcast Timing

```
Time (ms) →
0         40        80        120       160
│         │         │         │         │
├─────────┼─────────┼─────────┼─────────┼─────────>
│         │         │         │         │
Timer     Timer     Timer     Timer     Timer
│         │         │         │         │
Read      Read      Read      Read      Read
Pool      Pool      Pool      Pool      Pool
│         │         │         │         │
Smooth    Smooth    Smooth    Smooth    Smooth
│         │         │         │         │
Build     Build     Build     Build     Build
Packet    Packet    Packet    Packet    Packet
│         │         │         │         │
Write     Write     Write     Write     Write
All       All       All       All       All
Clients   Clients   Clients   Clients   Clients
```

### Client Receive Timing

```
Time (ms) →
0    5    10   15   20        40   45   50
│    │    │    │    │         │    │    │
├────┼────┼────┼────┼─────────┼────┼────┼──>
│                              │
Client                         Packet
Idle                          Arrives
│                              │
│                              EPOLLIN
│                              │
│                              Read
│                              │
│                              Validate
│                              │
│                              Update
│                              Pool
│                              │
│                              Check
│                              Changes
│                              │
│                              Call
│                              Callbacks
│                              │
│                              Done
│                              (< 1ms)
│                              │
Client                         Client
Idle                           Idle
```

---

## Appendix C: Common Issues

### Issue: Server Won't Start

**Symptoms**: `cluster-service` exits immediately

**Checks**:
1. Port already in use: `ss -x | grep cluster`
2. Permission denied: Run as user (not root)
3. Missing dependencies: `ldd cluster-service`

### Issue: Clients Can't Connect

**Symptoms**: `data_pool_client_setup_sdevent()` returns -1

**Checks**:
1. Server running: `ps aux | grep cluster-service`
2. Socket exists: `ss -x | grep cluster`
3. Permissions: Sockets in abstract namespace have no permissions

### Issue: Data Not Updating

**Symptoms**: Getters return stale values

**Checks**:
1. Event loop running: Client must call `sd_event_loop()` or `sd_event_run()`
2. Server broadcasting: Check server logs
3. Data source active: Server must have fake or CAN source enabled

### Issue: Callbacks Not Called

**Symptoms**: `registerIcHmi()` succeeds but callback never executes

**Checks**:
1. Data actually changing: Monitor with direct getters
2. Registration timing: Register after setup, before event loop
3. Callback signature: Must match `IC_HMI_FUNC_NOTIFY_IC_HMI`

---

**End of Developer Guide**
