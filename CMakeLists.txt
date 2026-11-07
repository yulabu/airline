cmake_minimum_required(VERSION 3.20)
project(airline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 中文字符串在 MSVC 下必须显式指定 UTF-8，否则答辩机器上会乱码
if (MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ─────────────────────────────────────────────────────────────
# 核心算法层（零第三方依赖）
#   base / ds / domain / repo / service
#   这个 target 刻意不链接任何外部库，是"算法自包含"的构建级证据
# ─────────────────────────────────────────────────────────────
add_library(airline_core STATIC
    src/base/status.cpp
    src/base/date.cpp
    src/domain/types.cpp
    src/domain/route.cpp
    src/domain/seat_layout.cpp
    src/domain/policies/avail.cpp
    src/domain/policies/config_check.cpp
    src/domain/policies/seat_alloc.cpp
    src/domain/policies/waitlist_match.cpp
    src/domain/policies/checkin_plan.cpp
    src/repo/repository.cpp
    src/service/query.cpp
    src/service/booking.cpp
    src/service/admin.cpp
    src/service/seed.cpp
)
target_include_directories(airline_core PUBLIC src third_party)

# ─────────────────────────────────────────────────────────────
# 持久化与 HTTP 适配层（唯一的第三方依赖边界）
# ─────────────────────────────────────────────────────────────
add_library(airline_api STATIC
    src/store/store.cpp
    src/api/server.cpp
)
target_link_libraries(airline_api PUBLIC airline_core)
target_include_directories(airline_api PUBLIC third_party)

if (WIN32)
    target_link_libraries(airline_api PUBLIC ws2_32)
elseif(APPLE)
    target_link_libraries(airline_api PUBLIC "-framework CoreFoundation" "-framework CFNetwork")
endif()

# ─────────────────────────────────────────────────────────────
# 可执行程序
# ─────────────────────────────────────────────────────────────
add_executable(airline
    src/app/main.cpp
    src/app/student_info.cpp
)
target_link_libraries(airline PRIVATE airline_api)

# ─────────────────────────────────────────────────────────────
# 单元测试（只链 core，不链任何第三方库）
# ─────────────────────────────────────────────────────────────
add_executable(airline_tests
    tests/test_main.cpp
    tests/test_service.cpp
)
target_link_libraries(airline_tests PRIVATE airline_core)

enable_testing()
add_test(NAME core_tests COMMAND airline_tests)
