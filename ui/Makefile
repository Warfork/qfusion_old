# Qfusion makefile for the ui module

include ../Makefile.inc


# Dependencies

-include ../$(BUILD_DEBUG_DIR)/baseqf/ui/*.d \
	../$(BUILD_RELEASE_DIR)/baseqf/ui/*.d


# Variables

OBJS_UI=\
	$(BUILDDIR)/baseqf/ui/ui_atoms.o \
	$(BUILDDIR)/baseqf/ui/ui_boneposes.o \
	$(BUILDDIR)/baseqf/ui/ui_connect.o \
	$(BUILDDIR)/baseqf/ui/ui_credits.o \
	$(BUILDDIR)/baseqf/ui/ui_dloptions.o \
	$(BUILDDIR)/baseqf/ui/ui_dmoptions.o \
	$(BUILDDIR)/baseqf/ui/ui_gfx.o \
	$(BUILDDIR)/baseqf/ui/ui_glext.o \
	$(BUILDDIR)/baseqf/ui/ui_keys.o \
	$(BUILDDIR)/baseqf/ui/ui_main.o \
	$(BUILDDIR)/baseqf/ui/ui_menu.o \
	$(BUILDDIR)/baseqf/ui/ui_mplayer.o \
	$(BUILDDIR)/baseqf/ui/ui_options.o \
	$(BUILDDIR)/baseqf/ui/ui_playerconfig.o \
	$(BUILDDIR)/baseqf/ui/ui_pmodels.o \
	$(BUILDDIR)/baseqf/ui/ui_quit.o \
	$(BUILDDIR)/baseqf/ui/ui_sound.o \
	$(BUILDDIR)/baseqf/ui/ui_splayer.o \
	$(BUILDDIR)/baseqf/ui/ui_startserver.o \
	$(BUILDDIR)/baseqf/ui/ui_syscalls.o \
	$(BUILDDIR)/baseqf/ui/ui_video.o \
	\
	$(BUILDDIR)/baseqf/ui/q_math.o \
	$(BUILDDIR)/baseqf/ui/q_shared.o \


# Targets

build:
	@mkdir -p $(BUILDDIR)/baseqf/ui
	@echo
	@echo "> Building $(BUILDDIR)/baseqf/ui_$(ARCH).so with flags: $(CFLAGS_COMMON) $(CFLAGS_EXTRA) $(CFLAGS_MODULE)"
	@$(MAKE) $(BUILDDIR)/baseqf/ui_$(ARCH).so \
		BUILDDIR="$(BUILDDIR)" \
		CFLAGS_EXTRA="$(CFLAGS_EXTRA)" \
		QF_VERSION="$(QF_VERSION)"

clean:
	@rm -f \
		$(BUILDDIR)/baseqf/ui_$(ARCH).so \
		$(BUILDDIR)/baseqf/ui/*.d \
		$(OBJS_UI)

distclean:
	@$(MAKE) clean BUILDDIR="../../$(BUILD_DEBUG_DIR)"
	@$(MAKE) clean BUILDDIR="../../$(BUILD_RELEASE_DIR)"
	@-rmdir \
		../../$(BUILD_DEBUG_DIR)/baseqf/ui \
		../../$(BUILD_RELEASE_DIR)/baseqf/ui


# Rules

$(BUILDDIR)/baseqf/ui_$(ARCH).so: $(OBJS_UI)
	@echo "  > Linking $@ with flags: $(LDFLAGS_COMMON) $(LDFLAGS_MODULE)"
	@$(CC) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE)

$(BUILDDIR)/baseqf/ui/%.o: %.c
	@$(DO_CC_MODULE)

$(BUILDDIR)/baseqf/ui/%.o: ../game/%.c
	@$(DO_CC_MODULE)
