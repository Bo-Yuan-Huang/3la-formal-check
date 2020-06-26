# ==============================================================================
# MIT License
#
# Copyright (c) 2020 Princeton University
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# ==============================================================================

cmake_minimum_required(VERSION 3.14.0)

# ---------------------------------------------------------------------------- #
# PROJECT
# name version language
# ---------------------------------------------------------------------------- #
project(pffc VERSION 0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------- #
# VARIABLE
# alias and configurations
# ---------------------------------------------------------------------------- #
set(MyTarget ${PROJECT_NAME})

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# ---------------------------------------------------------------------------- #
# External dependencies
# ---------------------------------------------------------------------------- #
include(FetchContent)

##
## ilang
##
find_package(ilang REQUIRED 1.0.6)

##
## flex::flexila
##
FetchContent_Declare(
  flex
  GIT_REPOSITORY git@github.com:PrincetonUniversity/flexnlp-ila.git
  GIT_TAG        master
)

##
## relay::relayila
##
FetchContent_Declare(
  relay
  GIT_REPOSITORY https://github.com/PrincetonUniversity/relay-ila
  GIT_TAG        master
)

##
## fmt::fmt
##
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        6.2.1
)

##
## csv
##
FetchContent_Declare(
  csv
  GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
  GIT_TAG        1.3.3
)

FetchContent_MakeAvailable(flex relay fmt csv)

# ---------------------------------------------------------------------------- #
# TARGET
# executable
# ---------------------------------------------------------------------------- #
add_executable(${MyTarget} 
  app/main.cc
  src/ischecker.cc
)

target_include_directories(${MyTarget} PRIVATE include)
target_include_directories(${MyTarget} PRIVATE ${csv_SOURCE_DIR}/include)

target_link_libraries(${MyTarget} PRIVATE ilang::ilang)
target_link_libraries(${MyTarget} PRIVATE fmt::fmt)
target_link_libraries(${MyTarget} PRIVATE csv)

target_link_libraries(${MyTarget} PRIVATE flex::flexila)
target_link_libraries(${MyTarget} PRIVATE relay::relayila)

