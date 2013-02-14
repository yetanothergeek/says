APPNAME=says

EXTRA_CFLAGS=
EXTRA_LDFLAGS=

FT_CFLAGS=-I/usr/include/freetype2

WARN_FLAGS=-Wall -Wextra -Wshadow -Wunused -pedantic -std=gnu99

CFLAGS=$(WARN_FLAGS) -O2 ${EXTRA_CFLAGS}

LDFLAGS=${EXTRA_LDFLAGS} -lX11 -lm

ifeq (${XFT}, 0)
else
  CFLAGS += -DXFT=1 ${FT_CFLAGS}
  LDFLAGS +=  -lfreetype -lXft
  ifeq (${XFT_DLOPEN}, 1)
    CFLAGS += -DXFT_DLOPENT=1
  endif
endif



all: clean $(APPNAME)

$(APPNAME): $(subst .c,.o,$(wildcard *.c))
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *.o

%o : %c
	$(CC)  $(CFLAGS) -c $<

clean:
	rm -f $(APPNAME) *.o *.log gmon.out $(APPNAME)-*.tar.gz

dist: clean
	date +$(APPNAME)-%Y-%m-%d > PKG_NAME
	rm -rf `cat PKG_NAME`
	mkdir -p `cat PKG_NAME`
	cp -p * `cat PKG_NAME` 2>/dev/null || true 
	rm -f `cat PKG_NAME`/PKG_NAME
	tar --owner=0 --group=0 -zcf `cat PKG_NAME`.tar.gz `cat PKG_NAME`
	rm -rf `cat PKG_NAME`
	rm -f PKG_NAME


