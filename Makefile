all: ntsc-1 mfm-1 mfm-2

mfm-1: mfm-1.cpp
	g++ -o $@ $<

mfm-2: mfm-2.cpp
	g++ -o $@ $<

ntsc-1: ntsc-1.cpp
	g++ -o $@ $<

clean:
	rm -f ntsc-1 mfm-1 mfm-2

