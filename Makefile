proj_root = $(shell pwd)
soup_src  = src/*.c
dbman_src = dbman/main.c src/article.c src/dictionary.c src/template.c src/database.c

db  = gdb
mem = valgrind


ifndef ROOT
	ROOT = www
endif


build_debug:
	gcc $(soup_src) -lfcgi -O0 -g -o build/soup -DMULTIMACHINE_RDONLY
	gcc $(dbman_src) -O0 -g -o build/dbman

build_release:
	gcc $(soup_src) -lfcgi -O2 -o build/soup -DMULTIMACHINE_RDONLY
	gcc $(dbman_src) -O2 -o build/dbman


run:
	(cd $(ROOT); REQUEST_URI=$(URI) $(proj_root)/build/soup)

run_db:
	(cd $(ROOT); REQUEST_URI=$(URI) $(db) $(DB_FLAGS) $(proj_root)/build/soup)

run_mem:
	(cd $(ROOT); REQUEST_URI=$(URI) $(mem) $(DB_FLAGS) $(proj_root)/build/soup)
