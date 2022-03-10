# Qfusion makefile for the cgame module

include ../Makefile.inc


# Dependencies

-include ../$(BUILD_DEBUG_DIR)/baseqf/cgame/*.d \
	../$(BUILD_RELEASE_DIR)/baseqf/cgame/*.d


# Variables

OBJS_CGAME=\
	$(BUILDDIR)/baseqf/cgame/gs_misc.o \
	$(BUILDDIR)/baseqf/cgame/gs_pmove.o \
	\
	$(BUILDDIR)/baseqf/cgame/cg_boneposes.o \
	$(BUILDDIR)/baseqf/cgame/cg_decals.o \
	$(BUILDDIR)/baseqf/cgame/cg_draw.o \
	$(BUILDDIR)/baseqf/cgame/cg_effects.o \
	$(BUILDDIR)/baseqf/cgame/cg_ents.o \
	$(BUILDDIR)/baseqf/cgame/cg_events.o \
	$(BUILDDIR)/baseqf/cgame/cg_lents.o \
	$(BUILDDIR)/baseqf/cgame/cg_main.o \
	$(BUILDDIR)/baseqf/cgame/cg_media.o \
	$(BUILDDIR)/baseqf/cgame/cg_players.o \
	$(BUILDDIR)/baseqf/cgame/cg_pmodels.o \
	$(BUILDDIR)/baseqf/cgame/cg_predict.o \
	$(BUILDDIR)/baseqf/cgame/cg_screen.o \
	$(BUILDDIR)/baseqf/cgame/cg_svcmds.o \
	$(BUILDDIR)/baseqf/cgame/cg_syscalls.o \
	$(BUILDDIR)/baseqf/cgame/cg_view.o \
	$(BUILDDIR)/baseqf/cgame/cg_vweap.o \
	$(BUILDDIR)/baseqf/cgame/cg_wmodels.o \
	\
	$(BUILDDIR)/baseqf/cgame/q_math.o \
	$(BUILDDIR)/baseqf/cgame/q_shared.o \


# Targets

build:
	@mkdir -p $(BUILDDIR)/baseqf/cgame
	@echo
	@echo "> Building $(BUILDDIR)/baseqf/cgame_$(ARCH).so with flags: $(CFLAGS_COMMON) $(CFLAGS_EXTRA) $(CFLAGS_MODULE)"
	@$(MAKE) $(BUILDDIR)/baseqf/cgame_$(ARCH).so \
		BUILDDIR="$(BUILDDIR)" \
		CFLAGS_EXTRA="$(CFLAGS_EXTRA)" \
		QF_VERSION="$(QF_VERSION)"

clean:
	@rm -f \
		$(BUILDDIR)/baseqf/cgame_$(ARCH).so \
		$(BUILDDIR)/baseqf/cgame/*.d \
		$(OBJS_CGAME)

distclean:
	@$(MAKE) clean BUILDDIR="../../$(BUILD_DEBUG_DIR)"
	@$(MAKE) clean BUILDDIR="../../$(BUILD_RELEASE_DIR)"
	@-rmdir \
		../../$(BUILD_DEBUG_DIR)/baseqf/cgame \
		../../$(BUILD_RELEASE_DIR)/baseqf/cgame


# Rules

$(BUILDDIR)/baseqf/cgame_$(ARCH).so: $(OBJS_CGAME)
	@echo "  > Linking $@ with flags: $(LDFLAGS_COMMON) $(LDFLAGS_MODULE)"
	@$(CC) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE)

$(BUILDDIR)/baseqf/cgame/%.o: %.c
	@$(DO_CC_MODULE)

$(BUILDDIR)/baseqf/cgame/%.o: ../game/%.c
	@$(DO_CC_MODULE)
