# DO NOT CHANGE THE SYMBOLIC VARIABLE `SOURCES`.
# You can, however, change the list of files that comprise this variable.

include_directories(include)
set(SOURCES
        src/main.cpp
        src/PerfectLinks.cpp
        src/PackageProcessor.cpp
        src/IdGenerator.cpp
        src/ProcessLogger.cpp
        src/Socket.cpp
        src/DeliveryTracker.cpp
        src/FIFOBroadcast.cpp
        src/FIFODeliveryTracker.cpp
        src/DestinationMap.cpp
)

# DO NOT EDIT THE FOLLOWING LINES
find_package(Threads)
add_executable(da_proc ${SOURCES})
target_link_libraries(da_proc ${CMAKE_THREAD_LIBS_INIT})
