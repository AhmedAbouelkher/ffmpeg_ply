build:
	gcc -o player src/*.c $$(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale libswresample SDL2)
	@echo "Player built successfully"

clean:
	rm -f player
	@echo "Player cleaned successfully"