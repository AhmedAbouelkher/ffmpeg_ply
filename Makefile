build:
	gcc -o player main.c $$(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale)
	@echo "Player built successfully"

clean:
	rm -f player
	@echo "Player cleaned successfully"