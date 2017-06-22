#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := vhcibridge

include $(IDF_PATH)/make/project.mk

ctags:
	ctags -R ../esp-idf
	ctags -a -R
