PROJ_DIR = $(shell pwd)
BUILD_DIR = $(PROJ_DIR)/build

list:
	@echo "configure_dir : configure the build directory"
	@echo "build_ninja : configure the cmake and build with ninja file"
	@echo "build_make : configure the cmake and build with make files"
	@echo "install_ninja : install to local bin folder after building with ninja file"
	@echo "install_make : install to local bin folder after building with make file"

configure_dir:
	@rm -rf $(BUILD_DIR)
	@mkdir $(BUILD_DIR)

build_ninja: configure_dir
	cmake -G "Ninja" -B $(BUILD_DIR) .
	cd build && ninja

build_make: configure_dir
	cmake -B $(BUILD_DIR) .
	cd build && make

install_ninja: build_ninja
	mkdir -p ~/.local/bin
	cp $(BUILD_DIR)/liquid ~/.local/bin/


install_make: build_make
	mkdir -p ~/.local/bin
	cp $(BUILD_DIR)/liquid ~/.local/bin/

uninstall:
	rm ~/.local/bin/liquid
