PROGRAM=Wallphiller

SRCDIR=src
INCDIR=inc
OBJDIR=obj
BINDIR=bin
SUBDIR=sub

CC_OPT_O=-o 
LD_OPT_O=$(CC_OPT_O)

VERSIONFILE0=$(INCDIR)/.0.version.hpp
VERSIONFILE=$(INCDIR)/version.hpp

EXECUTABLE=$(BINDIR)/$(PROGRAM)

PLATFORM=g++
include Makefile_$(PLATFORM)_inc

CFLAGS+="-DPROGRAM=\"$(PROGRAM)\""

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

build-static: compile-static link-static

compile: $(OBJDIR) $(OBJECTS) $(OBJECTS_QT) $(VERSIONFILE)

$(OBJDIR):
	mkdir $@

$(BINDIR):
	mkdir $@

$(INCDIR)/thumbnailbox.hpp: $(SUBDIR)/thumbnailbox/$(INCDIR)/thumbnailbox.hpp
	cp $< $@

$(SRCDIR)/thumbnailbox.cpp: $(SUBDIR)/thumbnailbox/$(SRCDIR)/thumbnailbox.cpp
	cp $< $@

$(OBJDIR)/%.obj: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) $< $(CC_OPT_O)$@

$(SRCDIR)/%.moc.cpp: $(INCDIR)/%.hpp
	$(MOC) $< -o $@

$(INCDIR)/%.hpp.pre: $(INCDIR)/%.hpp
	$(CC) $(CFLAGS) -E $< > $@

#link: $(BINDIR) $(EXECUTABLE)

$(EXECUTABLE): link

link: $(BINDIR) $(OBJECTS) $(OBJECTS_QT)
	$(LD) $(OBJECTS) $(OBJECTS_QT) $(LDFLAGS) $(LDFLAGS_QT) $(LD_OPT_O)$(EXECUTABLE)

link-static: $(OBJECTS) $(OBJECTS_QT)
	$(LD) $(OBJECTS) $(OBJECTS_QT) $(LDFLAGS) $(LDFLAGS_QT_STATIC) $(CC_OPT_O)$(EXECUTABLE)

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

show-cflags:
	@echo $(CFLAGS)

show-qtdir:
	@echo $(QTDIR)

