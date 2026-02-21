<p align="center">
  <img src="./assets/corekv.png" alt="CoreKV Logo" width="500">
</p>

<h1 align="center">CoreKV</h1>

<p align="center">A lightweight, high-performance key-value store written in C++</p>

---

## Requirements

- **C++ Compiler**: GCC 9+ or Clang 10+ (C++17 support)
- **CMake**: 3.31+
- **Make** or **Ninja**

---

## Setup

```bash
git clone https://github.com/Rajeshpatel07/corekv.git
cd corekv
```

---

## Build

### Production
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Debug/Dev
```bash
cmake -B build
cmake --build build
```

---


### Run server
```bash
./build/bin/corekv-server
```
### Run client
```bash
./build/bin/corekv-client
```

Listens on port **8000**.

---

## Project Structure

```
src/
├── core/
├── net/
├── protocol/
├── handler/
├─ server.cpp
└─ client.cpp
```
