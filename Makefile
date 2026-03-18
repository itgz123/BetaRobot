# 删除cubemx的main.c
C_SOURCES := $(filter-out Core/Src/main.c,$(C_SOURCES))