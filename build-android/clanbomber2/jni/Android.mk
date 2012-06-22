include ../directfb-env.mk

CLANBOMBER_INCLUDES = \
	-I$(DFB_INCLUDE_PATH)/../ClanBomber2/clanbomber \
	-I$(DFB_INCLUDE_PATH)/../FusionSound/include/

CLANBOMBER_SOURCES = \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_Keyboard.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Observer.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Arrow.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_None.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Bomber.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/cl_vector.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Ice.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Debug.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Joint.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_PutBomb.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Koks.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapEntry.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Corpse_Part.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_Stoned.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Thread.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Chat.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_Joystick.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/GameStatus_Team.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Menu.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_Frozen.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/ClientSetup.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/ClanBomber.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Trap.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Resources.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Viagra.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Explosion.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/GameStatus.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Box.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/ServerSetup.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Credits.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_RCMouse.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Timer.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/PlayerSetup.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_AI.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/GameObject.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Bomb.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Config.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Wall.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Map.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Glove.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Skateboard.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Bomber_Corpse.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Event.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapEditor.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Kick.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_AI_mass.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Mutex.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_Fast.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Bomb.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Power.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Server.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Client.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapSelector.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Ground.cpp



INCLUDES += $(CLANBOMBER_INCLUDES)

DIRECTFB_APP_SOURCES := $(CLANBOMBER_SOURCES)

include ../directfb.mk
