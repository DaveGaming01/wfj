cmake_minimum_required(VERSION 3.16)
project(dcsswiftbus CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(LIBEVENT REQUIRED libevent)

add_executable(dcsswiftbus
    src/main.cpp
    src/service.cpp
    src/traffic.cpp
    src/udplistener.cpp
    src/dbus/dbusconnection.cpp
    src/dbus/dbusdispatcher.cpp
    src/dbus/dbuserror.cpp
    src/dbus/dbusmessage.cpp
    src/dbus/dbusobject.cpp
    src/dbus/dbusserver.cpp
)
target_include_directories(dcsswiftbus PRIVATE src ${DBUS_INCLUDE_DIRS} ${LIBEVENT_INCLUDE_DIRS})
target_link_libraries(dcsswiftbus ${DBUS_LIBRARIES} ${LIBEVENT_LIBRARIES})
if(WIN32)
    target_link_libraries(dcsswiftbus ws2_32)
endif()

# Test client simulating the swift pilot client's FGSwiftBus connection sequence
add_executable(mock_swift_client test/mock_swift_client.cpp)
target_include_directories(mock_swift_client PRIVATE ${DBUS_INCLUDE_DIRS})
target_link_libraries(mock_swift_client ${DBUS_LIBRARIES})
