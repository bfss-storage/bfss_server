APPNAME = bfss-pressure
PORJECT-DIR = $(shell pwd)
BIN = $(PORJECT-DIR)/bin
GOCMD = /usr/local/go/bin/go
GOBUILD = $(GOCMD) build
GOCLEAN = $(GOCMD) clean
GOTEST = $(GOCMD) test
GOGET = $(GOCMD) get
BINARY_UNIX = $(BIN)_unix
COPY = cp
PID = .pid
THRIFT-ROOT=../../

.PHONY: web

build: thrift
	rm -rf $(BIN)/$(APPNAME)
	$(GOBUILD) -o $(BIN)/$(APPNAME) -v
	$(COPY) -R conf/ $(BIN)/conf
	mkdir -p $(BIN)/webui/static
	$(COPY) -R web/ui/dist/* $(BIN)/webui/static/

deploy: build
	scp -rp bin/* root@10.0.1.181:/home/bfss/tests/web-pressure/

test:
	$(GOTEST) -count=1 -v ./...

bench:
	$(GOTEST) -benchmem -bench=. bfss/rpc

clean: kill
	$(GOCLEAN)
	rm -rf $(BIN)

dev: thrift
	$(GOBUILD) -o $(BIN)/$(APPNAME) -v
	$(BIN)/$(APPNAME) -server 10.0.1.119:30000 -enable-concurrent -routines 100 -cache-size 4G -fast -sample S100K -expected-writes 3G & echo $$! > $(BIN)/$(PID)

web:
	$(GOBUILD) -o $(BIN)/$(APPNAME) -v
	$(BIN)/$(APPNAME) -web -ipc -log-level debug

build-linux: thrift
	CGO_ENABLED=0 GOOS=linux GOARCH=amd64 $(GOBUILD) -o $(BINARY_UNIX) -v
	
run:
	$(BIN)/$(APPNAME) -sample S1M & echo $$! > $(BIN)/$(PID)
	
restart:
	@echo restart the app...
	@kill `cat $(BIN)/$(PID)` || true
	$(BIN)/$(APPNAME) -sample S1M & echo $$! > $(BIN)/$(PID)
	
kill:
	@kill `cat $(BIN)/$(PID)` || true

PHONY:
	@mkdir -p thrift

thrift: PHONY
	thrift -gen go:package_prefix=pressure/thrift/ -out $(PORJECT-DIR)/thrift -r $(THRIFT-ROOT)bfss_api/bfss_api.thrift