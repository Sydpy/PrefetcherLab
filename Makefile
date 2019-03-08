include ./scripts/FRAMEWORK

CXX=g++-4.8
CC=gcc
DEBUG=0

export

all: compile test

compile:
	DEBUG=${DEBUG} ./scripts/compile.sh

test:
	DEBUG=${DEBUG} ./scripts/test_prefetcher.py


clean:
	rm -Rf build
	rm -Rf output
	rm -Rf stats.txt
