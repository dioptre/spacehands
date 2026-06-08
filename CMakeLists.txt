cmake_minimum_required(VERSION 3.18)
project(instrument CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------- Platform detection --------------------------------------------------
if(APPLE)
    add_compile_definitions(PLATFORM_MAC USE_MOCK_CAMERA)
    message(STATUS "Platform: macOS (mock camera)")
else()
    add_compile_definitions(PLATFORM_LINUX)
    message(STATUS "Platform: Linux/Pi")
endif()

# ---------- Dependencies --------------------------------------------------------
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio dnn)
find_package(Threads REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION} — DNN backend used for hand detection")

# liblo — required on Pi, optional on Mac
find_package(LibLo QUIET)
if(LibLo_FOUND)
    message(STATUS "liblo found — OSC enabled")
    add_compile_definitions(HAVE_LIBLO)
else()
    message(STATUS "liblo not found — OSC disabled (stub mode)")
endif()

if(NOT APPLE)
    find_package(ArducamDepthCamera REQUIRED)
endif()

# uSockets stub — uWebSockets headers are vendored but uSockets source
# is not needed since we use cpp-httplib for all HTTP/SSE serving.
add_library(uSockets INTERFACE)
target_include_directories(uSockets INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uSockets"
)

# ---------- Sources -------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/camera/MockSource.cpp
    src/vision/HandDetector.cpp
    src/vision/GestureClassifier.cpp
    src/vision/DepthEstimator.cpp
    src/game/GameState.cpp
    src/game/InstrumentMapper.cpp
    src/game/InstrumentPool.cpp
    src/audio/OscSender.cpp
    src/server/HttpServer.cpp
    src/server/WsServer.cpp
    src/server/MjpegStreamer.cpp
    src/util/Config.cpp
)

if(NOT APPLE)
    list(APPEND SOURCES src/camera/ArducamSource.cpp)
endif()

add_executable(instrument ${SOURCES})

target_include_directories(instrument PRIVATE
    src
    third_party
    third_party/uWebSockets/src
    third_party/uSockets/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(instrument PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
    uSockets
)
if(LibLo_FOUND)
    target_link_libraries(instrument PRIVATE ${LibLo_LIBRARIES})
    target_include_directories(instrument PRIVATE ${LibLo_INCLUDE_DIRS})
endif()

if(NOT APPLE)
    target_link_libraries(instrument PRIVATE ArducamDepthCamera::ArducamDepthCamera)
endif()


# ---------- Assets copy ---------------------------------------------------------
add_custom_command(TARGET instrument POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:instrument>/assets"
    COMMENT "Copying assets"
)

# ---------- macOS camera entitlement signing ------------------------------------
if(APPLE)
    add_custom_command(TARGET instrument POST_BUILD
        COMMAND codesign --force --sign - --deep
            --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/entitlements.plist"
            --identifier "com.apple.Terminal"
            "$<TARGET_FILE:instrument>"
        COMMENT "Signing with camera entitlement"
    )
endif()
