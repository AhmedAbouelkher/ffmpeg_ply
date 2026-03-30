build: clean
	gcc -o player src/*.c $$(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale libswresample SDL2)
	@echo "Player built successfully"


build-brew: clean	
	gcc -o player src/*.c $$(/opt/homebrew/bin/pkg-config --cflags --libs libavformat libavcodec libavutil libswscale libswresample SDL2)
	@echo "Player built successfully"

clean:
	rm -f player
	@echo "Player cleaned successfully"