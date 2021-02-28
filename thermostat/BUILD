# Build the core library for unit testing.
cc_library(
	name = "core",
	hdrs = ["comparison.h",
	        "interfaces.h",
	        "settings.h",
          "buttons.h",
          "print.h",
          "events.h",
          "calculate_iaq.h",
          "thermostat_tasks.h",
          "menus.h"
  ],
	copts = ["-Ithermostat", "-I../testing"],
	visibility = ["//visibility:public"],
  )
