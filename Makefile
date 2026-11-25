# Test della logica del protocollo sul PC (nessuna scheda necessaria).
#   make test
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -Ilib/voicecore -Itests

CORE := $(wildcard lib/voicecore/*.cpp)
HDRS := $(wildcard lib/voicecore/*.h tests/*.h tools/*.h)
TESTS := packet stream node button audio output pins battery settings link level tone decode amp profile menu position morse nmea live repeater repeater_link message_store inbox text_picker rx_monitor meshcom_frame text_template choice_picker tx_scheduler sha256 sysop radio_test node_link node_core reflector_core peer_resolve repeater_uplink reflector_config quality_report node_info aprs_gate aprs_client http_mini admin_auth nvs repeater_config repeater_force qrz listeners listen_config wifi_retry node_info_cache gps_probe

.PHONY: test test-page test-py lcr_decode lcr_live codec_compare pdf clean vlnode vlreflector reflector-smoke
test: $(addprefix build/test_,$(TESTS))
	@for t in $^; do ./$$t || exit 1; done

# Impostazioni salvate nella scheda: src/settings_nvs.cpp con una memoria NVS finta (tests/nvs).
build/test_nvs: tests/nvs/test_nvs.cpp src/settings_nvs.cpp tests/nvs/Preferences.h tests/nvs/Arduino.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Itests/nvs -Isrc -o $@ tests/nvs/test_nvs.cpp src/settings_nvs.cpp $(CORE)

# Collegamento con il server APRS: socket veri su 127.0.0.1 con un server finto e un thread (tools/aprs_client.h).
build/test_aprs_client: tests/test_aprs_client.cpp tools/aprs_client.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread -Itools -o $@ $< $(CORE)

build/test_%: tests/test_%.cpp $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $< $(CORE)

clean:
	rm -rf build

# Nodo VoLoRa-IP per il PC (tools/vlnode.cpp): prova il protocollo fra nodi su un vero socket UDP, o fa da secondo nodo per una T3.
vlnode: build/vlnode
build/vlnode: tools/vlnode.cpp $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O1 -Ilib/voicecore -o $@ $< $(CORE)

# Reflector VoLoRa per un server (tools/vlreflector.cpp, docs/studio-reflector.md).
vlreflector: build/vlreflector
# HTTPS della pagina pubblica (tools/tls_server.h): serve OpenSSL con le intestazioni (sudo apt install libssl-dev pkg-config); senza, il reflector si compila
# lo stesso e la pagina pubblica resta in HTTP (vlreflector --tls-check dice se il TLS c'e').
TLS_FLAGS := $(shell pkg-config --exists openssl 2>/dev/null && echo "-DVL_TLS $$(pkg-config --cflags --libs openssl)")
# Un file che cambia solo quando cambiano le opzioni di OpenSSL: cosi' il reflector si ricompila da solo dopo "apt install libssl-dev".
.PHONY: tlsflags-stamp
build/tlsflags: tlsflags-stamp
	@mkdir -p build
	@echo '$(TLS_FLAGS)' | cmp -s - $@ || echo '$(TLS_FLAGS)' > $@
# Il Codec2 serve al reflector per l'ascolto dal web (tools/listen_audio.h): si compila una volta in build/libcodec2.a (solo un compilatore C).
build/vlreflector: tools/vlreflector.cpp $(CORE) $(HDRS) build/tlsflags build/libcodec2.a
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O1 -pthread -Ilib/voicecore -Itools -Ithird_party/esp32_codec2-1.0.7/src -o $@ $< $(CORE) $(TLS_FLAGS) build/libcodec2.a -lm

reflector-smoke: build/vlreflector build/vlnode
	tests/reflector_smoke.sh
	tests/listen_smoke.sh
	tests/nodecache_smoke.sh

# Rigenera docs/formato-pacchetto.pdf dal codice (serve reportlab: pip install reportlab).
pdf:
	python3 tools/make_formato_pacchetto_pdf.py

# Prova della pagina web con un dispositivo seriale finto (serve Node e il modulo playwright con Chromium).
test-page:
	node tests/page/run.js
	node tests/page/run_wifi.js

# Decodificatore offline dei pacchetti catturati con un SDR (tools/lcr_decode.cpp): build/lcr_decode
lcr_decode: build/lcr_decode

# Confronto dei modi del Codec2 sulla stessa registrazione (tools/codec_compare.c): build/codec_compare. Usa gli stessi
# sorgenti del Codec2 che gira sulle schede (third_party/esp32_codec2-1.0.7); serve solo un compilatore C.
CODEC2_DIR := third_party/esp32_codec2-1.0.7/src
CODEC2_SRC := codec2.c codec2_fft.c kiss_fft.c kiss_fftr.c lpc.c lsp.c nlp.c pack.c phase.c postfilter.c quantise.c sine.c \
  interp.c mbest.c newamp1.c newamp2.c filter.c codebook.c codebookd.c codebookge.c codebookjmv.c codebookjvm.c \
  codebooklspmelvq.c codebookmel.c codebooknewamp1.c codebooknewamp1_energy.c codebooknewamp2.c codebooknewamp2_energy.c \
  dump.c mpdecode_core.c phi0.c golay23.c
codec_compare: build/codec_compare

CODEC2_OBJS := $(patsubst %.c,build/codec2/%.o,$(CODEC2_SRC))
build/codec2/%.o: $(CODEC2_DIR)/%.c
	@mkdir -p build/codec2
	$(CC) -O2 -w -I$(CODEC2_DIR) -c -o $@ $<
build/libcodec2.a: $(CODEC2_OBJS)
	ar rcs $@ $^

build/codec_compare: tools/codec_compare.c $(addprefix $(CODEC2_DIR)/,$(CODEC2_SRC))
	@mkdir -p build
	$(CC) -O2 -w -I$(CODEC2_DIR) -o $@ $^ -lm

# Ricezione in tempo reale (tools/lcr_live.cpp): build/lcr_live
lcr_live: build/lcr_live

build/lcr_live: tools/lcr_live.cpp tools/lcr_live_core.h tools/lcr_decode_core.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O2 -Ilib/voicecore -Itools -o $@ $< $(CORE)

build/lcr_decode: tools/lcr_decode.cpp tools/lcr_decode_core.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O2 -Ilib/voicecore -o $@ $< $(CORE)

# Prova della parte Python del ricevitore SDR (non serve GNU Radio).
test-py:
	python3 tests/sdr/test_assembler.py
	python3 tests/sdr/test_morse.py
	python3 tests/sdr/test_deviazione.py
	python3 tests/site/test_versions.py
	$(MAKE) --no-print-directory codec_compare
	python3 tests/codec/test_compare.py
	python3 -m py_compile tools/sdr/lora_rx.py tools/sdr/analizza_morse.py tools/sdr/deviazione_fm.py
