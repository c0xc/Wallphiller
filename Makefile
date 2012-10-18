PROGRAM=Wallphiller

SRCDIR=src
INCDIR=inc
OBJDIR=obj
BINDIR=bin

CC_OPT_O=-o 
LD_OPT_O=$(CC_OPT_O)

VERSIONFILE0=$(INCDIR)/.0.version.hpp
VERSIONFILE=$(INCDIR)/version.hpp

EXECUTABLE=$(BINDIR)/$(PROGRAM)

PLATFORM=g++
include Makefile_$(PLATFORM)_inc

CFLAGS+="-DPROGRAM=\"$(PROGRAM)\""

CFLAGS_YESQT=-DENABLE_QT
CFLAGS_NOQT=-DDISABLE_QT

CFLAGS+=$(CFLAGS_QT)
CFLAGS+=$(ADDCFLAGS)

#LDFLAGS+=$(LDFLAGS_BOOST)
LDFLAGS+=$(LDFLAGS_READLINE)
LDFLAGS+=$(ADDLDFLAGS)

MODULES=main wallphiller thumbnailbox
SOURCES=$(MODULES:=.cpp)
SOURCES_QT=$(MODULES:=.moc.cpp)
OBJECTS=$(SOURCES:%.cpp=$(OBJDIR)/%.obj)
OBJECTS_QT=$(SOURCES:%.cpp=$(OBJDIR)/%.moc.obj)

CMD_LINK_OLD=$(LD) $(OBJECTS) $(OBJECTS_QT) $(LDFLAGS_QT) $(LDFLAGS) $(LD_OPT_O)$(EXECUTABLE)
CMD_LINK=$(strip $(CMD_LINK_OLD))

rebuild: clean compile link clean-moc

build: compile link

rebuild-noqt: clean compile-noqt link-noqt

build-noqt: compile-noqt link-noqt

compile: CFLAGS_SPECIAL=$(CFLAGS_YESQT)
compile: $(OBJDIR) $(OBJECTS) $(OBJECTS_QT) $(VERSIONFILE)

compile-noqt: CFLAGS_SPECIAL=$(CFLAGS_NOQT)
compile-noqt: $(OBJDIR) $(OBJECTS)

$(OBJDIR):
	mkdir $@

$(BINDIR):
	mkdir $@

$(OBJDIR)/%.obj: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) $(CFLAGS_SPECIAL) $< $(CC_OPT_O)$@

$(SRCDIR)/%.moc.cpp: $(INCDIR)/%.hpp
	$(MOC) $< -o $@

$(INCDIR)/%.hpp.pre: $(INCDIR)/%.hpp
	$(CC) $(CFLAGS) -E $< > $@

#link: $(BINDIR) $(EXECUTABLE)

$(EXECUTABLE): link

link: $(BINDIR) $(OBJECTS) $(OBJECTS_QT)
	$(LD) $(OBJECTS) $(OBJECTS_QT) $(LDFLAGS) $(LDFLAGS_QT) $(LD_OPT_O)$(EXECUTABLE)

link-noqt: $(BINDIR) $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) $(CC_OPT_O)$(EXECUTABLE)

link-static: $(OBJECTS) $(OBJECTS_QT)
	$(LD) $(OBJECTS) $(OBJECTS_QT) $(LDFLAGS) $(LDFLAGS_QT) $(LDFLAGS_QT_STATIC) $(CC_OPT_O)$(EXECUTABLE)

clean:
ifeq ($(CMD_CLEAN),)
	rm -f $(OBJDIR)/*.o $(OBJDIR)/*.obj $(SRCDIR)/*.moc.cpp
else
	$(CMD_CLEAN)
endif

clean-moc:
ifeq ($(CMD_CLEAN_MOC),)
	rm -f $(SRCDIR)/*.moc.cpp
else
	$(CMD_CLEAN_MOC)
endif

list-sources:
	@echo $(SOURCES)

list-objects:
	@echo $(OBJECTS)

list-objects-qt:
	@echo $(OBJECTS_QT)

show-executable:
	@echo $(EXECUTABLE)

show-qtdir:
	@echo $(QTDIR)

