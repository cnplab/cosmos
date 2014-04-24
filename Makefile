################################################################################
# Basic configurations
################################################################################
verbose			?= n
debug			?= n

# The domain helper: one of [none, xcl, xl]
DOMLIB			?= none

DOM_NONE		:= n
DOM_XCL			:= n
DOM_XL			:= n
ifeq ($(DOMLIB),none)
DOM_NONE		:= y
else ifeq ($(DOMLIB),xcl)
DOM_XCL			:= y
else ifeq ($(DOMLIB),xl)
DOM_XL			:= y
else
$(error Invalid domain helper $(DOMLIB))
endif

ifneq (,$(filter $(DOMLIB),xcl xl))
ifndef XEN_ROOT
$(error "Define XEN_ROOT to build with DOMLIB=$(DOMLIB)")
endif
endif

_VERSION	?= $(shell xl info | grep xen_version)
ifeq "$(_VERSION)" ""
XEN_VERSION          ?= $(shell xl info | grep -E "(xen_major|xen_minor|xen_extra)" | cut -d: -f2 | tr -d ' ' | tr -d '.' | tr '\n' '.' | awk  '{ \
        split($$1, b, "."); \
        printf "%01x%02x%02x", b[1], b[2], b[3]; \
        }')
else
XEN_VERSION     ?= $(shell echo $(_VERSION) | awk -F': ' { \
	split($$2, a, "-"); \
	split(a[1], b, "."); \
	printf "%01x%02x%02x", b[1], b[2], b[3]; \
	}')
endif

PYTHON_VERSION	?= 2.7

V8_SWIG			?= swig
V8_VERSION		?= 3.15.5.9
V8_INCLUDE		?= /root/apps/node-v0.10.18/deps/v8/include


.PHONY:
default: all


################################################################################
# Base commands and functions
################################################################################
CP				 = cp -a
MV				 = mv -f
LN				 = ln -sf
RM				 = rm -f
MKDIR			 = mkdir -p
RMDIR			 = rm -rf
TOUCH			 = touch

ASCOMPILE		 = $(CC) $(ASDEFINES) $(CDEFINES) $(CINCLUDES) $(ASFLAGS)
COMPILE			 = $(CC) $(CDEFINES)  $(CINCLUDES) $(CPPFLAGS) $(CFLAGS)
CCLD			 = $(CC)
CCLINK			 = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@
CXXCOMPILE		 = $(CXX) $(CDEFINES) $(CINCLUDES) $(CPPFLAGS) $(CXXFLAGS)
CXXLD			 = $(CXX)
CXXLINK			 = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@

SWIG			 = swig
SWIGCOMPILE		 = $(SWIG) $(SWIGFLAGS) $(SWIGINCLUDES)
XXD				?= xxd

ifneq ($(verbose),y)
ascompile		 = @/bin/echo ' ' $(2) $< && $(ASCOMPILE) $(1)
ccompile		 = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cclink			 = @/bin/echo ' ' $(2) $@ && $(CCLINK) $(1)
cxxcompile		 = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
cxxlink			 = @/bin/echo ' ' $(2) $< && $(CXXLINK) $(1)
swig			 = @/bin/echo ' ' $(2) $< && $(SWIGCOMPILE) $(1)
archive			 = @/bin/echo ' ' $(2) $@ && $(AR) cr $(1)
x_verbose_cmd	 = $(if $(2),/bin/echo ' ' $(2) $(3) &&,) $(1) $(3)
verbose_cmd		 = @$(x_verbose_cmd)
else
ascompile		 = $(ASCOMPILE) $(1)
ccompile		 = $(COMPILE) $(1)
cclink			 = $(CCLINK) $(1)
cxxcompile		 = $(CXXCOMPILE) $(1)
cxxlink			 = $(CXXLINK) $(1)
swig			 = $(SWIGCOMPILE) $(1)
archive			 = $(AR) crv $(1)
x_verbose_cmd	 = $(1) $(3)
verbose_cmd		 = $(1) $(3)
endif


################################################################################
# Build directory structure
################################################################################
ROOT_DIR		?= $(realpath .)

SOURCE_DIR		?= $(ROOT_DIR)/src
INCLUDE_DIR		?= $(ROOT_DIR)/include
BUILD_DIR		?= $(ROOT_DIR)/build

DIST_DIR		?= $(ROOT_DIR)/dist
BIN_DIR			?= $(DIST_DIR)/bin
LIB_DIR			?= $(DIST_DIR)/lib

BUILD_DIRS		+= $(BUILD_DIR)
BUILD_DIRS		+= $(DIST_DIR)
BUILD_DIRS		+= $(BIN_DIR)
BUILD_DIRS		+= $(LIB_DIR)


################################################################################
# Configure building
################################################################################
CDEFINES	+= -DXEN_VERSION=$(XEN_VERSION)

CINCLUDES	+= -I$(INCLUDE_DIR)

CFLAGS		+= -std=gnu99
CFLAGS		+= -fno-strict-aliasing
CFLAGS		+= -fPIC
CFLAGS		+= -Wall -Wstrict-prototypes
CFLAGS		+= -Wno-format-zero-length

LDFLAGS		+= -lxenctrl
LDFLAGS		+= -lxenguest
LDFLAGS		+= -lxenstore
LDFLAGS		+= -luuid -ldl -lutil

ifeq ($(debug),y)
CFLAGS		+= -O0 -g
endif


################################################################################
# libxcl
################################################################################
LIBXCL_VERSION 			:= 0.1.0
LIBXCL_VERSION_MAJOR	:= 0

LIBXCL_BUILD_DIR		 = $(BUILD_DIR)/libxcl
LIBXCL_SOURCE_DIR		 = $(SOURCE_DIR)/xcl
LIBXCL_OBJS0			 =	\
	xcl_dom.o				\
	xcl_net.o
LIBXCL_OBJS				 = $(addprefix $(LIBXCL_BUILD_DIR)/,$(LIBXCL_OBJS0))
LIBXCL_LDFLAGS			+= -lxenlight

BUILD_DIRS				+= $(LIBXCL_BUILD_DIR)


$(LIBXCL_BUILD_DIR)/%.o: CINCLUDES += -I$(INCLUDE_DIR)/xcl
$(LIBXCL_BUILD_DIR)/%.o: CINCLUDES += -I$(XEN_ROOT)/tools/libxl
$(LIBXCL_BUILD_DIR)/%.o: CINCLUDES += -I$(XEN_ROOT)/tools/libxc
$(LIBXCL_BUILD_DIR)/%.o: CINCLUDES += -I$(XEN_ROOT)/tools/include
$(LIBXCL_BUILD_DIR)/%.o: $(LIBXCL_SOURCE_DIR)/%.c | bootstrap
	$(call ccompile,-c $< -o $@,'CC ')

$(LIB_DIR)/libxcl.so: LDFLAGS += -shared
$(LIB_DIR)/libxcl.so: $(LIBXCL_OBJS) | bootstrap
	$(call verbose_cmd,$(LN) libxcl.so,'LN ',$@.$(LIBXCL_VERSION))
	$(call verbose_cmd,$(LN) libxcl.so,'LN ',$@.$(LIBXCL_VERSION_MAJOR))
	$(call cclink,$^,'LD ')


.PHONY: libxcl
libxcl: $(LIB_DIR)/libxcl.so

.PHONY: clean-libxcl
clean: clean-libxcl
clean-libxcl:
	$(call verbose_cmd,$(RM) $(LIBXCL_BUILD_DIR)/*.o,'CLN $(LIBXCL_BUILD_DIR)/*.o')

.PHONY: distclean-libxcl
distclean: distclean-libxcl
distclean-libxcl: clean-libxcl
	$(call verbose_cmd,$(RMDIR),'CLN',$(LIBXCL_BUILD_DIR))
	$(call verbose_cmd,$(RM) $(LIB_DIR)/libxcl.so*,'CLN $(LIB_DIR)/libxcl.so')


################################################################################
# libxl
################################################################################
LIBXL_CINCLUDES		 = -I$(XEN_ROOT)/tools/libxl
LIBXL_LDFLAGS		 = -lxenlight -lxlutil -lyajl


################################################################################
# libcosmos
################################################################################
LIBCOSMOS_VERSION				:= 0.1.0
LIBCOSMOS_VERSION_MAJOR			:= 0

LIBCOSMOS_BUILD_DIR				 = $(BUILD_DIR)/libcosmos
LIBCOSMOS_SOURCE_DIR			 = $(SOURCE_DIR)
LIBCOSMOS_OBJS0-y				 =	\
	clickos.o
LIBCOSMOS_OBJS0-$(DOM_NONE)		+= domain_none.o
LIBCOSMOS_OBJS0-$(DOM_XCL)		+= domain_xcl.o
LIBCOSMOS_OBJS0-$(DOM_XL)		+= domain_xl.o
LIBCOSMOS_OBJS					 = $(addprefix $(LIBCOSMOS_BUILD_DIR)/,$(LIBCOSMOS_OBJS0-y))

LIBCOSMOS_CFLAGS-y				:=
LIBCOSMOS_CFLAGS-$(DOM_XCL)		+= -DHAVE_XCL
LIBCOSMOS_CFLAGS-$(DOM_XL)		+= -DHAVE_XL
LIBCOSMOS_CFLAGS				 = $(LIBCOSMOS_CFLAGS-y)

LIBCOSMOS_CINCLUDES-y			:=
LIBCOSMOS_CINCLUDES-$(DOM_XCL)	+= $(LIBXCL_CINCLUDES)
LIBCOSMOS_CINCLUDES-$(DOM_XL)	+= $(LIBXL_CINCLUDES)
LIBCOSMOS_CINCLUDES				 = $(LIBCOSMOS_CINCLUDES-y)

LIBCOSMOS_LDFLAGS-y				:=
LIBCOSMOS_LDFLAGS-$(DOM_XCL)	+= $(LIBXCL_LDFLAGS)
LIBCOSMOS_LDFLAGS-$(DOM_XL)		+= $(LIBXL_LDFLAGS)
LIBCOSMOS_LDFLAGS				 = $(LIBCOSMOS_LDFLAGS-y)

BUILD_DIRS						+= $(LIBCOSMOS_BUILD_DIR)


$(LIBCOSMOS_BUILD_DIR)/%.o: CFLAGS += $(LIBCOSMOS_CFLAGS)
$(LIBCOSMOS_BUILD_DIR)/%.o: CINCLUDES += $(LIBCOSMOS_CINCLUDES)
$(LIBCOSMOS_BUILD_DIR)/%.o: $(LIBCOSMOS_SOURCE_DIR)/%.c | bootstrap
	$(call ccompile,-c $< -o $@,'CC ')

$(LIB_DIR)/libcosmos.so: LDFLAGS += -shared
$(LIB_DIR)/libcosmos.so: $(LIBCOSMOS_OBJS) | bootstrap
	$(call verbose_cmd,$(LN) libcosmos.so,'LN ',$@.$(LIBCOSMOS_VERSION))
	$(call verbose_cmd,$(LN) libcosmos.so,'LN ',$@.$(LIBCOSMOS_VERSION_MAJOR))
	$(call cclink,$^,'LD ')


.PHONY: libcosmos
libcosmos: $(LIB_DIR)/libcosmos.so

.PHONY: clean-libcosmos
clean: clean-libcosmos
clean-libcosmos:
	$(call verbose_cmd,$(RM) $(LIBCOSMOS_BUILD_DIR)/*.o,'CLN $(LIBCOSMOS_BUILD_DIR)/*.o')

.PHONY: distclean-libcosmos
distclean: distclean-libcosmos
distclean-libcosmos: clean-libcosmos
	$(call verbose_cmd,$(RMDIR),'CLN',$(LIBCOSMOS_BUILD_DIR))
	$(call verbose_cmd,$(RM) $(LIB_DIR)/libcosmos.so*,'CLN $(LIB_DIR)/libcosmos.so')


################################################################################
# cosmos
################################################################################
COSMOS_APP					 = $(BIN_DIR)/cosmos
COSMOS_BUILD_DIR			 = $(BUILD_DIR)/cosmos
COSMOS_OBJS0				 =	\
	main.o
COSMOS_OBJS-y				 = $(addprefix $(COSMOS_BUILD_DIR)/,$(COSMOS_OBJS0))
COSMOS_OBJS-y				+= $(LIBCOSMOS_OBJS)
COSMOS_OBJS-$(DOM_XCL)		+= $(LIBXCL_OBJS)
COSMOS_OBJS					 = $(COSMOS_OBJS-y)

BUILD_DIRS					+= $(COSMOS_BUILD_DIR)


$(COSMOS_BUILD_DIR)/%.o: CFLAGS += $(LIBCOSMOS_CFLAGS)
$(COSMOS_BUILD_DIR)/%.o: CINCLUDES += $(LIBCOSMOS_CINCLUDES)
$(COSMOS_BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | bootstrap
	$(call ccompile,-c $< -o $@,'CC ')

$(COSMOS_APP): LDFLAGS+=$(LIBCOSMOS_LDFLAGS)
$(COSMOS_APP): $(COSMOS_OBJS) | bootstrap
	$(call cclink,$^,'LD ')


.PHONY: cosmos
all: cosmos
cosmos: $(BIN_DIR)/cosmos

.PHONY: clean-cosmos
clean: clean-cosmos
clean-cosmos:
	$(call verbose_cmd,$(RM) $(COSMOS_BUILD_DIR)/*.o,'CLN $(COSMOS_BUILD_DIR)/*.o')

.PHONY: distclean-cosmos
distclean: distclean-cosmos
distclean-cosmos: clean-cosmos
	$(call verbose_cmd,$(RM),'CLN',$(COSMOS_APP))


################################################################################
# bindings
################################################################################
BINDING_CFLAGS		 = -fPIC
BINDING_LDFLAGS		 = -L$(LIB_DIR) -lcosmos

SWIGFLAGS			 = -DBINDING_SWIG
SWIGINCLUDES		 = -I$(INCLUDE_DIR)

python-binding: BINDING_CINCLUDES += -I/usr/include/python$(PYTHON_VERSION)

js-binding: SWIG = $(V8_SWIG)
js-binding: SWIGFLAGS += -c++ -v8
js-binding: BINDING_CFLAGS += $(V8_INCLUDE)

# So that make doesn't delete intermediary files (in which the target is included)
.SECONDARY:

%-binding: LDFLAGS += -shared
%-binding: $(LIB_DIR)/%-cosmos.so
	@#

$(LIB_DIR)/%-cosmos.so: $(BUILD_DIR)/%/cosmos_wrap.o $(LIBCOSMOS_OBJS)
	$(call cclink, $^,'LD ')

$(BUILD_DIR)/%/cosmos_wrap.o: CFLAGS = $(BINDING_CFLAGS)
$(BUILD_DIR)/%/cosmos_wrap.o: CINCLUDES = $(BINDING_CINCLUDES)

ifneq (,$(filter $(MAKECMDGOALS),js-binding))
$(BUILD_DIR)/%/cosmos_wrap.o: $(BUILD_DIR)/%/cosmos_wrap.cxx
	$(call cxxcompile, -c $< -o $@,'CXX')
else
$(BUILD_DIR)/%/cosmos_wrap.o: $(BUILD_DIR)/%/cosmos_wrap.c
	$(call ccompile, -c $< -o $@,'CC ')
endif

ifneq (,$(filter $(MAKECMDGOALS),js-binding))
$(BUILD_DIR)/%/cosmos_wrap.cxx: $(BUILD_DIR)/%/cosmos.i
else
$(BUILD_DIR)/%/cosmos_wrap.c: $(BUILD_DIR)/%/cosmos.i
endif
	$(call swig, -includeall -$* $<,'SWG')

$(BUILD_DIR)/%/cosmos.i: $(SOURCE_DIR)/cosmos.i | bootstrap
	$(call verbose_cmd, $(MKDIR), MKD, $(dir $@))
	$(call verbose_cmd, $(CP), 'CP ',$< $@)


################################################################################
# Targets
################################################################################

.PHONY: all
all:
	@#

.PHONY: bootstrap
bootstrap: $(BUILD_DIRS)

$(BUILD_DIRS):
	$(call verbose_cmd,$(MKDIR),'MKD',$@)

.PHONY:clean
clean:
	@#

.PHONY: distclean
distclean: clean
	$(call verbose_cmd,$(RMDIR),'CLN',$(BUILD_DIR))
	$(call verbose_cmd,$(RMDIR),'CLN',$(DIST_DIR))

