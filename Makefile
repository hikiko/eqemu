src = $(wildcard src/*.cc)
obj = $(src:.cc=.o)
dep = $(obj:.o=.d)
bin = eqemu

libimago_path = libs/libimago
libimago = $(libimago_path)/libimago.a

CFLAGS = -pedantic -Wall -g -I$(libimago_path)/src
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lGL -lGLU -lGLEW -lX11 -lm -lpthread -L$(libimago_path) -limago -lpng -ljpeg -lz

$(bin): $(obj) $(libimago)
	$(CXX) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.d: %.cc
	@$(CPP) $< $(CXXFLAGS) -MM -MT $(@:.d=.o) >$@

.PHONY: $(libimago)
$(libimago):
	cd $(libimago_path) && ./configure --disable-debug --enable-opt
	$(MAKE) -C $(libimago_path)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: clean-libs
clean-libs:
	$(MAKE) -C $(libimago_path) clean
