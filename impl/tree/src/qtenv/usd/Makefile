#
# Makefile for the oppqtenv-usd plugin (OpenUSD/Hydra 3D viewer backend).
# Modeled on src/qtenv/osg/Makefile; built when WITH_USD=yes.
#

CONFIGFILE = ../../../Makefile.inc
include $(CONFIGFILE)

LIBNAME=$(LIB_PREFIX)oppqtenv-usd$D
TARGET_LIB_FILES=$(OMNETPP_LIB_DIR)/$(LIBNAME)$(LIB_SUFFIX)
ifeq ($(PLATFORM),win32)
  ifeq ($(SHARED_LIBS),yes)
    TARGET_LIB_FILES=$(OMNETPP_BIN_DIR)/$(LIBNAME)$(DLL_LIB_SUFFIX)
  endif
endif

O=$(OMNETPP_OUT_DIR)/$(CONFIGNAME)/src/qtenv/usd

# -std=gnu++17 appended last (overrides CXXFLAGS's -std=c++17): the TBB headers
# bundled with OpenUSD need GNU extensions on Apple clang 21 (same as the
# CMake-built spike, where gnu++17 is the default).
COPTS = $(CFLAGS) -I"$(OMNETPP_INCL_DIR)" -I"$(OMNETPP_SRC_DIR)" $(QT_CFLAGS) $(USD_CFLAGS) \
        -Wno-deprecated-declarations -Wno-ignored-attributes -Wno-inconsistent-missing-override \
        -std=gnu++17
# QT_NO_KEYWORDS is REQUIRED: Qt's signals/slots/emit macros collide with
# TBB/Boost identifiers pulled in by OpenUSD headers.
DEFINES += -DUNICODE -DQT_NO_KEYWORDS -DQT_OPENGL_LIB -DQT_OPENGLWIDGETS_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB
IMPLIBS = -loppsim$D -loppenvir$D -loppqtenv$D -lopplayout$D -loppcommon$D $(QT_LIBS) $(USD_LIBS)

SOURCES=$(wildcard *.cc)
SOURCES_MM=

ifeq ($(PLATFORM),macos)
  COPTS += -Wno-deprecated-declarations
  # Metal-native present path (usdpresent_metal.mm)
  SOURCES_MM = $(wildcard *.mm)
  LDFLAGS += -framework Metal -framework QuartzCore -framework AppKit -framework Foundation -framework ApplicationServices
endif

ifeq ($(MODE),release)
  DEFINES += -DQT_NO_DEBUG_OUTPUT
endif

MOC_SRC=$(filter-out ui_%.h, $(wildcard *.h))
MOC_OUT=$(MOC_SRC:%.h=moc_%.cpp)
GENERATED_SOURCES=$(MOC_OUT)
OBJS=$(SOURCES:%.cc=$O/%.o) $(SOURCES_MM:%.mm=$O/%.o) $(MOC_OUT:%.cpp=$O/%.o)

EXPORT_MACRO =

COPTS_FILE = $O/.last-copts
ifneq ("$(COPTS)","$(shell cat $(COPTS_FILE) 2>/dev/null || echo '')")
  $(shell $(MKPATH) "$O")
  $(file >$(COPTS_FILE),$(COPTS))
endif

.PHONY: all clean
.SECONDARY: $(OBJS) $O/$(LIBNAME)$(LIB_SUFFIX) $(GENERATED_SOURCES)

.SUFFIXES :

$O/%.o: %.cc $(CONFIGFILE) $(GENERATED_SOURCES) $(COPTS_FILE)
	@mkdir -p $O
	$(qecho) "$<"
	$(Q)$(CXX) -c $(CXXFLAGS) $(COPTS) $(EXPORT_DEFINES) $(IMPORT_DEFINES) -o $@ $<

$O/%.o: %.mm $(CONFIGFILE) $(GENERATED_SOURCES) $(COPTS_FILE)
	@mkdir -p $O
	$(qecho) "$<"
	$(Q)$(CXX) -c -fobjc-arc $(CXXFLAGS) $(COPTS) $(EXPORT_DEFINES) $(IMPORT_DEFINES) -o $@ $<

$O/%.o: %.cpp $(CONFIGFILE) $(GENERATED_SOURCES)
	@mkdir -p $O
	$(qecho) "$<"
	$(Q)$(CXX) -c $(CXXFLAGS) $(COPTS) $(EXPORT_DEFINES) $(IMPORT_DEFINES) -o $@ $<

moc_%.cpp: %.h
	$(Q)$(MOC) --no-notes $(DEFINES) -o $@ $<

all : $(TARGET_LIB_FILES)

ifeq ($(SHARED_LIBS),yes)
$O/%$(SO_LIB_SUFFIX) : $(OBJS)
	@mkdir -p $O
	@echo Creating shared library: $@
	$(Q)$(SHLIB_LD) $(LDFLAGS) -o $O/$(LIBNAME)$(SO_LIB_SUFFIX) $(OBJS) $(IMPLIBS)
else
$O/%$(A_LIB_SUFFIX) : $(OBJS)
	@mkdir -p $O
	@echo Creating static library: $@
	$(Q)$(AR_CR) $(ARFLAG_OUT)$O/$(LIBNAME)$(A_LIB_SUFFIX) $(OBJS)
	$(Q)$(RANLIB) $O/$(LIBNAME)$(A_LIB_SUFFIX)
endif

$(OMNETPP_LIB_DIR)/%: $O/% $(CONFIGFILE)
	@mkdir -p $(OMNETPP_LIB_DIR)
	@echo Copying to: $@
	$(Q)cp $< $@
	$(Q)$(SHLIB_POSTPROCESS) $@

$(OMNETPP_BIN_DIR)/%: $O/% $(CONFIGFILE)
	@mkdir -p $(OMNETPP_BIN_DIR)
	@echo Copying to: $@
	$(Q)cp $< $@
	$(Q)$(SHLIB_POSTPROCESS) $@

clean:
	$(qecho) Cleaning qtenv-usd
	$(Q)rm -rf $O $(GENERATED_SOURCES) $(TARGET_LIB_FILES)

-include $(OBJS:%.o=%.d)
