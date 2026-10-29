# Host-side tests against the Arduino stand-in in mock/, built once per transfer path.
#
#   make -C test
#
# -Werror here and not in a sketch build: the library is compiled with the same flags as the tests,
# so a new warning fails the tests rather than waiting for someone to read the compiler output.

CXX ?= g++
CXXFLAGS = -std=gnu++11 -Wall -Wextra -Werror -g
CPPFLAGS = -Imock -I..

SOURCES = MaxMatrixTest.cpp mock/Arduino.cpp ../MaxMatrix.cpp
HEADERS = ../MaxMatrix.h ../StandardTypes.h mock/Arduino.h mock/SPI.h mock/binary.h

.PHONY: test clean

test: build/test-bitbang build/test-spi
	./build/test-bitbang
	./build/test-spi

build/test-bitbang: $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $(SOURCES)

build/test-spi: $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DMAXMATRIX_USE_SPI=STD_ON -o $@ $(SOURCES)

clean:
	rm -rf build
