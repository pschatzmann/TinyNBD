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
