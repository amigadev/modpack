cmake_minimum_required(VERSION 3.5.1)
project(modpack, LANGUAGES C)

add_executable(modpack src/main.c src/protracker.c src/player61a.c src/log.c src/buffer.c src/options.c)