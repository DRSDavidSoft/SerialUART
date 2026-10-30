# GNU Make entry point; CMake owns platform libraries and installation metadata.
BUILD_DIR ?= build-make
CMAKE ?= cmake
CMAKE_FLAGS ?=
PREFIX ?= /usr/local
.PHONY: all configure test install clean
all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config Release
configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS)
test: all
	ctest --test-dir "$(BUILD_DIR)" -C Release --output-on-failure
install: all
	$(CMAKE) --install "$(BUILD_DIR)" --config Release --prefix "$(PREFIX)"
clean:
	$(CMAKE) --build "$(BUILD_DIR)" --target clean --config Release
