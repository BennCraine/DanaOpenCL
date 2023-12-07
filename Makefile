CC = gcc

API_PATH = "./resources-ext/dana_api_1.7"
STD_INCLUDE = -I $(API_PATH)
CCFLAGS = -Wall -fno-strict-aliasing
OUTPUT_FILE=

INSTALL_PATH=
CP_CMD=
CHIP=
PLATFORM=
ALL_RULES = calendar cmdln iofile iotcp ioudp dns sysinfo timer run math mysql_lib uiplane png jpg zlib clipboard ssl_lib sha_lib cipher_lib x509_lib audio rwlock semaphore clockhd

ifeq ($(OS),Windows_NT)
    CCFLAGS += -DWINDOWS
	CCFLAGS += -DLIB_PLATFORM_NAME=\"win\"
	CCFLAGS += -DMACHINE_ENDIAN_LITTLE
	INSTALL_PATH = "c:\programfiles\dana\"
	CP_CMD = copy
	PLATFORM = win
	CCFLAGS += -shared
	NET_LIBS = wepoll/wepoll.c -lws2_32
	MYSQL_CONCPP_DIR= "C:/libs/MySQL Connector C 6.1"
	MYSQL_INCLUDE = -I $(MYSQL_CONCPP_DIR)/include -L $(MYSQL_CONCPP_DIR)/lib
	MYSQL_LIBS = -lmysql
	PNG_INCLUDE = -I "C:/libs/lpng/"
	PNG_LIBS = "C:/libs/lpng/libpng.a" -L"C:/ProgramFiles/Dana/" $(ZLIB_LIBS)
	JPG_INCLUDE = -I "C:/libs/jpeg-9c"
	JPG_LIBS = "C:/libs/jpeg-9c/libjpeg.a"
	ZLIB_INCLUDE = -I "C:/libs/zlib"
	ZLIB_LIBS = "C:/libs/zlib/libz.a"
	SSL_INCLUDE = -I C:/libs/openssl-3.1.1/include
	SSL_LIBS = C:/libs/openssl-3.1.1/libssl.a C:/libs/openssl-3.1.1/libcrypto.a -lws2_32 -lgdi32 -lADVAPI32 -luser32
	AUDIO_INCLUDE = -I "C:/libs/miniaudio"
	AUDIO_LIBS = -lm -lpthread
    ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
        CCFLAGS += -DMACHINE_64
		CCFLAGS += -DLIB_CHIP_NAME=\"x64\"
		CHIP = x64
		SDL_INCLUDE = -I "C:/libs/SDL/SDL-release-2.26.4/include" -I C:/libs/SDL/SDL2_ttf-2.20.2 -I C:/libs/SDL2_gfx -I C:/msys64/mingw64/include
		SDL_LIBS = C:/msys64/mingw64/lib/libSDL2main.a C:/msys64/mingw64/lib/libSDL2.a C:/msys64/mingw64/lib/libSDL2_ttf.a C:/libs/SDL2_gfx/.libs/libSDL2_gfx.a -lmingw32 -lImm32 -lVersion -lwinmm -lgdi32 -lADVAPI32 -luser32 -lole32 -loleaut32 -lshell32 -lsetupapi -lrpcrt4 -mwindows -static-libgcc"
    endif
    ifeq ($(PROCESSOR_ARCHITECTURE),x86)
        CCFLAGS += -DMACHINE_32
		CCFLAGS += -DLIB_CHIP_NAME=\"x86\"
		CHIP = x86
		SDL_INCLUDE = -I "C:/libs/SDL/SDL-release-2.26.4/include" -I C:/libs/SDL/SDL2_ttf-2.20.2 -I C:/libs/SDL2_gfx
		SDL_LIBS = C:/msys32/mingw32/lib/libSDL2main.a C:/msys32/mingw32/lib/libSDL2.a C:/msys32/mingw32/lib/libSDL2_ttf.a C:/libs/SDL2_gfx/.libs/libSDL2_gfx.a -lmingw32 -lImm32 -lVersion -lwinmm -lADVAPI32 -luser32 -lole32 -loleaut32 -lshell32 -lsetupapi -lrpcrt4 -lUsp10 -lgdi32 -mwindows -static-libgcc"
    endif
else
    UNAME_S := $(shell uname -s)
	CCFLAGS += -ldl
	CCFLAGS += -DMACHINE_ENDIAN_LITTLE
	INSTALL_PATH = ~/dana/
	CP_CMD = cp
	CCFLAGS += -shared -fPIC
	MATH_LIBS = -lm
	SSL_INCLUDE = -I ~/libs/openssl-3.1.1/include
	SSL_LIBS = ~/libs/openssl-3.1.1/libssl.a ~/libs/openssl-3.1.1/libcrypto.a
	MYSQL_INCLUDE = -I ~/libs/mysql_lib/include
	MYSQL_LIBS = ~/libs/mysql_lib/libmysqlclient.a -lpthread -lz -lm -lrt -ldl -lstdc++ $(SSL_LIBS)
	AUDIO_INCLUDE = -I ~/libs/miniaudio/
	AUDIO_LIBS = -lm -lpthread
	ZLIB_LIBS = ~/libs/zlib_lib/libz.a
	JPG_INCLUDE = -I ~/libs/jpeg_lib/include
	JPG_LIBS = ~/libs/jpeg_lib/libjpeg.a
	PNG_INCLUDE = -I ~/libs/png_lib/include/libpng16
	PNG_LIBS = ~/libs/png_lib/libpng.a -lz -lm
    ifeq ($(UNAME_S),Linux)
        CCFLAGS += -DLINUX
		CCFLAGS += -DLIB_PLATFORM_NAME=\"deb\"
		PLATFORM = deb
		CLIPBOARD_LIBS = -lX11
		SDL_LIBS = /usr/local/lib/libSDL2main.a /usr/local/lib/libSDL2.a /usr/local/lib/libSDL2_ttf.a ~/libs/SDL2_gfx/.libs/libSDL2_gfx.a -lm -lfreetype
		SDL_INCLUDE = -I ~/libs/SDL2_gfx -I /usr/local/include/SDL2/
    endif
    ifeq ($(UNAME_S),Darwin)
        CCFLAGS += -DOSX
        CCFLAGS += -DLINUX
        PLATFORM = osx
		CCFLAGS += -DLIB_PLATFORM_NAME=\"osx\"
        CCFLAGS += -DMACHINE_64
		CCFLAGS += -DLIB_CHIP_NAME=\"x64\"
		CHIP = x64
		SDL_LIBS = /usr/local/lib/libSDL2.a -liconv -framework Cocoa -framework Carbon -framework IOKit -framework CoreAudio -framework CoreVideo -framework AudioToolbox -framework ForceFeedback -framework CoreHaptics -framework GameController -framework Metal /usr/local/lib/libSDL2_ttf.a ~/libs/SDL2_gfx/.libs/libSDL2_gfx.a -lfreetype -Wl,-rpath,'@executable_path/resources-ext'
		SDL_INCLUDE = -I ~/libs/ -I ~/libs/SDL2_gfx -I ~/libs/SDL2/ -I ~/libs/SDL2/include
		MYSQL_INCLUDE = -I /usr/local/mysql-8.0.12-macos10.13-x86_64/include/
		MYSQL_LIBS = /usr/local/lib/libcrypto.a /usr/local/lib/libssl.a /usr/local/mysql/lib/libmysqlclient.a -lpthread -lz -lm -ldl -lstdc++
		CLIPBOARD_LIBS = -framework ApplicationServices -x objective-c -ObjC -std=c99
    endif
    ifneq ($(UNAME_S),Darwin)
        UNAME_P := $(shell uname -p)
		ifeq ($(UNAME_P), unknown)
            CCFLAGS += -DMACHINE_64
            CCFLAGS += -DLIB_CHIP_NAME=\"x64\"
            CHIP = x64
		endif
        ifeq ($(UNAME_P),x86_64)
            CCFLAGS += -DMACHINE_64
            CCFLAGS += -DLIB_CHIP_NAME=\"x64\"
            CHIP = x64
        endif
        ifneq ($(filter %86,$(UNAME_P)),)
            CCFLAGS += -DMACHINE_32
            CCFLAGS += -DLIB_CHIP_NAME=\"x86\"
            CHIP = x86
        endif
        ifneq ($(filter arm%,$(UNAME_P)),)
            CCFLAGS += -DARM
        endif
        UNAME_N := $(shell uname -n)
        ifneq ($(filter %raspberrypi,$(UNAME_N)),)
            CCFLAGS += -DMACHINE_32
            CCFLAGS += -DLIB_CHIP_NAME=\"armv6\"
            CHIP = armv6
			SDL_LIBS = /usr/local/lib/libSDL2main.a /usr/local/lib/libSDL2.a /usr/local/lib/libSDL2_ttf.a ~/libs/SDL2_gfx/.libs/libSDL2_gfx.a -lm -lfreetype -L/opt/vc/lib -lbcm_host
        endif
    endif
endif

opencl:
	dnc ./gpu/Compute.dn
	dnc ./gpu/Compute.dn -gni
	$(CP_CMD) ./OpenCLLib_dni.c ./resources-ext/
	rm ./OpenCLLib_dni.c
	dnc ./gpu/ComputeManager.dn
	dnc ./gpu/ComputeDistributionManager.dn
	dnc ./gpu/IntelDefaultDistManager.dn
	dnc ./gpu/LogicalComputeDevice.dn
	dnc ./nn/NeuralNet.dn
	dnc ./dataprocessing/TypeConversion.dn
	dnc ./dataprocessing/Normalisation.dn
	dnc ./dataprocessing/Resizing.dn
	dnc ./linear/LinearOperations.dn
	$(CC) -O -s ./resources-ext/OpenCLLib_dni.c $(API_PATH)/vmi_util.c ./resources-ext/OpenCLLib.c -o OpenCLLib[$(PLATFORM).$(CHIP)].dnl -lOpenCL $(STD_INCLUDE) $(CCFLAGS)
	$(CP_CMD) OpenCLLib[$(PLATFORM).$(CHIP)].dnl "./resources-ext/"
	rm OpenCLLib[$(PLATFORM).$(CHIP)].dnl

all: $(ALL_RULES)
