# CMake generated Testfile for 
# Source directory: /home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests
# Build directory: /home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(nbd-qemu "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests/qemu-test.sh" "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/tests/nbd-test")
set_tests_properties(nbd-qemu PROPERTIES  WORKING_DIRECTORY "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/tests" _BACKTRACE_TRIPLES "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests/CMakeLists.txt;7;add_test;/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests/CMakeLists.txt;0;")
add_test(nbd-client "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests/client-test.sh" "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/tests/nbd-test" "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/tests/nbd-client-test")
set_tests_properties(nbd-client PROPERTIES  WORKING_DIRECTORY "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/tests" _BACKTRACE_TRIPLES "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests/CMakeLists.txt;14;add_test;/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/tests/CMakeLists.txt;0;")
