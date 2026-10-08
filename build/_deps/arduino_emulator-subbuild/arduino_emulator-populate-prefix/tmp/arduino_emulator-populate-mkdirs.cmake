# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-src"
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-build"
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix"
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix/tmp"
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix/src/arduino_emulator-populate-stamp"
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix/src"
  "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix/src/arduino_emulator-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix/src/arduino_emulator-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/pschatzmann/Development/Arduino/libraries/arduino-mbd/build/_deps/arduino_emulator-subbuild/arduino_emulator-populate-prefix/src/arduino_emulator-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
