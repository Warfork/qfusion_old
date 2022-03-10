# Qfusion makefile for the game module

include ../Makefile.inc


# Dependencies

-include ../$(BUILD_DEBUG_DIR)/baseqf/game/*.d \
	../$(BUILD_RELEASE_DIR)/baseqf/game/*.d


# Variables

OBJS_GAME=\
	$(BUILDDIR)/baseqf/game/gs_misc.o \
	$(BUILDDIR)/baseqf/game/gs_pmove.o \
	\
	$(BUILDDIR)/baseqf/game/g_ai.o \
	$(BUILDDIR)/baseqf/game/g_chase.o \
	$(BUILDDIR)/baseqf/game/g_cmds.o \
	$(BUILDDIR)/baseqf/game/g_combat.o \
	$(BUILDDIR)/baseqf/game/g_ctf.o \
	$(BUILDDIR)/baseqf/game/g_func.o \
	$(BUILDDIR)/baseqf/game/g_items.o \
	$(BUILDDIR)/baseqf/game/g_main.o \
	$(BUILDDIR)/baseqf/game/g_misc.o \
	$(BUILDDIR)/baseqf/game/g_monster.o \
	$(BUILDDIR)/baseqf/game/g_phys.o \
	$(BUILDDIR)/baseqf/game/g_pmodels.o \
	$(BUILDDIR)/baseqf/game/g_save.o \
	$(BUILDDIR)/baseqf/game/g_spawn.o \
	$(BUILDDIR)/baseqf/game/g_svcmds.o \
	$(BUILDDIR)/baseqf/game/g_syscalls.o \
	$(BUILDDIR)/baseqf/game/g_target.o \
	$(BUILDDIR)/baseqf/game/g_trigger.o \
	$(BUILDDIR)/baseqf/game/g_utils.o \
	$(BUILDDIR)/baseqf/game/g_weapon.o \
	\
	$(BUILDDIR)/baseqf/game/m_move.o \
	\
	$(BUILDDIR)/baseqf/game/p_client.o \
	$(BUILDDIR)/baseqf/game/p_hud.o \
	$(BUILDDIR)/baseqf/game/p_menu.o \
	$(BUILDDIR)/baseqf/game/p_trail.o \
	$(BUILDDIR)/baseqf/game/p_view.o \
	$(BUILDDIR)/baseqf/game/p_weapon.o \
	\
	$(BUILDDIR)/baseqf/game/q_math.o \
	$(BUILDDIR)/baseqf/game/q_shared.o \


# Targets

build:
	@mkdir -p $(BUILDDIR)/baseqf/game
	@echo
	@echo "> Building $(BUILDDIR)/baseqf/game_$(ARCH).so with flags: $(CFLAGS_COMMON) $(CFLAGS_EXTRA) $(CFLAGS_MODULE)"
	@$(MAKE) $(BUILDDIR)/baseqf/game_$(ARCH).so \
		BUILDDIR="$(BUILDDIR)" \
		CFLAGS_EXTRA="$(CFLAGS_EXTRA)" \
		QF_VERSION="$(QF_VERSION)"

clean:
	@rm -f \
		$(BUILDDIR)/baseqf/game_$(ARCH).so \
		$(BUILDDIR)/baseqf/game/*.d \
		$(OBJS_GAME)

distclean:
	@$(MAKE) clean BUILDDIR="../../$(BUILD_DEBUG_DIR)"
	@$(MAKE) clean BUILDDIR="../../$(BUILD_RELEASE_DIR)"
	@-rmdir \
		../../$(BUILD_DEBUG_DIR)/baseqf/game \
		../../$(BUILD_RELEASE_DIR)/baseqf/game


# Rules

$(BUILDDIR)/baseqf/game_$(ARCH).so: $(OBJS_GAME)
	@echo "  > Linking $@ with flags: $(LDFLAGS_COMMON) $(LDFLAGS_MODULE)"
	@$(CC) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE)

$(BUILDDIR)/baseqf/game/%.o: %.c
	@$(DO_CC_MODULE)
