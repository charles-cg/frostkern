#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -c ../FrostKern_UART_Receiver/app_uart_protocol.c -o "$build_dir/codec.o"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Istubs test_receiver.cpp "$build_dir/codec.o" -o "$build_dir/test"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/test"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DFROSTKERN_WIFI_ENABLED=1 -Istubs test_receiver.cpp "$build_dir/codec.o" -o "$build_dir/test_wifi"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/test_wifi"
