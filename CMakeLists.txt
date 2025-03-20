cmake_minimum_required(VERSION 3.12...3.31)
project(ris CXX)
set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadUserver)

#find_package(userver COMPONENTS core QUIET)
if(NOT userver_FOUND)
  # Tries TRY_DIR first, falls back to downloading userver from GitHub using CPM.
  download_userver(TRY_DIR third_party/userver)
endif()

userver_setup_environment()

file(GLOB_RECURSE SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/src/dto/*.yaml)
userver_target_generate_chaotic(${PROJECT_NAME}-chgen
        ARGS
        # Map '/components/schemas/*' JSONSchema types to C++ types in 'dto::' namespace
        -n "/components/schemas/([^/]*)/=dto::{0}"
        # Generate serializers for responses
        --generate-serializers
        OUTPUT_DIR
        ${CMAKE_CURRENT_BINARY_DIR}
        SCHEMAS
        ${SCHEMAS}
        RELATIVE_TO
        ${CMAKE_CURRENT_SOURCE_DIR}
)


add_library(${PROJECT_NAME}_task OBJECT
        src/task/Md5PartSolver.cpp
)

add_library(${PROJECT_NAME}_worker OBJECT
        src/worker/BackgroundTaskProcessor.cpp
        src/worker/WorkerHandlers.cpp
)

add_library(${PROJECT_NAME}_manager OBJECT
        src/manager/ManagerState.cpp
        src/manager/WorkerState.cpp
        src/manager/ManagerHandlers.cpp
        src/manager/ManagerHandlers.h
)

target_link_libraries(${PROJECT_NAME}_task PUBLIC userver::core)

target_link_libraries(${PROJECT_NAME}_worker PUBLIC userver::core ${PROJECT_NAME}-chgen  )
target_include_directories(${PROJECT_NAME}_worker PUBLIC src)

target_link_libraries(${PROJECT_NAME}_manager PUBLIC userver::core ${PROJECT_NAME}-chgen  )
target_include_directories(${PROJECT_NAME}_manager PUBLIC src)


add_executable(worker_service src/worker/main.cpp)
target_link_libraries(worker_service PRIVATE ${PROJECT_NAME}_worker ${PROJECT_NAME}_task)

add_executable(manager_service src/manager/main.cpp)
target_link_libraries(manager_service PRIVATE ${PROJECT_NAME}_manager ${PROJECT_NAME}_task)

# Unit Tests
add_executable(${PROJECT_NAME}_unittest
        src/tests/md5_task.cpp
)
target_include_directories(${PROJECT_NAME}_unittest PUBLIC src)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE   ${PROJECT_NAME}_task userver::utest)
add_google_tests(${PROJECT_NAME}_unittest)


# Functional testing
#userver_testsuite_add_simple()

# Install
include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
  message(STATUS "Set install prefix: $ENV{PREFIX}")
  file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
  set(CMAKE_INSTALL_PREFIX "${PREFIX_PATH}")
endif()

#file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

#install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PROJECT_NAME})
#install(FILES ${CONFIGS_FILES} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME} COMPONENT ${PROJECT_NAME})
