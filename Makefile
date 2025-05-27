

all:
	gcc test.c tl.c tlbc.c tlpr.c tlopenlib.c -fsanitize=address \
		-Wincompatible-pointer-types -Wall -g -o toylang
	./toylang test.tl

