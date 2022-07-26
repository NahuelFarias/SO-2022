# - Este makefile actúa de template para el Makefile cada módulo

CONFIGS=message-colors.mk log.mk functions.mk packages-installed

-include $(DIR_BASE)/$(DIR_PROJECT)/Makefile.cfg
-include $(foreach config, $(CONFIGS), $(addprefix $(DIR_CONFIGS), $(config)))

ifeq (false,$(GNU_MAKE_PRINT_RECIPE))
AT:=@
endif

ifeq (true,$(GCC_REDIRECT_LOGS))
ifeq (true,$(GCC_PRINT_LOGS))
CMD_REDIRECT_LOGS=2>&1 | tee -a $(DIR_COMPILE_LOGS)/compilation.log
else
CMD_REDIRECT_LOGS=1>>$(DIR_COMPILE_LOGS)/compilation.log 2>&1
endif
endif

$(DIRS): ; $(MKDIR) $@

# TODO: la primera manera no considero que sea la mejor, la segunda con `undefine` no funciona como se espera
# - Le asignamos el valor en las configs modulos.cfg para desacoplar las static/shared libraries
# con ?= le asignamos un valor vacio si ningun modulo le asigna un valor
DIR_SHARED_LIBS?=
DIR_STATIC_LIBS?=
#undefine DIR_STATIC_LIBS

####################################################################################################################
# rani you're the best one :)
# https://www.gnu.org/software/make/manual/html_node/Secondary-Expansion.html

# 1. Guardamos el nombre de las dependencias de las shared/static libraries
# - Agarra el nombre de la biblioteca Ej. `static-pepon` y convierte esta cadena `static-peon/bin/static-pepon.a`
#
# - Con (lib:%=%/bin/lib$(notdir $(lib)).ext) le agrega al final de cada ruta `/bin/lib/nombre.ext`

LIB_DEPS=$(foreach lib,$(DIR_SHARED_LIBS),$(lib:%=%/bin/lib$(notdir $(lib)).so)) \
	$(foreach lib,$(DIR_STATIC_LIBS),$(lib:%=%/bin/lib$(notdir $(lib)).a))

# 2. Cada dependencia (ej: ../static/bin/libstatic.a) se actualiza solo si se modifica algun fichero de su src/ o include/
# - Hara un nuevo build de cada shared/static library siempre que alguno de sus archivo fuente .c o archivos cabecera .h sean modificadas
# (compara el timestamp del binario .so o .a de sus codigos fuente .c y .h, si los binarios estan desactualizados
# entonces ejecuta la orden/recipe para el build)
#
# - El target `LID_DEPS` contiene la ruta de los binarios (.so y .a) lo reutiliza de la macro
#
# - La dependencia $$(shell find ...) reemplaza de la ruta que contiene el target el `bin/` por `/src`  usando la macro $@
# y los busca con `find` (comando de linux)
#
# - La orden/recipe `make -C ruta..` ejecuta el Makefile de la shared/static library
# - con `patsubt` remueve de la ruta el bin/
# - con $(dir $@) remueve el nombre del fichero de la ruta, solo queda una ruta
.SECONDEXPANSION:
$(LIB_DEPS): $$(shell find $$(patsubst %bin/,%src/,$$(dir $$@)) -iname "*.c") \
	$$(shell find $$(patsubst %bin/,%include/,$$(dir $$@)) -iname "*.h")
	$(AT)-make --no-print-directory -C $(patsubst %bin/,%,$(dir $@))

####################################################################################################################

$(DIR_BIN)/$(BIN): $(OBJ)
	$(AT)$(COMPILE)
	@echo $(LOG) $(COMPILE_DESCRIPTION) $(MSG_OK)

$(OBJ): $(DIR_OBJ)/%.o: $(LIB_DEPS) $(DIR_SRC)/%.c $(DIR_DEP)/%.d | $(DIR_DEP)
	$(call apply_clang_format,$(DIR_SRC)/$*.c)
# redireccionamos el stderr al stdout con 2>&1 y el stodut a error.log con 1>>error.log (es importante el orden en que se hace)

	$(AT) if $(CC) $(DEPFLAGS) $(CPPFLAGS) $(CFLAGS) -o $(DIR_OBJ)/$*.o -c $(DIR_SRC)/$*.c $(CMD_REDIRECT_LOGS); \
	then \
		mv -f $(DIR_DEP)/$*.tmp.d $(DIR_DEP)/$*.d && touch $@;\
		echo $(LOG) "Compilamos el archivo fuente ($(notdir $<)) en objeto en $(dir $@)" $(MSG_OK);\
		echo $(LOG) "Se modificó el archivo ($?)" $(MSG_OK);\
	else \
		echo $(LOG) "Ocurrio un error al compilar $(OBJ)" $(MSG_ERROR);\
		sleep 2 && exit 1;\
	fi
	@echo ""
# --------------------------------------------------------------------

compile: ctags-installed libcommons-installed $(DIRS) $(DIR_BIN)/$(BIN) ## Compilar módulo

test: libcspecs-installed compile
	$(AT)$(MAKE) --no-print-directory -C tests compile exec

clean-base: ## Remover ejecutables, objetos y dependencias
	@-$(RM) $(DIR_BIN)/*.out
	@-$(RM) $(DIR_BIN)/*.a
	@-$(RM) $(DIR_BIN)/*.so
	@-$(RM) $(DIR_OBJ)/*.o
	@-$(RM) $(DIR_LOGS)/*.txt
	@-$(RM) $(DIR_LOGS)/*.log
	@-$(RM) $(DIR_DEP)/*.d
	@-$(RM) $(DIR_DEP)/*.tmp.d

	@echo "Removiendo ejecutable, objetos y dependencias desde $(CURRENT_MODULE_NAME)" $(MSG_OK)
ifneq ($(CURRENT_MODULE_NAME),tests)
	$(AT)$(MAKE) --no-print-directory -C tests clean
endif

e exec: ## Ejecutar binario
	$(AT)-$(CURRENT_PATH)/$(DIR_BIN)/$(BIN)

memcheck: valgreen-installed ## Ejecutar Memcheck con la aplicación dentro del contenedor Docker
	$(AT)-valgrind $(MEMCHECK_FLAGS) --log-file="logs/memcheck.log" $(DIR_BIN)/$(BIN)
ifneq (, $(shell which valgreen))
	$(AT)-valgreen $(DIR_BIN)/$(BIN)
endif

debug: debugger-installed
	$(AT)-$(DEBUGGER) $(CURRENT_PATH)/$(DIR_BIN)/$(BIN)

$(DEP):
-include $(wildcard $(DEP))

w watch: ## Observar cambios en /src /include y compilar automáticamente
	$(info Observando cambios en la aplicación...)
	$(AT)while true; do $(MAKE) -q --no-print-directory || $(MAKE) --no-print-directory; sleep 1; done

h help: ## Muestra menú de ayuda
	@awk 'BEGIN {FS = ":.*##"; printf "\nGuía de Comandos:\n  make \033[36m\033[0m\n"} /^[$$()% a-zA-Z_-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

# necesario para evitar los warnings de GNU Make por override de targets
# de makefiles que incluyen a este makefile
# los warnings hubieran sido
# - warning: overriding recipe for target nombre,
# - warning: ignoring old recipe for target nombre
%: %-base
	@true

# necesario para evitar
# el warning hubiera sido "make: *** No rule to make target 'nombre'.  Stop."
%:
	@true

.PHONY: e exec clean-base compile h help memcheck tests
