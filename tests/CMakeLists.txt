nbd_sketch(nbd-test nbd-test/nbd-test.ino)
nbd_sketch(nbd-client-test nbd-client-test/nbd-client-test.ino)

find_program(QEMU_IMG qemu-img)
find_program(QEMU_IO qemu-io)
if(QEMU_IMG AND QEMU_IO)
    add_test(NAME nbd-qemu
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/qemu-test.sh $<TARGET_FILE:nbd-test>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
else()
    message(STATUS "qemu-img/qemu-io not found: nbd-qemu test disabled")
endif()

add_test(NAME nbd-client
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/client-test.sh
            $<TARGET_FILE:nbd-test> $<TARGET_FILE:nbd-client-test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# FatIOBlockDevice test: needs the TinyFATFS sources from
# https://github.com/pschatzmann/TinyFATFS. A local copy can be selected with
# -DNBD_TINYFATFS_DIR=/path/to/TinyFATFS; otherwise the sources are fetched.
if(NOT NBD_TINYFATFS_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../TinyFATFS/src/driver/IO.h")
        set(NBD_TINYFATFS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../TinyFATFS")
    else()
        include(FetchContent)
        FetchContent_Declare(tinyfatfs GIT_REPOSITORY "https://github.com/pschatzmann/TinyFATFS.git" GIT_TAG main)
        FetchContent_GetProperties(tinyfatfs)
        if(NOT tinyfatfs_POPULATED)
            FetchContent_Populate(tinyfatfs)
        endif()
        set(NBD_TINYFATFS_DIR "${tinyfatfs_SOURCE_DIR}")
    endif()
endif()
set(NBD_TINYFATFS_DIR "${NBD_TINYFATFS_DIR}" CACHE PATH
    "Path to the TinyFATFS library (contains src/driver/IO.h)" FORCE)
if(EXISTS "${NBD_TINYFATFS_DIR}/src/driver/IO.h")
    add_executable(fatio-test fatio-test/fatio-test.cpp)
    target_link_libraries(fatio-test PRIVATE arduino-nbd)
    target_include_directories(fatio-test PRIVATE "${NBD_TINYFATFS_DIR}/src")
    target_compile_options(fatio-test PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
    add_test(NAME fatio-block-device COMMAND fatio-test)
else()
    message(STATUS "TinyFATFS not found (${NBD_TINYFATFS_DIR}): fatio-block-device test disabled")
endif()
