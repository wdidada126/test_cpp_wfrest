# test_cpp_wfrest

A minimal RESTful HTTP server built with [wfrest](https://github.com/WFerns/wfrest) (C++17).

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
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
