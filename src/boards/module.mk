BOARDSRC := $(shell find boards -type f ! -path \*/.svn\* -name \*.c)
SRCFILES += $(BOARDSRC)
