


all: main.c fontx.c ili9340.c
	cc -Wall  -o main.elf main.c fontx.c ili9340.c -lwiringPi -lm -DWPI -lcurl -ljson-c

clean: 
	rm main.elf
