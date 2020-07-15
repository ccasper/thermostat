# Build the core library for unit testing.
cc_library(
	name = "core",
	hdrs = ["comparison.h",
	        "interfaces.h",
	 "settings.h","buttons.h", "print.h", "events.h","maintain_hvac.h","settings_storer.h","calculate_iaq.h","fan_controller.h","menus.h"],
	copts = ["-Ithermostat", "-I../testing"],
	visibility = ["//visibility:public"],
	)
