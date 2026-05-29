cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)

project(bare_addon_exercise C)

add_bare_module(bare_addon_exercise)

target_sources(${bare_addon_exercise} PRIVATE 
  binding.c
)