SUBDEVICES:=$(shell find devices/* -type d ! -name CVS ! -name . ! -name .. ! -path \*/.svn\*)
INCLUDES += $(patsubst %,-I%,$(SUBDEVICES)) 
-include $(patsubst %,%/module.mk,$(SUBDEVICES))
