# Developer shortcuts for fontu. These wrap the CMake/CTest commands documented
# in README.md; CMake remains the real build system.
#
# Overridable variables, e.g. `make build BUILD_TYPE=Release BUILD_DIR=build-rel`:
#   BUILD_DIR   CMake binary directory               (default: build)
#   BUILD_TYPE  CMAKE_BUILD_TYPE for single-config    (default: Debug)
#   QT_PREFIX   Qt install prefix for CMAKE_PREFIX_PATH (default: `brew --prefix qt`, if Homebrew is present)
#   JOBS        Parallel build jobs                   (default: CPU count)
#   CMAKE_ARGS  Extra arguments for the configure step
#   FEATURE     verify-fontu recipe for `make verify` (default: drawing)
#   EVIDENCE    New evidence directory for `make verify`

BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
CMAKE      ?= cmake
CTEST      ?= ctest
PYTHON     ?= python3
CMAKE_ARGS ?=

UNAME_S := $(shell uname -s 2>/dev/null)

JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Homebrew Qt when available; empty otherwise so CMake uses its normal search
# (CMAKE_PREFIX_PATH env, Qt6_DIR, system packages, ...).
QT_PREFIX ?= $(shell command -v brew >/dev/null 2>&1 && brew --prefix qt 2>/dev/null)
ifneq ($(strip $(QT_PREFIX)),)
  QT_PREFIX_ARG := -DCMAKE_PREFIX_PATH="$(QT_PREFIX)"
  VERIFY_QT_ARG := --qt-prefix "$(QT_PREFIX)"
endif

ifeq ($(UNAME_S),Darwin)
  APP_EXE := $(BUILD_DIR)/fontu.app/Contents/MacOS/fontu
else
  APP_EXE := $(BUILD_DIR)/fontu
endif

VERIFY_HELPER := .agents/skills/verify-fontu/helpers/control-fontu
FEATURE  ?= drawing
ifeq ($(origin EVIDENCE),undefined)
  # Evaluated once so the echoed path matches the one passed to the helper.
  EVIDENCE := $(patsubst %/,%,$(or $(TMPDIR),/tmp))/fontu-proof-$(FEATURE)-$(shell date +%Y%m%d-%H%M%S)
endif

.DEFAULT_GOAL := all
.PHONY: all help configure build run test test-py clean rebuild verify

all: build ## Build the app and tests (default)

help: ## List available targets
	@echo "Usage: make [target] [VAR=value ...]"
	@echo
	@grep -E '^[a-zA-Z_-]+:.*## ' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*## "}; {printf "  %-10s %s\n", $$1, $$2}'
	@echo
	@echo "Variables: BUILD_DIR=$(BUILD_DIR) BUILD_TYPE=$(BUILD_TYPE) JOBS=$(JOBS)"
	@echo "           QT_PREFIX=$(if $(strip $(QT_PREFIX)),$(QT_PREFIX),<unset; CMake default search>)"
	@echo "           FEATURE=$(FEATURE) (verify recipe) CMAKE_ARGS=$(CMAKE_ARGS)"

configure: ## Configure the CMake build directory
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(QT_PREFIX_ARG) $(CMAKE_ARGS)

$(BUILD_DIR)/CMakeCache.txt:
	$(MAKE) configure

build: $(BUILD_DIR)/CMakeCache.txt ## Build everything in parallel (configures first if needed)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

run: build ## Build, then launch the development app in the foreground
	./$(APP_EXE)

test: build ## Build, then run all CTest suites with output on failure
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

test-py: ## Run the Python packaging tests (as in CI)
	$(PYTHON) -m unittest discover -s tests -p "test_*.py" -v

clean: ## Remove the build directory
	rm -rf $(BUILD_DIR)

rebuild: clean ## Clean, reconfigure, and build from scratch
	$(MAKE) build

verify: ## Run a verify-fontu recipe offscreen (FEATURE=drawing EVIDENCE=<new dir>)
	$(PYTHON) $(VERIFY_HELPER) $(FEATURE) --evidence "$(EVIDENCE)" $(VERIFY_QT_ARG)
	@echo "Evidence: $(EVIDENCE)"
