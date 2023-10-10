# SPDX-FileCopyrightText: 2023 A3 Audio UG (haftungsbeschränkt) <contact@a3-audio.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.11)

set(PROJECT "a3-motion-ui")
project(${PROJECT} VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig)
find_package(JUCE CONFIG REQUIRED)

add_subdirectory("src/a3-motion-engine")
add_subdirectory("src/a3-motion-ui")

set(TESTS_ENABLED TRUE CACHE BOOL "enable unit testing with GoogleTest")
if(TESTS_ENABLED)
add_subdirectory("src/a3-motion-tests")
endif()
