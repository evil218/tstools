# =============================================================================
# definition
# =============================================================================
CFLAGS = /W2 /nologo

!if "$(NAME)" == "libts1"
POSTFIX = .lib
!else
POSTFIX = .exe
!endif

OBJ_DIR = ..\..\release\windows
INSTALL_DIR = C:\WINDOWS\system32

# -------------------------------------------------------------------
# debug or release
# -------------------------------------------------------------------
BUILD_TYPE = release

!if "$(BUILD_TYPE)" == "release"
CFLAGS = $(CFLAGS) /O2
!else
CFLAGS = $(CFLAGS) /Zi
!endif

# -------------------------------------------------------------------
# others
# -------------------------------------------------------------------
CFLAGS = $(CFLAGS) /I. /I../include /DPLATFORM=MSVC

OBJS = $(SRCS:.c=.obj)
DEPS = $(SRCS:.c=.d)

# =============================================================================
# supported aim
# =============================================================================
all: $(NAME)$(POSTFIX)

$(NAME).lib: $(OBJS)
	LIB /NOLOGO /OUT:$(NAME).lib $(OBJS)

$(NAME).exe: $(OBJS) $(DEPS)
	$(CC) /nologo $(OBJS) ..\libts\libts1.lib

$(INSTALL_DIR)\$(NAME)$(POSTFIX): $(NAME)$(POSTFIX)
!if "$(NAME)" != "libts1"
	copy $** $@
!endif

$(OBJ_DIR)\$(NAME)$(POSTFIX): $(NAME)$(POSTFIX)
!if "$(NAME)" != "libts1"
	copy $** $@
!endif

clean:
	-del /F /Q $(OBJS) *~
	-del /F /Q $(NAME).exe $(NAME).lib

explain:
	@echo "    Source     files: $(SRCS)"
	@echo "    Object     files: $(OBJS)"
	@echo "    Dependency files: $(DEPS)"

#!include $(DEPS)

install: $(INSTALL_DIR)\$(NAME)$(POSTFIX)

release: $(OBJ_DIR)\$(NAME)$(POSTFIX)

uninstall:
!if "$(NAME)" != "libts1"
	-del /F /Q $(INSTALL_DIR)\$(NAME)$(POSTFIX)
!endif

# =============================================================================
# THE END
# =============================================================================
