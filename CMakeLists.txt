set(TARGET_APP aws_iot_wrapper)

# ==== tìm các file cpp triển khai trong inc/*.h ====
# file(GLOB_RECURSE SRC_FILES
#     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
# )

add_executable(${TARGET_APP}
    main.cpp 
    # ${SRC_FILES}
)

target_include_directories(${TARGET_APP}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/core
        ${CMAKE_CURRENT_SOURCE_DIR}/include/transport
        ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/include/interface
        ${CMAKE_CURRENT_SOURCE_DIR}/include/retry
)

find_package(aws-crt-cpp REQUIRED)
find_package(IotShadow-cpp REQUIRED) 

target_link_libraries(${TARGET_APP} PRIVATE AWS::aws-crt-cpp AWS::IotShadow-cpp )

