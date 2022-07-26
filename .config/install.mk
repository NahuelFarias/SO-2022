# TODO: pendiente definir
deploy-dev: popup-confirm-action
#	$(info Haciendo deploy)

deploy-prod: popup-confirm-action ## Deploy a servidor remoto
	$(info Subiendo proyecto a servidor remoto...)
	@rsync -a --progress --partial --rsh=ssh . $(SSH_USER)@$(SSH_IP):$(SSH_PATH_DEST)

popup-confirm-action:
	@.config/popup-confirm-action.sh

copy-project:
ifeq ($(USER_UTNSO_IS_REQUIRED), false)
	@sudo rsync -rvz . $(DIR_BASE)
	@sudo chown -R utnso:utnso $(DIR_BASE)
	@sudo chmod -R ug+rwx $(DIR_BASE)
endif

install-dev-utils:
	$(info Instalando utilidades de desarrollo...)
	@-sudo apt install -y gcc gdb libcunit1 g++ libcunit1-dev \
  libncurses5 tig autotools-dev libfuse-dev libreadline6-dev \
	build-essential valgrind
	@-sudo apt install -y nemiver rsync screen
	@-sudo apt install -y clang-format
	@-sudo apt install -y \
    pkg-config autoconf automake lnav \
    python3-pip python3-docutils libseccomp-dev libjansson-dev libyaml-dev libxml2-dev
	@-sudo apt install -y universal-ctags
	@-sudo apt update && pip3 install valgreen

install-virtualbox:
ifeq ($(VBOX_IS_REQUIRED), true)
ifeq ($(VBOX_LATEST), true)
# Adding VirtualBox Package Repository:
	@echo "deb [arch=amd64] http://download.virtualbox.org/virtualbox/debian bionic contrib" | sudo tee /etc/apt/sources.list.d/virtualbox.list
# Adding VirtualBox Public PGP Key:
	@wget -q https://www.virtualbox.org/download/oracle_vbox_2016.asc -O- | sudo apt-key add -
	@sudo apt update && sudo apt install -y virtualbox virtualbox-ext-pack
else
	@cd /tmp && \
	wget https://download.virtualbox.org/virtualbox/6.0.24/virtualbox-6.0_6.0.24-139119~Ubuntu~bionic_amd64.deb && \
	sudo dpkg -i virtualbox-6.0_6.0.24-139119~Ubuntu~bionic_amd64.deb && \
	sudo rm -vf virtualbox-6.0_6.0.24-139119~Ubuntu~bionic_amd64.deb
endif
endif

add-user:
ifeq ($(USER_UTNSO_IS_REQUIRED), true)
	$(info Configurando usuario utnso...)
# creamos el usuario, le asignamos una shell, un directorio y lo agregamos al grupo de sudo
	@sudo useradd -s /bin/bash -d $(DIR_BASE) -m -G sudo utnso
# le asignamos contraseña
	@sudo passwd utnso
# nos logeamos con ese usuario
	@su utnso && cd (DIR_BASE)
endif

# TODO: es necesario el sudo en make install, porque la implementacion del makefile de Cpsec no lo agrego
# TODO: validar nuevamente si en el contenedor de docker debe tener sudo..
# TODO: integrar validacion de shared library instalada con ldconfig (interviene el LD_PATH)
install-lib-cspec:
# validamos si existe la ruta, caso contrario arrojara error y el makefile fallara..
ifeq (, $(wildcard $(DIR_LIBS)/cspec))
	$(info Instalando cspec library...)
	@cd $(DIR_LIBS) && $(RM) cspec && git clone http://github.com/mumuki/cspec
	@sudo $(MAKE) -C $(DIR_LIBS)/cspec clean all install
endif

# TODO: validar nuevamente si en el contenedor de docker debe tener sudo..
# TODO: integrar validacion de shared library instalada con ldconfig (interviene el LD_PATH)
install-lib-commons:
# validamos si existe la ruta, caso contrario arrojara error y el makefile fallara..
ifeq (, $(wildcard $(DIR_LIBS)/so-commons-library))
	$(info Instalando so-commons...)
	@cd $(DIR_LIBS) && $(RM) so-commons-library && git clone http://github.com/sisoputnfrba/so-commons-library
	@$(MAKE) -C $(DIR_LIBS)/so-commons-library clean all test install
endif

# TODO: validar si en el contenedor de docker debe tener sudo..
install-ctags:
ifeq (, $(shell which universal-ctags))
	$(info Instalando ctags...)

	@cd /tmp && $(RM) ctags && git clone https://github.com/universal-ctags/ctags.git && \
	cd ctags && ./autogen.sh && ./configure && make && sudo make install
endif

.PHONY: install-virtualbox install-dev-utils install-ctags install-lib-cspec install-lib-commons add-user copy-project deploy-dev deploy-prod
