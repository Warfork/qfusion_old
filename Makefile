# Qfusion source makefile

include Makefile.inc


# Dependencies

-include ../$(BUILD_DEBUG_DIR)/client/*.d \
	../$(BUILD_DEBUG_DIR)/ded/*.d \
	../$(BUILD_RELEASE_DIR)/client/*.d \
	../$(BUILD_RELEASE_DIR)/ded/*.d


# Variables

LDFLAGS_CL=-ljpeg -lz -lasound -L/usr/X11R6/lib -lX11 -lXxf86dga -lXxf86vm -lXext -lXinerama
LDFLAGS_DED=-lz

DO_CC_DED=$(DO_CC) -DDEDICATED_ONLY -DC_ONLY

OBJS_CLIENT=\
	$(BUILDDIR)/client/cmd.o \
	$(BUILDDIR)/client/common.o \
	$(BUILDDIR)/client/cvar.o \
	$(BUILDDIR)/client/net_chan.o \
	$(BUILDDIR)/client/cm_main.o \
	$(BUILDDIR)/client/cm_trace.o \
	$(BUILDDIR)/client/files.o \
	$(BUILDDIR)/client/mem.o \
	$(BUILDDIR)/client/mdfour.o \
	$(BUILDDIR)/client/patch.o \
	$(BUILDDIR)/client/huff.o \
	\
	$(BUILDDIR)/client/sv_ccmds.o \
	$(BUILDDIR)/client/sv_game.o \
	$(BUILDDIR)/client/sv_main.o \
	$(BUILDDIR)/client/sv_send.o \
	$(BUILDDIR)/client/sv_world.o \
	$(BUILDDIR)/client/sv_ents.o \
	$(BUILDDIR)/client/sv_init.o \
	$(BUILDDIR)/client/sv_user.o \
	\
	$(BUILDDIR)/client/cin.o \
	$(BUILDDIR)/client/cl_demo.o \
	$(BUILDDIR)/client/cl_game.o \
	$(BUILDDIR)/client/cl_parse.o \
	$(BUILDDIR)/client/cl_ui.o \
	$(BUILDDIR)/client/snd_dma.o \
	$(BUILDDIR)/client/snd_oal.o \
	$(BUILDDIR)/client/snd_ogg.o \
	$(BUILDDIR)/client/cl_cin.o \
	$(BUILDDIR)/client/cl_input.o \
	$(BUILDDIR)/client/snd_mem.o \
	$(BUILDDIR)/client/cl_ents.o \
	$(BUILDDIR)/client/cl_screen.o \
	$(BUILDDIR)/client/cl_vid.o \
	$(BUILDDIR)/client/console.o \
	$(BUILDDIR)/client/snd_mix.o \
	$(BUILDDIR)/client/cl_main.o \
	$(BUILDDIR)/client/keys.o \
	\
	$(BUILDDIR)/client/r_alias.o \
	$(BUILDDIR)/client/r_backend.o \
	$(BUILDDIR)/client/r_bloom.o \
	$(BUILDDIR)/client/r_main.o \
	$(BUILDDIR)/client/r_math.o \
	$(BUILDDIR)/client/r_mesh.o \
	$(BUILDDIR)/client/r_shader.o \
	$(BUILDDIR)/client/r_shadow.o \
	$(BUILDDIR)/client/r_cin.o \
	$(BUILDDIR)/client/r_cull.o \
	$(BUILDDIR)/client/r_image.o \
	$(BUILDDIR)/client/r_register.o \
	$(BUILDDIR)/client/r_program.o \
	$(BUILDDIR)/client/r_poly.o \
	$(BUILDDIR)/client/r_surf.o \
	$(BUILDDIR)/client/r_skin.o \
	$(BUILDDIR)/client/r_skm.o \
	$(BUILDDIR)/client/r_light.o \
	$(BUILDDIR)/client/r_model.o \
	$(BUILDDIR)/client/r_sky.o \
	\
	$(BUILDDIR)/client/glob.o \
	$(BUILDDIR)/client/qgl_linux.o \
	$(BUILDDIR)/client/snd_alsa.o \
	$(BUILDDIR)/client/vid_linux.o \
	$(BUILDDIR)/client/net_udp.o \
	$(BUILDDIR)/client/in_x11.o \
	$(BUILDDIR)/client/glx_imp.o \
	$(BUILDDIR)/client/sys_linux.o \
	\
	$(BUILDDIR)/client/q_math.o \
	$(BUILDDIR)/client/q_shared.o \

OBJS_DEDICATED=\
	$(BUILDDIR)/ded/cmd.o \
	$(BUILDDIR)/ded/common.o \
	$(BUILDDIR)/ded/cvar.o \
	$(BUILDDIR)/ded/net_chan.o \
	$(BUILDDIR)/ded/cm_main.o \
	$(BUILDDIR)/ded/cm_trace.o \
	$(BUILDDIR)/ded/files.o \
	$(BUILDDIR)/ded/mem.o \
	$(BUILDDIR)/ded/mdfour.o \
	$(BUILDDIR)/ded/patch.o \
	$(BUILDDIR)/ded/huff.o \
	\
	$(BUILDDIR)/ded/sv_ccmds.o \
	$(BUILDDIR)/ded/sv_game.o \
	$(BUILDDIR)/ded/sv_main.o \
	$(BUILDDIR)/ded/sv_send.o \
	$(BUILDDIR)/ded/sv_world.o \
	$(BUILDDIR)/ded/sv_ents.o \
	$(BUILDDIR)/ded/sv_init.o \
	$(BUILDDIR)/ded/sv_user.o \
	\
	$(BUILDDIR)/ded/cl_null.o \
	\
	$(BUILDDIR)/ded/glob.o \
	$(BUILDDIR)/ded/net_udp.o \
	$(BUILDDIR)/ded/sys_linux.o \
	\
	$(BUILDDIR)/ded/q_math.o \
	$(BUILDDIR)/ded/q_shared.o \

MODULES=cgame game ui


# Targets

all: debug release

modules_debug:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir build \
		BUILDDIR="../../$(BUILD_DEBUG_DIR)" \
		CFLAGS_EXTRA="$(CFLAGS_DEBUG)" \
		QF_VERSION="$(VERSION) Debug"; \
	done

modules_release:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir build \
		BUILDDIR="../../$(BUILD_RELEASE_DIR)" \
		CFLAGS_EXTRA="$(CFLAGS_RELEASE)" \
		QF_VERSION="$(VERSION)"; \
		strip ../$(BUILD_RELEASE_DIR)/baseqf/"$$dir"_$(ARCH).so; \
	done

debug:
	@$(MAKE) build \
		BUILDDIR="../$(BUILD_DEBUG_DIR)" \
		CFLAGS_EXTRA="$(CFLAGS_DEBUG)" \
		QF_VERSION="$(VERSION) Debug"
	@$(MAKE) modules_debug		

release:
	@$(MAKE) build \
		BUILDDIR="../$(BUILD_RELEASE_DIR)" \
		CFLAGS_EXTRA="$(CFLAGS_RELEASE)" \
		QF_VERSION="$(VERSION)"
	@strip ../$(BUILD_RELEASE_DIR)/qfusion
	@strip ../$(BUILD_RELEASE_DIR)/qfds
	@$(MAKE) modules_release


clean: clean-debug clean-release

clean-debug:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir clean BUILDDIR="../../$(BUILD_DEBUG_DIR)"; \
	done
	@$(MAKE) clean2 BUILDDIR="../$(BUILD_DEBUG_DIR)"

clean-release:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir clean BUILDDIR="../../$(BUILD_RELEASE_DIR)"; \
	done
	@$(MAKE) clean2 BUILDDIR="../$(BUILD_RELEASE_DIR)"

clean2:
	@rm -f \
		$(BUILDDIR)/qfusion \
		$(BUILDDIR)/qfds \
		$(BUILDDIR)/client/*.d \
		$(BUILDDIR)/ded/*.d \
		$(OBJS_CLIENT) \
		$(OBJS_DEDICATED)

distclean:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir distclean; \
	done
	@$(MAKE) clean2 BUILDDIR="../$(BUILD_DEBUG_DIR)"
	@$(MAKE) clean2 BUILDDIR="../$(BUILD_RELEASE_DIR)"
	@-rmdir \
		../$(BUILD_DEBUG_DIR)/client \
		../$(BUILD_DEBUG_DIR)/ded \
		../$(BUILD_RELEASE_DIR)/client \
		../$(BUILD_RELEASE_DIR)/ded


# Rules

build:
	@mkdir -p $(BUILDDIR)/client $(BUILDDIR)/ded
	@echo
	@echo "> Building $(BUILDDIR)/qfusion with flags: $(CFLAGS_COMMON) $(CFLAGS_EXTRA)"
	@$(MAKE) $(BUILDDIR)/qfusion \
		BUILDDIR="$(BUILDDIR)" \
		CFLAGS_EXTRA="$(CFLAGS_EXTRA)" \
		QF_VERSION="$(QF_VERSION)"
	@echo
	@echo "> Building $(BUILDDIR)/qfds with flags: $(CFLAGS_COMMON) $(CFLAGS_EXTRA)"
	@$(MAKE) $(BUILDDIR)/qfds \
		BUILDDIR="$(BUILDDIR)" \
		CFLAGS_EXTRA="$(CFLAGS_EXTRA)" \
		QF_VERSION="$(QF_VERSION)"


$(BUILDDIR)/qfusion: $(OBJS_CLIENT)
	@echo "  > Linking $@ with flags: $(LDFLAGS_COMMON) $(LDFLAGS_CL)"
	@$(CC) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_CL)

$(BUILDDIR)/client/%.o: qcommon/%.c 
	@$(DO_CC)

$(BUILDDIR)/client/%.o: server/%.c
	@$(DO_CC)

$(BUILDDIR)/client/%.o: client/%.c
	@$(DO_CC)

$(BUILDDIR)/client/%.o: ref_gl/%.c
	@$(DO_CC)

$(BUILDDIR)/client/%.o: linux/%.c
	@$(DO_CC)

$(BUILDDIR)/client/%.o: game/%.c
	@$(DO_CC)


$(BUILDDIR)/qfds: $(OBJS_DEDICATED)
	@echo "  > Linking $@ with flags: $(LDFLAGS_COMMON) $(LDFLAGS_DED)"
	@$(CC) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_DED)

$(BUILDDIR)/ded/%.o: qcommon/%.c 
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: server/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: linux/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: null/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: game/%.c
	@$(DO_CC_DED)
