# test_cpp_wfrest

A minimal RESTful HTTP server built with [wfrest](https://github.com/wfrest/wfrest) (C++17).

## Prerequisites

wfrest is not packaged, so build it (it brings the `workflow` submodule) and install it to `/usr/local`:

```bash
git clone --recurse-submodules https://github.com/wfrest/wfrest.git
make -C wfrest -j$(nproc)
sudo make -C wfrest install   # headers + lib + wfrest-targets.cmake -> /usr/local
```

## Build

```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DWFREST_SOURCE_DIR=/path/to/wfrest   # source tree holding workflow/_include and workflow/_lib
cmake --build build --parallel $(nproc)
```

## Run

```bash
./build/wfrest_server [port]   # default port 8080
```

## Endpoints

| Method | Path   | Description          |
|--------|--------|----------------------|
| GET    | /ping  | returns `pong`       |
| GET    | /json  | returns JSON greeting |
| POST   | /echo  | echoes request body  |

## CI

GitHub Actions workflow at `.github/workflows/ci.yml` builds and smoke-tests on Ubuntu (C++17 & C++20).
