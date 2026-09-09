# GTA IV is a 32-bit game: i686 mingw. IV-SDK is header-only (single TU, see
# sdk/ivsdk/README-mingw.md); -lversion is for GetFileVersionInfo (game
# version detection). --exclude-all-symbols: the .asi exports nothing, and
# mingw's auto-export chokes on the asm-labelled hook symbols.
CXX      = i686-w64-mingw32-g++
CXXFLAGS = -O2 -std=c++17 -static -static-libgcc -static-libstdc++ -shared -w \
           -Wl,--exclude-all-symbols -Isdk/ivsdk -Isrc

mod_iv.asi: src/main.cpp src/vehicles_iv.h src/weapons_iv.h
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp -lversion

clean:
	rm -f mod_iv.asi

.PHONY: clean
