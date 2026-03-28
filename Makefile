build:
	gcc -o player main.c $$(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale SDL3)
	@echo "Player built successfully"

clean:
	rm -f player
	@echo "Player cleaned successfully"

build_sdl:
	gcc -o sdl_program sdl_program.c $$(pkg-config --cflags --libs SDL3)
	@echo "SDL program built successfully"

clean_sdl:
	rm -f sdl_program
	@echo "SDL program cleaned successfully"