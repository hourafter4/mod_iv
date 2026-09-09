# GTA IV is 32-bit, so i686. Two .asi files come out of the same src/main.cpp,
# differing only in which backend it is built against (see src/backend.h):
#
#   mod_iv.asi     IV-SDK, fixed addresses for GTAIV.exe 1.0.7.0 / 1.0.8.0
#   mod_iv_ce.asi  pattern scanning, Complete Edition (1.2.0.x) and older
#
# -lversion: GetFileVersionInfo, IV-SDK's game version check.
# --exclude-all-symbols: an .asi exports nothing, and mingw's auto-export trips
# over the asm-labelled hook symbols in IV-SDK's Hooks.h.
# -DNDEBUG: the pattern scanner asserts on a missing pattern; mod_iv checks for
# that itself (src/backend_ce.h) and degrades instead of aborting.
CXX      = i686-w64-mingw32-g++
CXXFLAGS = -O2 -std=c++17 -static -static-libgcc -static-libstdc++ -shared -w \
           -DNDEBUG -Wl,--exclude-all-symbols -Isrc
SHARED   = src/main.cpp src/backend.h src/vehicles_iv.h src/weapons_iv.h

all: mod_iv.asi mod_iv_ce.asi

mod_iv.asi: $(SHARED) src/backend_ivsdk.h
	$(CXX) $(CXXFLAGS) -Isdk/ivsdk -o $@ src/main.cpp -lversion

mod_iv_ce.asi: $(SHARED) src/backend_ce.h sdk/patterns/Patterns.cpp sdk/patterns/Patterns.hh
	$(CXX) $(CXXFLAGS) -DMOD_IV_CE=1 -Isdk/ivsdk -Isdk/patterns \
	    -o $@ src/main.cpp sdk/patterns/Patterns.cpp

clean:
	rm -f mod_iv.asi mod_iv_ce.asi

.PHONY: all clean
